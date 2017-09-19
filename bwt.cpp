/*********************************************
* Asymmetric Burrows Wheeler Transform
* Parallel and Out-of-Order execution decoding on a single block.
**********************************************/
#include "bwt.hpp"

#ifdef __CUDACC__
__global__ void BlockSort::CUDAInverse(int Threads, int Units, unsigned char *Bwt, unsigned char *T, int Step, Index *p, Index Idx, Index* MAP, int *Offset)
{
	int j = (blockIdx.x * Threads / Units) + threadIdx.x;
	if(j < Threads)
	{
		for (int i = 0; i != Step; i++)
		{
			p[j] = MAP[p[j] - 1];
			T[i + Offset[j]] = Bwt[p[j] - (p[j] >= Idx)];
		}
	}
}
#endif

void BlockSort::Bwt::ForwardBwt(Buffer Input, Buffer Output)
{
	unsigned char *T = Input.block;
	unsigned char* Bwt = Output.block;
	int Len = *Input.size;
	*Output.size = *Input.size + (BWT_UNITS * sizeof(Index));
	
	int remainder = Len % BWT_UNITS;
	int nlen = Len - remainder;

	for(int i = 0; i < remainder; i++) 
		Bwt[nlen + i] = T[nlen + i]; 
	
	if(nlen > 0)
	{
		Index Indicies[BWT_UNITS] = {0};
		Index *SA = (Index*)calloc(nlen, sizeof(Index)); 
		if (SA == NULL) 
			Error("Bwt :: Couldn't allocate suffix array!");
		if(divsufsort(T, SA, nlen) != 0) 
			Error("Bwt :: Failure computing the Suffix Array!");

		int step = nlen / BWT_UNITS;
		
		for(Index i = 0; i < nlen; i++) 
			if((SA[i] % step) == 0) 
				Indicies[SA[i] / step] = i;
		
		Bwt[0] = T[nlen - 1];
		Index idx = Indicies[0];
			
		for(Index i = 0; i < idx; i++) 
			Bwt[i + 1] = T[(SA[i] - 1) % nlen];
		for(Index i = idx + 1; i < nlen; i++) 
			Bwt[i] = T[(SA[i] - 1) % nlen];
		for(Index i = 0; i < BWT_UNITS; i++) 
			Indicies[i] += 1;
		
		for(Index i = 0; i < BWT_UNITS; i++)
			memcpy(&Bwt[Len + (i * sizeof(Index))], &Indicies[i], sizeof(Index));

		free(SA);
	}
}

