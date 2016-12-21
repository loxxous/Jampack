/**
*   Defaults for the compressor and decompressor.
*/
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

#define BLOCKSIZE 		4 << 20
//#define MIN_BLOCKSIZE 	1 << 20		// For variable block size support, not yet implemented
//#define MAX_BLOCKSIZE 	100 << 20
#define MAX_THREADS	getCoreCount()
#define MIN_THREADS	1
#define DEFAULT_THREADS ((getCoreCount() == 1) ? 1 : getCoreCount() - 1)
#define HBITS 19
#define ELEMENTS 1 << HBITS

typedef unsigned char byte;
struct match{
	int pos;
	int ctx;
};
static inline int hash(int word){
	return (word * 0x9E3779B1) >> (32 - HBITS);
}

#endif // FORMAT_H