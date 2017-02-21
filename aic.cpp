/*********************************************
	Adaptive Index Coding (MTF variant)
**********************************************/
#ifndef coder_H
#define coder_H

class coder 
{
public:
	void encode(byte *inbuf, int *size, byte *outbuf, int *out_size);
	void decode(byte *inbuf, int *size, byte *outbuf, int *out_size);
};

void coder::encode(byte *inbuf, int *size, byte *outbuf, int *out_size)
{ 	
	int P[256];
	int S2R[256];
	for(int k = 0; k < 256; k++)
	{
		P[k] = k;
		S2R[k] = k;
	}
	for(int i = 0; i < *size; i++)
	{
		byte c = inbuf[i];
		byte idx = S2R[c];	
		outbuf[i] = idx;
		if(idx > 0)
		{
			int LB = idx >> 2; // lower bound
			do { S2R[P[idx] = P[idx - 1]] = idx; } while(LB < --idx);
			P[LB] = c;
			S2R[P[LB] = c] = LB;
		}
	}
	*out_size = *size;
}

void coder::decode(byte *inbuf, int *size, byte *outbuf, int *out_size)
{
	int P[256];
	for(int k = 0; k < 256; k++) P[k] = k;
	for(int i = 0; i < *size; i++)
	{
		byte idx = inbuf[i];
		byte c = P[idx];
		outbuf[i] = c;
		if(idx > 0)
		{
			int LB = idx >> 2;
			do { P[idx] = P[idx - 1]; } while(LB < --idx);
			P[LB] = c;
		}
	}
	*out_size = *size;
}

#endif // coder_H
