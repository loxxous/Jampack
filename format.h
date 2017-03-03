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

#define DEFAULT_BLOCKSIZE 	16 << 20
#define MIN_BLOCKSIZE 		1 << 20
#define MAX_BLOCKSIZE 		256 << 20	// This is limited by the LZ77 precompressor specs
#define MAX_THREADS		getCoreCount()
#define MIN_THREADS		1
#define DEFAULT_THREADS 	((getCoreCount() == 1) ? 1 : getCoreCount() - 1)
#define HBITS 			20
#define LZ_ELEMENTS 		1 << HBITS	// The size of the LZ hash table
#define BWT_UNITS 		840		// The amount of independant parallel units that can process the BWT block

const char Magic[]="JAM";
typedef int Index;

struct Buffer
{
	unsigned char *block;
	int *size;
};

struct Match
{
	int pos;
	int ctx;
};

static inline int Hash(int word)
{
	return (word * 0x9E3779B1) >> (32 - HBITS);
}

static  pthread_mutex_t lock; 
int Global_Error_Count = 0;

/*
	When debugging mode is disabled it allows the process to kill itself during an error. 
	Enabling debugging displays error information and prevents too many errors from occuring.
*/
void Error(const char *string)
{
	pthread_mutex_init(&lock, NULL);
	pthread_mutex_lock(&lock);
	
		printf ("\n Error: %s \n", string);
		#ifdef NDEBUG
			exit(1);
		#endif
		Global_Error_Count++;
		if(Global_Error_Count > 8){ printf ("\n Aborting operation: Too many errors! \n"); exit(2); }
		
	pthread_mutex_unlock(&lock);
}

#endif // FORMAT_H
