/*********************************************
* Asymmetric Burrows Wheeler Transform
**********************************************/
#include "bwt.hpp"

void BlockSort::BWT::ParallelBWT::Load(unsigned char *_BWT, unsigned char *_T, int _Step, Index *_p, Index _Idx, Index* _MAP, int *_Offset, int _Start, int _End, int _NU)
{
	BWT = _BWT; 
	T = _T; 
	MAP = _MAP;
	Idx = _Idx, 
	Step = _Step; 
	Start = _Start; 
	End = _End;
	N_Units = _NU;
			
	p = (Index*)calloc(N_Units, sizeof(Index));
	Offset = (int*)calloc(N_Units, sizeof(int)); 
	if (Offset == NULL || p == NULL) Error("Couldn't allocate inversion pool!"); 
	
	for (int i = 0; i < N_Units; i++)
	{
		p[i] = _p[i];
		Offset[i] = _Offset[i];
	}
	
	int initpos = Offset[Start];
	T += initpos;
	for (int i = Start; i < End; i++)
		Offset[i] = Offset[i] - initpos;
}

void BlockSort::BWT::ParallelBWT::ThreadedInversion()
{
	for (int i = 0; i != Step; i++)
	{
		for (int j = Start; j != End; j++)
		{
			p[j] = MAP[p[j] - 1];
			T[i + Offset[j]] = BWT[p[j] - (p[j] >= Idx)];
		}
	}
	free(p);
	free(Offset);
}

#ifdef __CUDACC__
__global__ void BlockSort::CUDAInverse(int Threads, int Units, unsigned char *BWT, unsigned char *T, int Step, Index *p, Index Idx, Index* MAP, int *Offset)
{
	int j = (blockIdx.x * Threads / Units) + threadIdx.x;
	if(j < Threads)
	{
		for (int i = 0; i != Step; i++)
		{
			p[j] = MAP[p[j] - 1];
			T[i + Offset[j]] = BWT[p[j] - (p[j] >= Idx)];
		}
	}
}
#endif

void BlockSort::BWT::ForwardBWT(Buffer Input, Buffer Output, Index *Indicies)
{
	unsigned char *T = Input.block;
	unsigned char* BWT = Output.block;
	int *Len = Input.size;
	*Output.size = *Input.size;
	
	int remainder = *Len % BWT_UNITS;
	int newLen = *Len - remainder;

	for(int i = 0; i < remainder; i++) 
		BWT[newLen + i] = T[newLen + i]; 
		
	if(newLen > 0)
	{
		Index* SA = (Index*)calloc(newLen, sizeof(Index)); if (SA == NULL) Error("Couldn't allocate suffix array!");
		if(divsufsort(T, SA, newLen) != 0) Error("Failure computing the Suffix Array!");
			
		int step = newLen / BWT_UNITS;
			
		for(Index i = 0; i < newLen; i++) 
		if ((SA[i]%step)==0) Indicies[SA[i]/step] = i;
		
		BWT[0] = T[newLen - 1];
		Index idx = Indicies[0];
			
		for(Index i = 0; i < idx; i++) 
			BWT[i + 1] = T[(SA[i] - 1) % newLen];
		for(Index i = idx + 1; i < newLen; i++) 
			BWT[i] = T[(SA[i] - 1) % newLen];
		for(Index i = 0; i < BWT_UNITS; i++) 
			Indicies[i] += 1;
		
		free(SA);
	}
}

