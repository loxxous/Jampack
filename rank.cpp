/*********************************************
* Second stage algorithm, Move-to-front
**********************************************/
#include "rank.hpp"

void Postcoder::encode(unsigned char *block, int len)
{ 	
	int S2R[256];
	int R2S[256];
	
	for(int k = 0; k < 256; k++)
	{
		R2S[k] = k;
		S2R[k] = k;
	}
	for(int i = 0; i < len; i++)
	{
		unsigned char c = block[i];
		unsigned char idx = S2R[c];	
		block[i] = idx;

		if(idx > 0)
		{
			int LB = idx >> 2;
			int idx_cpy = idx;
			do { S2R[R2S[idx_cpy] = R2S[idx_cpy - 1]] = idx_cpy; } while(LB < --idx_cpy);
			S2R[R2S[LB] = c] = LB;
		}
	}
}

void Postcoder::decode(unsigned char *block, int len)
{
	int R2S[256];
	for(int k = 0; k < 256; k++) 
		R2S[k] = k;
	for(int i = 0; i < len; i++)
	{
		unsigned char idx = block[i];
		unsigned char c = R2S[idx];
		block[i] = c;

		if(idx > 0)
		{
			int LB = idx >> 2;
			int idx_cpy = idx;
			do { R2S[idx_cpy] = R2S[idx_cpy - 1]; } while(LB < --idx_cpy);
			R2S[LB] = c;
		}
	}
}
