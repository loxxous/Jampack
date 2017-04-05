/*********************************************
* Second stage algorithm, Move-to-front fused with WFC.
* The 8 most recent characters are sorted by their frequency relative to a 
* fast adapting probability (based on fpaqc's model), all other characters use a MTF 
* variant which moves the index 75% of the way to the front relative to the previous index.
**********************************************/
#ifndef postcoder_H
#define postcoder_H

class Postcoder 
{
public:
	void encode(unsigned char *block, int len);
	void decode(unsigned char *block, int len);
private:
	static const int WFC_UPDATE = 8;	// The upper index limit which imposes WFC rules
	static const int FREQ_RANGE = 1 << 16;
	static const int RATE = 3;
};

void Postcoder::encode(unsigned char *block, int len)
{ 	
	int S2R[256];
	int R2S[256];
	int S_Freq[256];
	
	for(int k = 0, init = FREQ_RANGE / 256; k < 256; k++)
		S_Freq[k] = init;
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

		if(idx < WFC_UPDATE) // Update just the WFC table
		{
			// Update frequency ranges
			for(int k = 0; k < WFC_UPDATE; k++)
			{
				if(k == idx)
					S_Freq[k] += FREQ_RANGE - S_Freq[k] >> RATE;
				else
					S_Freq[k] -= S_Freq[k] >> RATE;
			}
			// Sort ranks by frequency of the 8 most recent symbols
			if(idx > 0)
			{
				int j, swap_freq, swap_rank;
				for(int i = 1; i < WFC_UPDATE; i++)
				{
					j = i;
					while(j > 0 && S_Freq[j - 1] < S_Freq[j])
					{
						swap_freq = S_Freq[j];
						swap_rank = R2S[j];
						S_Freq[j] = S_Freq[j - 1];
						S2R[R2S[j] = R2S[j - 1]] = j;
						S_Freq[j - 1] = swap_freq;
						S2R[R2S[j - 1] = swap_rank] = j - 1;
						j--;
					}
				}
			}
		}
		else // Update the MTF table
		{
			int LB = idx >> 2;
			int idx_cpy = idx;
			do { S2R[R2S[idx_cpy] = R2S[idx_cpy - 1]] = idx_cpy; } while(LB < --idx_cpy);
			S2R[R2S[LB] = c] = LB;
		}
		/*
		int Cc[256] = {0};
		for(int p = 0; p < 256; p++)
		{
			if(Cc[R2S[p]]++ > 1) Error("broken mtf");
		}
		*/
	}
	/*
	printf("Freq: ");
	for(int p = 0; p < WFC_UPDATE; p++)
		printf("%i ", S_Freq[p]);
	printf("\n");
	*/
}

void Postcoder::decode(unsigned char *block, int len)
{
	int R2S[256];
	int S_Freq[256];
	
	for(int k = 0, init = FREQ_RANGE / 256; k < 256; k++)
		S_Freq[k] = init;
	for(int k = 0; k < 256; k++) 
		R2S[k] = k;
	for(int i = 0; i < len; i++)
	{
		unsigned char idx = block[i];
		unsigned char c = R2S[idx];
		block[i] = c;
		
		int LB = idx >> 2;
		if(idx < WFC_UPDATE)
		{
			// Update frequency ranges
			for(int k = 0; k < WFC_UPDATE; k++)
			{
				if(k == idx)
					S_Freq[k] += FREQ_RANGE - S_Freq[k] >> RATE;
				else
					S_Freq[k] -= S_Freq[k] >> RATE;
			}
			// Sort ranks by frequency of the 8 most recent symbols
			if(idx > 0)
			{
				int j, swap_freq, swap_rank;
				for(int i = 1; i < WFC_UPDATE; i++)
				{
					j = i;
					while(j > 0 && S_Freq[j - 1] < S_Freq[j])
					{
						swap_freq = S_Freq[j];
						swap_rank = R2S[j];
						S_Freq[j] = S_Freq[j - 1];
						R2S[j] = R2S[j - 1];
						S_Freq[j - 1] = swap_freq;
						R2S[j - 1] = swap_rank;
						j--;
					}
				}
			}
		}
		else
		{
			int idx_cpy = idx;
			do { R2S[idx_cpy] = R2S[idx_cpy - 1]; } while(LB < --idx_cpy);
			R2S[LB] = c;
		}
	}
}

#endif // postcoder_H