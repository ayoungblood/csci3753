/* multi-lookup.c
 * Main code for multi-threaded DNS resolution engine
 * Based on `pthread_hello.c` from PA3 assignment files
 * All error and debug messages are sent to stderr
 * Resolved results are the only thing sent to stdout
 */

#include "multi-lookup.h"

const int queueSize = 20;

int main(int argc, char *argv[]) {
    int i, rv;
    // Parse command-line arguments
    if (argc <= 1) {
        // No input files, warn and print usage
        fprintf(stderr,"No input files specified.\n");
        fprintf(stderr,"Usage:\n"
                       "  resolve file [...]\n");
        return 1; // return an error
    }
    // We have at least one input file
    for (i = 1; i < argc; ++i) {
        printf("%s\n",argv[i]);
    }
    // Create a requester thread pool based on number of input files
    // Some may be invalid, but that is handled by the threads
    const int NUM_THREADS_RQR = argc-1;
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
    printf("All requester threads have finished\n");


    // Create a queue
    queue q;
    if (queue_init(&q, queueSize) == QUEUE_FAILURE){
        fprintf(stderr, "error: queue_init failed!\n");
    }
    /*
    // Test for empty queue when empty
    if (!queue_is_empty(&q)) {
        fprintf(stderr,"error: queue should report empty\n");
    }
    // Test for full queue when empty
    if (queue_is_full(&q)) {
        fprintf(stderr,"error: queue should report empty\n");
    }
    */
    return 0;

    /*
    // Setup Local Vars
    pthread_t threads[NUM_THREADS];
    int rc;
    long t;
    long cpyt[NUM_THREADS];

    // Spawn NUM_THREADS threads
    for(t=0;t<NUM_THREADS;t++){
        printf("In main: creating thread %ld\n", t);
        cpyt[t] = t;
        rc = pthread_create(&(threads[t]), NULL, PrintHello, &(cpyt[t]));
        if (rc){
            printf("ERROR; return code from pthread_create() is %d\n", rc);
            exit(EXIT_FAILURE);
        }
    }

    // Wait for All Theads to Finish
    for(t=0;t<NUM_THREADS;t++){
           pthread_join(threads[t],NULL);
    }
    printf("All of the threads were completed!\n");

    */
    /* Last thing that main() should do */
    /* pthread_exit unnecessary due to previous join */
    //pthread_exit(NULL);

    return 0;
}

void* RequesterThreadAction(void* file_name) {
    fprintf(stderr,"RequesterThreadAction: %s\n",(char *)file_name);
    return NULL;
}

/* Function for Each Thread to Run */
void* PrintHello(void* threadid) {
    /* Setup Local Vars and Handle void* */
    long* tid = threadid;
    long t;
    long numprint = 3;

    /* Print hello numprint times */
    for(t=0; t<numprint; t++)
    {
        printf("Hello World! It's me, thread #%ld! "
           "This is printout %ld of %ld\n",
           *tid, (t+1), numprint);
        /* Sleep for 1 to 2 Seconds */
        usleep((rand()%10)*100+1000);
    }

    /* Exit, Returning NULL*/
    return NULL;
}