/**
* Sharing an even workload over an uneven amount of threads is kinda tricky, you end up losing position information when you round.
* Easiest solution is to have the maximum allowed threads be a least common multiple of common thread counts.
* 120 is a nice multiple of 1, 2, 3, 4, 5, 6, and 8. Which is fine for CPU threading, and GPU threading.
*/
void BlockSort::Bwt::InverseBwt(Buffer Input, Buffer Output, Options Opt)
{
	int Threads = Opt.Threads;
	unsigned char *Bwt = Input.block;
	unsigned char *T = Output.block;
	int Len = *Input.size -= (BWT_UNITS * sizeof(Index));
	*Output.size = *Input.size;
	
	int remainder = Len % BWT_UNITS;
	int nlen = Len - remainder;
	for(int i = 0; i < remainder; i++)
		T[nlen + i] = Bwt[nlen + i];
	
	if(nlen > 0)
	{
		Index Indicies[BWT_UNITS] = {0};
		for(Index i = 0; i < BWT_UNITS; i++)
			memcpy(&Indicies[i], &Bwt[Len + (i * sizeof(Index))], sizeof(Index));

		// Adjust thread counts if necessary (shares the workload evenly)
		int Units = 4;
		int N_Units = Threads;

		#ifdef __CUDACC__
		bool InvertOnGPU = false;
		
		if(Opt.Gpu == true)
		{
			if(CheckCudaSupport() == true)
			{
				uint64_t CudaMemory = GetCudaMemory();
				if((CudaMemory * MAX_GPU_RESOURCES) > (nlen * (sizeof(Index) + (sizeof(unsigned char) * 2)))) // See if there's enough space to move everything to the GPU.
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
		}
		#endif
		while ((BWT_UNITS % (N_Units * Units)) != 0)
		{
			if ((N_Units * Units) >= BWT_UNITS)
			{
				N_Units = Threads;
				while ((BWT_UNITS % (N_Units * Units)) != 0)
				{
					if (N_Units <= 0) 
						Error("Bwt :: Arithmetic error has occurred!");
					N_Units--;
				}
				if((BWT_UNITS % (N_Units * Units)) == 0) break;
			}
			N_Units++;
		}
		N_Units *= Units;
		Threads = N_Units / Units;
		
		// Compute all the necessities
		Index* Map = (Index*)malloc(sizeof(Index) * nlen); 
		if (Map == NULL) 
			Error("Bwt :: Couldn't allocate index map!");
			
		Index idx = Indicies[0];
		
		Index count[257] = {0};
		{
			Index F[8][257] = {{0}};
			int j = nlen;
			while((j-8) > 0)
			{
				++F[0][Bwt[j-1]+1];
				++F[1][Bwt[j-2]+1];
				++F[2][Bwt[j-3]+1];
				++F[3][Bwt[j-4]+1];
				++F[4][Bwt[j-5]+1];
				++F[5][Bwt[j-6]+1];
				++F[6][Bwt[j-7]+1];
				++F[7][Bwt[j-8]+1];
				j-=8;
			}
			while(j > 0)
			{
				++F[0][Bwt[j-1]+1];
				j--;
			}
			for(int k = 0; k < 257; k++)
				F[0][k] = F[0][k] + F[1][k] + F[2][k] + F[3][k] + F[4][k] + F[5][k] + F[6][k] + F[7][k];
			
			for(int k = 0; k < 257; k++)
				count[k]=  F[0][k];
		}
		for (Index i = 1; i < 256; ++i) 
			count[i] += count[i - 1];
		
		for (Index i = 0; i < idx; ++i)
			Map[count[Bwt[i]]++] = i;
		for (Index i = idx; i < nlen; ++i)
			Map[count[Bwt[i]]++] = i + 1;			
	
		Index step = nlen / N_Units;
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
			unsigned char *d_Bwt;
			unsigned char *d_T;
			Index* d_p;
			Index* d_offset;
			Index* d_Map;

			cudaCheck(cudaMalloc(&d_Bwt, sizeof(unsigned char) * nlen));
			cudaCheck(cudaMalloc(&d_T, sizeof(unsigned char) * nlen));
			cudaCheck(cudaMalloc(&d_Map, sizeof(Index) * nlen)); 
			cudaCheck(cudaMalloc(&d_p, sizeof(Index) * N_Units));
			cudaCheck(cudaMalloc(&d_offset, sizeof(Index) * N_Units));
			
			cudaCheck(cudaMemcpy(d_Bwt, Bwt, sizeof(unsigned char) * nlen, cudaMemcpyHostToDevice));			
			cudaCheck(cudaMemcpy(d_T, T, sizeof(unsigned char) * nlen, cudaMemcpyHostToDevice));
			cudaCheck(cudaMemcpy(d_Map, Map, sizeof(Index) * nlen, cudaMemcpyHostToDevice));
			cudaCheck(cudaMemcpy(d_p, p, sizeof(Index) * N_Units, cudaMemcpyHostToDevice));
			cudaCheck(cudaMemcpy(d_offset, offset, sizeof(Index) * N_Units, cudaMemcpyHostToDevice));
			
			int TryUnits = 32;
			int CudaUnits = TryUnits;
			
			while ((Threads % CudaUnits) != 0)
			{
				if (CudaUnits >= BWT_UNITS)
				{
					CudaUnits = TryUnits;
					while ((Threads % CudaUnits) != 0)
					{
						if (CudaUnits <= 0) 
							Error("Bwt :: Arithmetic error has occurred!");
						CudaUnits--;
					}
					if((Threads % CudaUnits) == 0) break;
				}
				CudaUnits++;
			}
			
			dim3 dimGrid(CudaUnits);
			dim3 dimBlock(Threads/CudaUnits);
			
			CUDAInverse<<<dimGrid, dimBlock>>>(Threads, CudaUnits, d_Bwt, d_T, step, &d_p[0], idx, d_Map, &d_offset[0]);
			
			cudaDeviceSynchronize(); // wait for gpu to finish 
			
			cudaCheck(cudaMemcpy(T, d_T, sizeof(unsigned char) * nlen, cudaMemcpyDeviceToHost));
			
			cudaCheck(cudaFree(d_Bwt));
			cudaCheck(cudaFree(d_T));
			cudaCheck(cudaFree(d_Map));
			cudaCheck(cudaFree(d_p));
			cudaCheck(cudaFree(d_offset));
		}
		else
		{
			#pragma omp parallel for num_threads(Threads) 
			for(int n = 0; n < Threads; n++) // Threaded
			{
				int start = n * Units;
				int end = (n + 1) * Units;
				for (int i = 0; i != step; i++) // OoOE
				{
					for (int j = start; j != end; j++) // Worker loop
					{
						p[j] = Map[p[j] - 1];
						T[i + offset[j]] = Bwt[p[j] - (p[j] >= idx)];
					}
				}
			}
		}
		#endif

		#ifndef __CUDACC__
		#pragma omp parallel for num_threads(Threads)
		
		for(int n = 0; n < Threads; n++)
		{
			int start = n * Units;
			int end = (n + 1) * Units;
			for (int i = 0; i != step; i++)
			{
				for (int j = start; j != end; j++)
				{
					p[j] = Map[p[j] - 1]; 
					T[i + offset[j]] = Bwt[p[j] - (p[j] >= idx)];
				}
			}
		}
		#endif
		
		delete[] p;
		delete[] offset;
		free(Map);
	}
}
