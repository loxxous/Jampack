/*********************************************
* These are the defaults for the compressor and decompressor
**********************************************/
#ifndef FORMAT_H
#define FORMAT_H

#include <time.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <pthread.h>
#include "sys_detect.h"

#define DEFAULT_BLOCKSIZE 	4 << 20
#define MIN_BLOCKSIZE 		1 << 20
#define MAX_BLOCKSIZE 		1000 << 20
#define MAX_THREADS		getCoreCount()
#define MIN_THREADS		1
#define DEFAULT_THREADS 	((getCoreCount() == 1) ? 1 : getCoreCount() - 1)
#define DUPE_HBITS 			20	// The size of the LZ dedupe table
#define SHORT_HBITS 			18	// The size of the LZ short match table
#define LZ_DUPE_ELEMENTS 		1 << DUPE_HBITS	
#define LZ_SHORT_ELEMENTS 		1 << SHORT_HBITS
#define BWT_UNITS 		840		// The amount of independant parallel units that can process the BWT block

static const char Magic[]="JAM";
static const char MagicLength = 3;
typedef int Index;

struct Buffer
{
	unsigned char *block;
	int *size;
};

static  pthread_mutex_t lock; 

void Error(const char *string)
{
	pthread_mutex_init(&lock, NULL);
	pthread_mutex_lock(&lock);
		printf ("\n Error: %s \n", string);		
	pthread_mutex_unlock(&lock);
	exit(1);
}

#endif // FORMAT_H
