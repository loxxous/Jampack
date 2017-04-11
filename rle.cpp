/*********************************************
* Run length encoding of zeroes
**********************************************/
#include "rle.hpp"

inline int RLE::getMSB(unsigned int v)
{
	int msb = 0;
	while (v) 
	{
		v >>= 1;
		msb++;
	}
	return msb;
}

/*
	Encode 8-bit input into 16-bit output
	Runs of symbol 0 get encoded as a binary permutation of symbols 0 and 1,
	all other symbols greater than 0 are coded as sym + 1.
*/
void RLE::encode(unsigned char *in, unsigned short *out, int *len)
{ 	
	int in_p = *len;
	int out_p = 0;	
	for(int i = 0; i < in_p;)
	{		
		if(in[i] == 0)
		{
			int run = 1;
			while (in[i] == in[i+run] && (i+run) < in_p)
				run++;
			i += run;
			
			int L = run + 1;
			int msb = getMSB(L) - 1;
			
			while(msb--)
				out[out_p++] = (L >> msb) & 1;	
		}
		else
		{
			out[out_p++] = in[i++] + 1;
		}
	}
	*len = out_p;
}

/*
	Decode 16-bit input into 8-bit output
*/
void RLE::decode(unsigned short *in, unsigned char *out, int *len, int real_len)
{
	int in_p = *len;
	int out_p = 0;
	for(int i = 0; i < in_p;)
	{
		if(in[i] > 1)
		{
			out[out_p++] = in[i++] - 1;
		}
		else
		{
			int rle = 1;
			while (in[i] <= 1 && i < in_p)
				rle = (rle << 1) | in[i++];
			rle -= 1;
			while(rle--)
				out[out_p++] = 0;	
		}
	}
	if(out_p != real_len) Error("rle mismatch!");
	*len = out_p;
}
