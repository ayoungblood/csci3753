/* multi-lookup.c
 * Main code for multi-threaded DNS resolution engine
 * Based on `pthread_hello.c` from PA3 assignment files
 * All error and debug messages are sent to stderr
 * Resolved results are the only thing sent to stdout
 */

#include "multi-lookup.h"

// Global queue
const int queueSize = 20;
queue q;
// Global output file
FILE* outputfp = NULL;
// Global locks
pthread_mutex_t queue_lock, output_lock;

int main(int argc, char *argv[]) {
    int i, rv;
    // Parse command-line arguments
    if (argc <= 2) {
        // Need at least two file names (in and out), warn and print usage
        fprintf(stderr,"Not enough arguments provided.\n");
        fprintf(stderr,"Usage:\n"
                       "  resolve infile [infile2 ...] outfile\n");
        return EXIT_FAILURE;
    }
    // We have at least one input file and an output file
    /* DEBUG */
    for (i = 1; i < argc-1; ++i) {
        printf("Input file: %s\n",argv[i]);
    }
    printf("Output file: %s\n",argv[argc-1]);
    /* END DEBUG */
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
        fprintf(stderr,"Spawning requester thread %d\n", i);
        // Create the thread and make sure it was created, pass the filename
        rv = pthread_create(&(threads_rqr[i]), NULL, RequesterThreadAction, argv[i+1]);
        if (rv){
            fprintf(stderr,"Error: failed to create requester thread %d, rv = %d\n", i, rv);
            exit(EXIT_FAILURE);
        }
    }
    // Wait for requester threads to finish
    for (i = 0; i < NUM_THREADS_RQR; i++){
           pthread_join(threads_rqr[i],NULL);
    }
    printf("All requester threads have finished, printing queue contents:\n");

    fclose(outputfp);
    pthread_mutex_destroy(&queue_lock);
    pthread_mutex_destroy(&output_lock);
    return 0;
}

// Run by each requester thread.
// Opens file, adds hostnames to queue, and exits
void* RequesterThreadAction(void* file_name) {
    fprintf(stderr,"RequesterThreadAction: %s\n",(char *)file_name);
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
