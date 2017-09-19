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
#include <emmintrin.h>
#include <omp.h>
#include "sys_detect.hpp"

#define JAM_VERSION		0.80
#define DEFAULT_BLOCKSIZE 	8 << 20
#define MIN_BLOCKSIZE 		1 << 20
#define MAX_BLOCKSIZE 		1000 << 20
#define MAX_THREADS		GetCoreCount()
#define MIN_THREADS		1
#define DEFAULT_THREADS 	((GetCoreCount() == 1) ? 1 : GetCoreCount() - 1)
#define BWT_UNITS 		120	// The amount of independant parallel units that can process the BWT block
#define MAX_GPU_RESOURCES	0.80	// Use up to 80% of GPU memory

static const char Magic[]="JAM";
static const char MagicLength = 3;

typedef int Index;

/**
* This type is used for de/compression stage calls for easy Func(Input, Output) style plug and play.
*/
struct Buffer
{
	unsigned char *block;
	Index *size;
};

/**
* This contains information which gets passed from the command-line to the algorithm
*/
struct Options
{
	Index BlockSize; // Size of the block to compress
	unsigned int MatchFinder; // 8 = Suffix array match finding, 1 to 7 is hash chain with memory reduction by 1/n+1, 0 is fast dedupe 
	unsigned int Threads; // Pretty self explanatory 
	unsigned int Filters; // Brute force filter configurations instead of distance histogram detection (tries 96+1 filter configurations and picks the best)
	bool Gpu; // Use gpu acceleration if available 
	bool Multiblock; // Use multiple block threading if true, if false then it uses multiple threads working on a single block.
};

/**
* When an impossible condition happens that will corrupt the output we call this.
*/
extern void Error(const char *string);

#ifdef __CUDACC__

	#include <cuda_runtime.h>
	
	#define cudaCheck(condition) \
		if (condition != cudaSuccess){ Error("Cuda error detected!\n"); }
	
#endif

#endif // FORMAT_H
