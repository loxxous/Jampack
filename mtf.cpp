/*********************************************
 Second stage algorithm, Move-to-front variant
**********************************************/
#ifndef postcoder_H
#define postcoder_H

class Postcoder 
{
public:
	void encode(Buffer Input, Buffer Output);
	void decode(Buffer Input, Buffer Output);
};

void Postcoder::encode(Buffer Input, Buffer Output)
{ 	
	int P[256];
	int S2R[256];
	for(int k = 0; k < 256; k++)
	{
		P[k] = k;
		S2R[k] = k;
	}
	for(int i = 0; i < *Input.size; i++)
	{
		unsigned char c = Input.block[i];
		unsigned char idx = S2R[c];	
		Output.block[i] = idx;
		if(idx > 0)
		{
			int LB = idx >> 2; // lower bound
			do { S2R[P[idx] = P[idx - 1]] = idx; } while(LB < --idx);
			P[LB] = c;
			S2R[P[LB] = c] = LB;
		}
	}
	*Output.size = *Input.size;
}

void Postcoder::decode(Buffer Input, Buffer Output)
{
	int P[256];
	for(int k = 0; k < 256; k++) P[k] = k;
	for(int i = 0; i < *Input.size; i++)
	{
		unsigned char idx = Input.block[i];
		unsigned char c = P[idx];
		Output.block[i] = c;
		if(idx > 0)
		{
			int LB = idx >> 2;
			do { P[idx] = P[idx - 1]; } while(LB < --idx);
			P[LB] = c;
		}
	}
	*Output.size = *Input.size;
}

#endif // postcoder_H
