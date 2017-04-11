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
#include <omp.h>
#include "sys_detect.hpp"

#ifdef __CUDACC__
	#include <cuda_runtime.h>
#endif

#define JAM_VERSION	0.60
#define DEFAULT_BLOCKSIZE 	4 << 20
#define MIN_BLOCKSIZE 		1 << 20
#define MAX_BLOCKSIZE 		1000 << 20
#define MAX_THREADS		GetCoreCount()
#define MIN_THREADS		1
#define DEFAULT_THREADS 	((GetCoreCount() == 1) ? 1 : GetCoreCount() - 1)
#define DUPE_HBITS 			20	// The size of the LZ dedupe table
#define SHORT_HBITS 			18	// The size of the LZ short match table
#define LZ_DUPE_ELEMENTS 		1 << DUPE_HBITS	
#define LZ_SHORT_ELEMENTS 		1 << SHORT_HBITS
#define BWT_UNITS 		840		// The amount of independant parallel units that can process the BWT block
#define MAX_GPU_RESOURCES	0.80	// Use up to 80% of GPU memory

static const char Magic[]="JAM";
static const char MagicLength = 3;
typedef int Index;

struct Buffer
{
	unsigned char *block;
	int *size;
};

extern void Error(const char *string);

#endif // FORMAT_H
