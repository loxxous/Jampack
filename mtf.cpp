/*********************************************
 Second stage algorithm, Move-to-front variant
**********************************************/
#ifndef postcoder_H
#define postcoder_H

class Postcoder 
{
public:
	void encode(unsigned char *block, int len);
	void decode(unsigned char *block, int len);
};

void Postcoder::encode(unsigned char *block, int len)
{ 	
	int R2S[256];
	int S2R[256];

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
			int LB = idx >> 2; // lower bound
			do { S2R[R2S[idx] = R2S[idx - 1]] = idx; } while(LB < --idx);
			R2S[LB] = c;
			S2R[R2S[LB] = c] = LB;
		}
	}
}

void Postcoder::decode(unsigned char *block, int len)
{
	int R2S[256];
	for(int k = 0; k < 256; k++) R2S[k] = k;
	for(int i = 0; i < len; i++)
	{
		unsigned char idx = block[i];
		unsigned char c = R2S[idx];
		block[i] = c;
		if(idx > 0)
		{
			int LB = idx >> 2;
			do { R2S[idx] = R2S[idx - 1]; } while(LB < --idx);
			R2S[LB] = c;
		}
	}
}

#endif // postcoder_H
