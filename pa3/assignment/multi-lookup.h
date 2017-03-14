/* multi-lookup.h
 * For multi-threaded DNS resolution engine
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "queue.h"

//#define NUM_THREADS	5
void* RequesterThreadAction(void* file_name);
void* PrintHello(void* threadid);