/*
	Sharing an even workload over an uneven amount of threads is kinda tricky, you end up losing position information when you round.
	Easiest solution is to have the maximum allowed threads be a least common multiple of common thread counts.
	840 is a nice multiple of 1, 2, 3, 4, 5, 6, 7, and 8. Which is perfect for CPU threading, and GPU threading.
*/
void BlockSort::BWT::InverseBWT(Buffer Input, Buffer Output, Index *Indicies, int Threads, bool UseGpu)
{
	unsigned char *BWT = Input.block;
	unsigned char *T = Output.block;
	int *Len = Input.size;
	*Output.size = *Input.size;
		
	int remainder = *Len % BWT_UNITS;
	int newLen = *Len - remainder;
	for(int i = 0; i < remainder; i++) T[newLen + i] = BWT[newLen + i];
		
	if(newLen > 0)
	{
		// Adjust thread counts if necessary (shares the workload evenly)
		int Units = 4;
		int N_Units = Threads;

		#ifdef __CUDACC__
		bool InvertOnGPU = false;
		
		if(CheckCudaSupport() == true && UseGpu == true)
		{
			uint64_t CudaMemory = GetCudaMemory();
			if((CudaMemory * MAX_GPU_RESOURCES) > (newLen * (sizeof(Index) + (sizeof(unsigned char) * 2)))) // See if there's enough space to move everything to the GPU.
			{
				Units = 1;
				uint64_t CudaCores = GetCudaCoreCount();
				if(CudaCores >= BWT_UNITS)
					N_Units = Threads = BWT_UNITS;
				else
					N_Units = Threads = CudaCores;
				InvertOnGPU = true;
			}
		}
		#endif
		while ((BWT_UNITS % (N_Units * Units)) != 0)
		{
			if ((N_Units * Units) >= BWT_UNITS)
			{
				N_Units = Threads;
				while ((BWT_UNITS % (N_Units * Units)) != 0)
				{
					if (N_Units <= 0) Error("Arithmetic error has occurred!");
					N_Units--;
				}
				if((BWT_UNITS % (N_Units * Units)) == 0) break;
			}
			N_Units++;
		}
		N_Units *= Units;
		Threads = N_Units / Units;
		
		// Compute all the necessities
		Index* MAP = (Index*)malloc(sizeof(Index) * newLen); if (MAP == NULL) Error("Couldn't allocate index map!");		
			
		Index idx = Indicies[0];
		Index count[257] = { 0 };
		
		{
			Index F[8][257] = {{0}};
			int j = newLen;
			while((j-8) > 0)
			{
				++F[0][BWT[j-1]+1];
				++F[1][BWT[j-2]+1];
				++F[2][BWT[j-3]+1];
				++F[3][BWT[j-4]+1];
				++F[4][BWT[j-5]+1];
				++F[5][BWT[j-6]+1];
				++F[6][BWT[j-7]+1];
				++F[7][BWT[j-8]+1];
				j-=8;
			}
			while(j > 0)
			{
				++F[0][BWT[j-1]+1];
				j--;
			}
			for(int k = 0; k < 257; k++)
				F[0][k] = F[0][k] + F[1][k] + F[2][k] + F[3][k] + F[4][k] + F[5][k] + F[6][k] + F[7][k];
			
			for(int k = 0; k < 257; k++)
				count[k]=  F[0][k];
		}
		for (Index i = 1; i < 256; ++i) 
		count[i] += count[i - 1];
		
		for (Index i = 0; i < newLen; ++i)
			MAP[count[BWT[i]]++] = i + (i >= idx);
		
		Index step = newLen / N_Units;
		Index* p = new Index[N_Units]; 
		Index* offset = new Index[N_Units]; 
			
		for (int i = 0; i < N_Units; i++) 
			p[i] = Indicies[BWT_UNITS / N_Units * i];
		for (int i = 0; i < N_Units; i++) 
			offset[i] = step * i;

		// INVERT 		
		#ifdef __CUDACC__
		if(InvertOnGPU == true)
		{
			unsigned char *d_BWT;
			unsigned char *d_T;
			Index* d_p;
			Index* d_offset;
			Index* d_MAP;

			cudaAssert(cudaMalloc(&d_BWT, sizeof(unsigned char) * newLen));
			cudaAssert(cudaMalloc(&d_T, sizeof(unsigned char) * newLen));
			cudaAssert(cudaMalloc(&d_MAP, sizeof(Index) * newLen)); 
			cudaAssert(cudaMalloc(&d_p, sizeof(Index) * N_Units));
			cudaAssert(cudaMalloc(&d_offset, sizeof(Index) * N_Units));
			
			cudaAssert(cudaMemcpy(d_BWT, BWT, sizeof(unsigned char) * newLen, cudaMemcpyHostToDevice));			
			cudaAssert(cudaMemcpy(d_T, T, sizeof(unsigned char) * newLen, cudaMemcpyHostToDevice));
			cudaAssert(cudaMemcpy(d_MAP, MAP, sizeof(Index) * newLen, cudaMemcpyHostToDevice));
			cudaAssert(cudaMemcpy(d_p, p, sizeof(Index) * N_Units, cudaMemcpyHostToDevice));
			cudaAssert(cudaMemcpy(d_offset, offset, sizeof(Index) * N_Units, cudaMemcpyHostToDevice));
			
			int TryUnits = 100;
			int CudaUnits = TryUnits;
			
			while ((Threads % CudaUnits) != 0)
			{
				if (CudaUnits >= BWT_UNITS)
				{
					CudaUnits = TryUnits;
					while ((Threads % CudaUnits) != 0)
					{
						if (CudaUnits <= 0) Error("Arithmetic error has occurred!");
						CudaUnits--;
					}
					if((Threads % CudaUnits) == 0) break;
				}
				CudaUnits++;
			}
			
			dim3 dimGrid(CudaUnits);
			dim3 dimBlock(Threads/CudaUnits);
			
			CUDAInverse<<<dimGrid, dimBlock>>>(Threads, CudaUnits, d_BWT, d_T, step, &d_p[0], idx, d_MAP, &d_offset[0]);
			
			cudaDeviceSynchronize(); // wait for gpu to finish 
			
			cudaAssert(cudaMemcpy(T, d_T, sizeof(unsigned char) * newLen, cudaMemcpyDeviceToHost));
			
			cudaAssert(cudaFree(d_BWT));
			cudaAssert(cudaFree(d_T));
			cudaAssert(cudaFree(d_MAP));
			cudaAssert(cudaFree(d_p));
			cudaAssert(cudaFree(d_offset));
		}
		else
		{
			ParallelBWT* pBWT = new ParallelBWT[Threads];			
			for(int n = 0; n < Threads; n++)
			{
				int start = n * Units;
				int end = (n+1) * Units;
				pBWT[n].Load(BWT, T, step, &p[0], idx, MAP, &offset[0], start, end, N_Units);
			}
			#pragma omp parallel for num_threads(Threads) 
			for(int n = 0; n < Threads; n++)
			{
				pBWT[n].ThreadedInversion();
			}
			delete[] pBWT;
		}
		#endif
		
		#ifndef __CUDACC__
		ParallelBWT* pBWT = new ParallelBWT[Threads];			
		for(int n = 0; n < Threads; n++)
		{
			int start = n * Units;
			int end = (n+1) * Units;
			pBWT[n].Load(BWT, T, step, &p[0], idx, MAP, &offset[0], start, end, N_Units);
		}
		#pragma omp parallel for num_threads(Threads) 
		for(int n = 0; n < Threads; n++)
		{
			pBWT[n].ThreadedInversion();
		}
		delete[] pBWT;
		#endif
		
		delete[] p;
		delete[] offset;
		free(MAP);
	}
}
