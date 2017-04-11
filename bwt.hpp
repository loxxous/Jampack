/*********************************************
* Asymmetric Burrows Wheeler Transform
**********************************************/
#ifndef BWT_H
#define BWT_H

#include "format.hpp"
#include "divsufsort.hpp"

class BlockSort
{
	public:
	void ForwardBWT(Buffer Input, Buffer Output, Index *Indicies);
	void InverseBWT(Buffer Input, Buffer Output, Index *Indicies, int Threads);
	
	private:
	class Parallel_BWT
	{
		private:
		unsigned char *BWT; unsigned char *T; int Step; Index *p; Index Idx; Index* MAP; int *Offset; int Start; int End; int N_Units;
		
		public:
		void Load(unsigned char *_BWT, unsigned char *_T, int _Step, Index *_p, Index _Idx, Index* _MAP, int *_Offset, int _Start, int _End, int _NU);
		void Threaded_Inversion();
		#ifdef __CUDACC__
		__device__ void CUDA_Threaded_Inversion();
		#endif
	};
	public:
	#ifdef __CUDACC__
	__host__ __device__ void CUDA_Threaded_Launch(Parallel_BWT* pBWT, int Threads);
	#endif
};
#endif // BWT_H
