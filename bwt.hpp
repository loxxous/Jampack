/*********************************************
* Asymmetric Burrows Wheeler Transform
**********************************************/
#ifndef BWT_H
#define BWT_H

#include "format.hpp"
#include "divsufsort.hpp"

namespace BlockSort
{
	class BWT
	{
		public:
		void ForwardBWT(Buffer Input, Buffer Output, Index *Indicies);
		void InverseBWT(Buffer Input, Buffer Output, Index *Indicies, int Threads, bool UseGPU);
		
		class ParallelBWT
		{
			private:
			unsigned char *BWT; unsigned char *T; int Step; Index *p; Index Idx; Index* MAP; int *Offset; int Start; int End; int N_Units;
			
			public:
			void Load(unsigned char *_BWT, unsigned char *_T, int _Step, Index *_p, Index _Idx, Index* _MAP, int *_Offset, int _Start, int _End, int _NU);
			void ThreadedInversion();
		};
	};
	#ifdef __CUDACC__
	__global__ void CUDAInverse(int Threads, unsigned char *BWT, unsigned char *T, int Step, Index *p, Index Idx, Index* MAP, int *Offset);
	#endif
};
#endif // BWT_H
