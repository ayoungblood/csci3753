/* multi-lookup.c
 * Akira Youngblood, 2017-03-15
 * Main code for multi-threaded DNS resolution engine
 * Based on `pthread_hello.c` from PA3 assignment files
 * All error and debug messages are sent to stderr
 *
 * Uses queue.c/.h and util.c/.h from the PA3 files, unmodified
 */

#include "multi-lookup.h"

// Ratio of threads per core
const int threadsPerCore = 4;

// Global queue
const int queueSize = 32; // Size doesn't seem to change much in benchmarking
queue q;
// Global output file
FILE* outputfp = NULL;
// Global locks
pthread_mutex_t queue_lock, output_lock;

int main(int argc, char *argv[]) {
    int i, rv;
    clock_t tic = clock();
    // Parse command-line arguments
    if (argc <= 2) {
        // Need at least two file names (in and out), warn and print usage
        fprintf(stderr,"Not enough arguments provided.\n");
        fprintf(stderr,"Usage:\n"
                       "  resolve infile [infile2 ...] outfile\n");
        return EXIT_FAILURE;
    }
    // We have at least one input file and an output file
    // Open the output file
    outputfp = fopen(argv[argc-1],"w");
    if (!outputfp) {
        fprintf(stderr,"Unable to open output file, exiting.\n");
        return EXIT_FAILURE;
    }
    // Initialize the queue
    if (queue_init(&q,queueSize) == QUEUE_FAILURE){
        fprintf(stderr,"Error: queue_init failed!\n");
        return EXIT_FAILURE;
    }
    // Initialize the locks
    if (pthread_mutex_init(&queue_lock,NULL) || pthread_mutex_init(&output_lock,NULL)) {
        fprintf(stderr,"Error: pthread_mutex_init failed!\n");
        return EXIT_FAILURE;
    }
    // Create a requester thread pool based on number of input files
    // Some may be invalid, but that is handled by the threads
    const int NUM_THREADS_RQR = argc-2;
    pthread_t threads_rqr[NUM_THREADS_RQR];
    for (i = 0; i < NUM_THREADS_RQR; ++i) {
        // Create the thread and make sure it was created, pass the filename
        rv = pthread_create(&(threads_rqr[i]), NULL, RequesterThreadAction, argv[i+1]);
        if (rv) {
            fprintf(stderr,"Error: failed to create requester thread %d, rv = %d\n", i, rv);
            exit(EXIT_FAILURE);
        }
    }
    // Create a resolver thread pool based on number of cores
    // This is not entirely portable, but hopefully "portable enough"
    const int NUM_CORES = sysconf(_SC_NPROCESSORS_ONLN);
    const int NUM_THREADS_RLV = threadsPerCore*NUM_CORES;
    pthread_t threads_rlv[NUM_THREADS_RLV];
    fprintf(stderr, "Detected %d cores, using %d threads\n",NUM_CORES,NUM_THREADS_RLV);
    for (i = 0; i < NUM_THREADS_RLV; ++i) {
        // Create the thread and make sure it was created, pass the file pointer
        rv = pthread_create(&(threads_rlv[i]),NULL, ResolverThreadAction, outputfp);
        if (rv) {
            fprintf(stderr,"Error: failed to create resolver %d, rv = %d\n", i, rv);
            exit(EXIT_FAILURE);
        }
    }

    // Wait for requester threads to finish
    for (i = 0; i < NUM_THREADS_RQR; ++i) {
        pthread_join(threads_rqr[i],NULL);
    }
    // Wait for resolver threads to finish
    for (i = 0; i < NUM_THREADS_RLV; ++i) {
        pthread_join(threads_rlv[i],NULL);
    }
    // We are done, close the output file and clean up
    fclose(outputfp);
    pthread_mutex_destroy(&queue_lock);
    pthread_mutex_destroy(&output_lock);
    queue_cleanup(&q);
    // Print benchmarking info
    clock_t toc = clock();
    printf("Elapsed: %f s (%d resolver threads, queue size: %d)\n", (double)(toc - tic)/CLOCKS_PER_SEC, NUM_THREADS_RLV, queueSize);
    return 0;
}

// Run by each resolver thread.
// Pulls from queue and writes to output file, exits when queue is empty
void* ResolverThreadAction(void* fp) {
    char hostname[1025];
    char* temp;
    // Get hostnames from the queue and resolve them
    pthread_mutex_lock(&queue_lock);
    while (!queue_is_empty(&q)) {
        // Get an element from the queue
        if ((temp = queue_pop(&q)) == NULL) {
            fprintf(stderr,"Failed to pop from queue. Thread halting.\n");
            return NULL;
        }
        pthread_mutex_unlock(&queue_lock); // end of queue critical section
        // Copy to local stack space and free
        strcpy(hostname,temp);
        free(temp);
        // Resolve the hostname
        char firstipstr[INET6_ADDRSTRLEN];
        if (dnslookup(hostname, firstipstr, sizeof(firstipstr)) == UTIL_FAILURE) {
            fprintf(stderr, "dnslookup error: %s\n", hostname);
            strncpy(firstipstr, "", sizeof(firstipstr));
        }
        // Lock the output file, add line to output file, and release
        pthread_mutex_lock(&output_lock);
        fprintf(fp, "%s,%s\n", hostname, firstipstr);
        pthread_mutex_unlock(&output_lock);

        pthread_mutex_lock(&queue_lock);
    }
    pthread_mutex_unlock(&queue_lock);
    return NULL;
}

// Run by each requester thread.
// Opens file, adds hostnames to queue, and exits
void* RequesterThreadAction(void* file_name) {
    // Try to open the file
    FILE* fp = fopen((char*)file_name,"r");
    if (!fp) {
        fprintf(stderr,"Failed to open input file %s\n",(char*)file_name);
        return NULL; // exit quietly
    }
    // File opened succesfully, read lines and add to queue
    char hostname[1025];
    while (fscanf(fp, "%1024s", hostname) > 0) {
        pthread_mutex_lock(&queue_lock);
        // Spin on full queue, sleeping for random time between 0-100 us
        while (queue_is_full(&q)) {
            pthread_mutex_unlock(&queue_lock);
            usleep(rand()%100);
            pthread_mutex_lock(&queue_lock);
        }
        // Put the hostname on the heap so we can queue it.
        // malloc'ed memory is freed by resolver threads
        char* temp = (char*)malloc(strlen(hostname)+1);
        if (temp == NULL) {
            fprintf(stderr,"Out of memory. Thread halting.\n");
            return NULL; // exit quietly
        }
        strcpy(temp, hostname);
        // Add to queue and stop if something goes horribly wrong
        if (queue_push(&q, temp) == QUEUE_FAILURE) {
            fprintf(stderr,"Failed to push to queue. Thread halting.\n");
            return NULL; // exit quietly
        }
        // printf("%s: %s\n",(char*)file_name,hostname);
        pthread_mutex_unlock(&queue_lock);
    }
    // Processed all lines in the file, close the file and halt
    fclose(fp);
    return NULL;
}
