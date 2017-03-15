/* multi-lookup.h
 * For multi-threaded DNS resolution engine
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include "queue.h"
#include "util.h"

void* RequesterThreadAction(void* file_name);
void* ResolverThreadAction(void* fp);
