/*********************************************
* ASYMMETRIC BURROWS WHEELER TRANSFORM
*		
* Asymmetric BWT uses multiple BWT indexes to allow for multiple decoding streams to interleave without any dependencies.
* By default it uses 8-way BWT, this requires the block be a multiple of N, so any odd bytes are left unprocessed.
**********************************************/
#ifndef BWT_H
#define BWT_H

#include "divsufsort.h"

class bwt
{
public:
	void forwardBWT(int *SA, byte *T, byte* BWT, int *len, int *indices);
	void inverseBWT(int *MAP, byte *BWT, byte *T, int *len, int *indices);
};

	void bwt::forwardBWT(int *SA, byte *T, byte* BWT, int *len, int *indices)
	{
		int remainder = *len % 8;
		int newlen = *len - remainder;

		for(int i = 0; i < remainder; i++) BWT[newlen + i] = T[newlen + i]; 
		if(divsufsort(T, SA, newlen) != 0) { printf("Failure computing the Suffix Array!\n"); exit(1); }
		
		int step = newlen / 8;
		int offset[7] = { step, step*2, step*3, step*4, step*5, step*6, step*7 };
		
		for(int i = 0; i < newlen; i++) 
		{
			if(SA[i] == 0) indices[0] = i;
			else if (SA[i] == offset[0]) indices[1] = i;
			else if (SA[i] == offset[1]) indices[2] = i;
			else if (SA[i] == offset[2]) indices[3] = i;
			else if (SA[i] == offset[3]) indices[4] = i;
			else if (SA[i] == offset[4]) indices[5] = i;	
			else if (SA[i] == offset[5]) indices[6] = i;
			else if (SA[i] == offset[6]) indices[7] = i;
		}
		
		BWT[0] = T[newlen - 1];
		int idx = indices[0];
		for(int i = 0; i < idx; i++) 
		{
			BWT[i + 1] = T[(SA[i] - 1) % newlen];
		}
		for(int i = idx + 1; i < newlen; i++) 
		{
			BWT[i] = T[(SA[i] - 1) % newlen];
		}
		for(int i = 0; i < 8; i++) indices[i] += 1;
	}

	// 8-way inverse BWT construction
	void bwt::inverseBWT(int *MAP, byte *BWT, byte *T, int *len, int *indices)
	{
		int remainder = *len % 8; 	// find remainder if any
		int newlen = *len - remainder;	// get the new length of processable data
		for(int i = 0; i < remainder; i++) T[newlen + i] = BWT[newlen + i];

		int idx = indices[0];
		int count[257] = { 0 };
		for (int i = 0; i < newlen; ++i) ++count[BWT[i] + 1];
		for (int i = 1; i < 256; ++i) count[i] += count[i - 1];
		for (int i = 0; i < newlen; ++i) int pos = MAP[count[BWT[i]]++] = i + (i >= idx);
		
		int step = newlen / 8;
		int p[8] = { indices[0],  indices[1],  indices[2], indices[3], indices[4], indices[5], indices[6], indices[7] };
		int offset[8] =  { 0, step, (step*2), (step*3), (step*4), (step*5), (step*6), (step*7)};	

		for (int i = 0; i != step; i++) // decode 8 symbols at once
		{
			p[0] = MAP[p[0] - 1];
			p[1] = MAP[p[1] - 1];
			p[2] = MAP[p[2] - 1];
			p[3] = MAP[p[3] - 1];
			p[4] = MAP[p[4] - 1];
			p[5] = MAP[p[5] - 1];
			p[6] = MAP[p[6] - 1];
			p[7] = MAP[p[7] - 1];
			
			T[i + offset[0]] = BWT[p[0] - (p[0] >= idx)];
			T[i + offset[1]] = BWT[p[1] - (p[1] >= idx)];
			T[i + offset[2]] = BWT[p[2] - (p[2] >= idx)];
			T[i + offset[3]] = BWT[p[3] - (p[3] >= idx)];
			T[i + offset[4]] = BWT[p[4] - (p[4] >= idx)];
			T[i + offset[5]] = BWT[p[5] - (p[5] >= idx)];
			T[i + offset[6]] = BWT[p[6] - (p[6] >= idx)];
			T[i + offset[7]] = BWT[p[7] - (p[7] >= idx)];
		}
	}
	
#endif // BWT_H
