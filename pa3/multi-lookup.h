/* multi-lookup.h
 * Akira Youngblood, 2017-03-15
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
