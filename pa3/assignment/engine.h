/* engine.h
 * For multi-threaded DNS resolution engine
 */

#ifndef _ENGINE_H
#define _ENGINE_H

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "queue.h"

//#define NUM_THREADS	5
void* RequesterThreadAction(void* file_name);
void* PrintHello(void* threadid);

// ANSI colour escapes
#define ANSI_C_BLACK   "\x1b[1;30m"
#define ANSI_C_RED     "\x1b[1;31m"
#define ANSI_C_YELLOW  "\x1b[1;33m"
#define ANSI_C_GREEN   "\x1b[1;32m"
#define ANSI_C_CYAN    "\x1b[1;36m"
#define ANSI_C_BLUE    "\x1b[1;34m"
#define ANSI_C_MAGENTA "\x1b[1;35m"
#define ANSI_C_WHITE   "\x1b[1;37m"
#define ANSI_C_RESET   "\x1b[0m"

#endif /* _ENGINE_H */
