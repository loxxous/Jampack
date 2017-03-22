/*********************************************
* Asymmetric Burrows Wheeler Transform
*	
* Asymmetric BWT uses multiple BWT indexes to allow for multiple decoding streams to interleave without any dependencies.
* Decoding is parallel on a single block, and even in single threaded mode thanks to out-of-order execution.
**********************************************/
#ifndef BWT_H
#define BWT_H

#include "divsufsort.cpp"

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
		void load(unsigned char *_BWT, unsigned char *_T, int _Step, Index *_p, Index _Idx, Index* _MAP, int *_Offset, int _Start, int _End, int _NU){
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

		void Threaded_Inversion()
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
		
		static void* Launch(void* object){
			Parallel_BWT* obj = reinterpret_cast<Parallel_BWT*>(object);
			obj->Threaded_Inversion();
			return NULL;
		}
		
		void pThread_Inverse_BWT(pthread_t *thread){
			int err = pthread_create(thread, NULL, &Parallel_BWT::Launch, this); 
			if(err) Error("Unable to create thread!");
		}
	};
};

	void BlockSort::ForwardBWT(Buffer Input, Buffer Output, Index *Indicies)
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
			int offset[BWT_UNITS]; 
			for (int i = 0; i < BWT_UNITS; i++) 
				offset[i] = i * step;
			
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
	void BlockSort::InverseBWT(Buffer Input, Buffer Output, Index *Indicies, int Threads)
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
			int N_Units = Units * Threads;
			while ((BWT_UNITS % N_Units) != 0){
				if (N_Units >= BWT_UNITS){
					N_Units = Units * Threads;
					while ((BWT_UNITS % N_Units) != 0){
						if (N_Units <= 0) Error("Arithmetic error has occurred!");
						N_Units--;
					}
				}
				N_Units++;
			}
			Threads = N_Units / Units;
			
			// Compute all the necessities
			Index* MAP = (Index*)calloc(newLen, sizeof(Index)); if (MAP == NULL) Error("Couldn't allocate index map!");
		
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

			int step = newLen / N_Units;
			int p[N_Units]; for (int i = 0; i < N_Units; i++) p[i] = Indicies[BWT_UNITS / N_Units * i];
			int offset[N_Units]; for (int i = 0; i < N_Units; i++) offset[i] = step * i;

			// INVERT 
			Parallel_BWT pBWT[Threads];
			pthread_t pthreads[Threads];
			
			for(int n = 0; n < Threads; n++)
			{
				int start = n * Units;
				int end = (n+1) * Units;
				pBWT[n].load(BWT, T, step, &p[0], idx, MAP, &offset[0], start, end, N_Units);
			}
			for(int n = 0; n < Threads; n++)
				pBWT[n].pThread_Inverse_BWT(&pthreads[n]);
			for(int n = 0; n < Threads; n++)
				pthread_join( pthreads[n], NULL );
	
			free(MAP);
		}
	}

#endif // BWT_H
