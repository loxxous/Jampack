/*********************************************
* Asymmetric Burrows Wheeler Transform
**********************************************/
#ifndef BWT_H
#define BWT_H

#include "format.hpp"
#include "divsufsort.hpp"
#include "sys_detect.hpp"

namespace BlockSort
{
	class Bwt
	{
		public:
		void ForwardBwt(Buffer Input, Buffer Output);
		void InverseBwt(Buffer Input, Buffer Output, Options Opt);
	};
	#ifdef __CUDACC__
	__global__ void CUDAInverse(int Threads, int Units, unsigned char *Bwt, unsigned char *T, int Step, Index *p, Index Idx, Index* MAP, int *Offset);
	#endif
};
#endif // BWT_H
