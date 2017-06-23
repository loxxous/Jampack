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
		void ForwardBwt(Buffer Input, Buffer Output, Index *Indicies);
		void InverseBwt(Buffer Input, Buffer Output, Index *Indicies, int Threads, bool UseGpu);
		
		class ParallelBwt
		{
			private:
			unsigned char *Bwt; unsigned char *T; int Step; Index *p; Index Idx; Index* MAP; int *Offset; int Start; int End; int N_Units;
			
			public:
			void Load(unsigned char *_Bwt, unsigned char *_T, int _Step, Index *_p, Index _Idx, Index* _MAP, int *_Offset, int _Start, int _End, int _NU);
			void ThreadedInversion();
		};
	};
	#ifdef __CUDACC__
	__global__ void CUDAInverse(int Threads, int Units, unsigned char *Bwt, unsigned char *T, int Step, Index *p, Index Idx, Index* MAP, int *Offset);
	#endif
};
#endif // BWT_H
