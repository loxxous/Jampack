/*********************************************
* Second stage algorithm : Sorted rank coding
*
* SRC is equivalent to Move-to-Front but using a non-sequential output. 
* This produces an almost identical freq spectrum as MTF does but it is ordered completely differently. 
* This allows pretty much pure ranking to take place without worry of contextual distortion on local frequencies.
**********************************************/
#include "rank.hpp"

/**
* Create a SortedMap[] of sorted frequencies of Freq[].
* It repeatedly reduces the set of freqs by finding the largest freq, adding to SortedMap,
* and removing the most recent largest freq from FreqCopy until there are no more freqs above 0.
*/
void Postcoder::GenerateSortedMap(int* Freq, unsigned char* SortedMap)
{
	int FreqCopy[256] = {0};
	for(int i = 0; i < 256; i++)
		FreqCopy[i] = Freq[i];

	for(int j = 0; j < 256; j++)
	{
		int max = 0;
		unsigned char bsym = 0;
		for(int i = 0; i < 256; i++)
		{
			if(FreqCopy[i] > max)
			{
				bsym = i;
				max = FreqCopy[i];
			}
		}
		if(max == 0)
			break;
		else
			SortedMap[j] = bsym;
		FreqCopy[bsym] = 0;
	}
}

/**
* Encoding is fairly straight forward, perform MTF on current symbol and store the rank at bucket[sym]. 
* This clusters ranks based on the symbol it refers to. 
*/
void Postcoder::Encode(unsigned char* T, int* Freq, int len)
{
    int Bucket[256];
    unsigned char SortedMap[256], S2R[256], R2S[256], sym, rank;
	unsigned char *RankArray; 
	if((RankArray = (unsigned char*)malloc(sizeof(unsigned char) * len)) == NULL) 
		Error("Failed to alloc sorted rank array!");
	memset(Freq, 0, 256 * sizeof(int));
	
	int UniqueSyms = 0;
    for (int i = 0; i < len; i++) 
	{
		sym = T[i];
        if (Freq[sym] == 0) 
		{
            S2R[R2S[UniqueSyms] = sym] = UniqueSyms;
            UniqueSyms++;
        }
        Freq[sym]++;
    }

    GenerateSortedMap(Freq, SortedMap);
    for (int i = 0, BucketPos = 0; i < UniqueSyms; i++) 
	{
        sym = SortedMap[i];
        Bucket[sym] = BucketPos;
        BucketPos += Freq[sym];
    }

    for (int i = 0; i < len; i++) 
	{
        sym = T[i]; 
		rank = S2R[sym]; 
        RankArray[Bucket[sym]++] = rank;
        if (rank > 0)
		{
            do
			{
                S2R[R2S[rank] = R2S[rank - 1]] = rank;
            } while (0 < --rank);
            S2R[R2S[0] = sym] = 0;
        }
    }
	memcpy(&T[0], &RankArray[0], len * sizeof(unsigned char));
	free(RankArray);
}

/**
* Decoding is a little weird, it performs an inverse MTF update while jumping through buckets to restore the original symbol.
* The bucket rank implies the current symbol, and the symbol implies the next bucket to go to.
*/
void Postcoder::Decode(unsigned char* RankArray, int* Freq, int len)
{
    int Bucket[256], BucketEnd[256];
    unsigned char SortedMap[256], R2S[256], sym, rank;
	unsigned char *T;
	if((T = (unsigned char*)malloc(sizeof(unsigned char) * len)) == NULL) 
		Error("Failed to alloc inverse sorted rank array!");

	int total = 0;
	for(int i = 0; i < 256; i++)
		total += Freq[i];
	if(total != len)
		Error("Invalid decoder frequencies!");
	
	int UniqueSyms = 0;
	for(int i = 0; i < 256; i++)
		if(Freq[i] > 0) 
			UniqueSyms++;
	
    GenerateSortedMap(Freq, SortedMap);
    for(int i = 0, BucketPos = 0; i < UniqueSyms; i++) 
	{
        sym = SortedMap[i];
        R2S[RankArray[BucketPos]] = sym;
        Bucket[sym] = BucketPos + 1;
        BucketPos += Freq[sym];
        BucketEnd[sym] = BucketPos;
    }
	
    for(int i = 0, sym = R2S[0]; i < len; i++) 
	{
        T[i] = sym;
        if (Bucket[sym] < BucketEnd[sym])
		{
            if (0 < (rank = RankArray[Bucket[sym]++])) 
			{
                int s = 0;
                do
                    R2S[s] = R2S[s + 1];
                while(++s < rank);
                R2S[rank] = sym;
                sym = R2S[0];
            }
        }
        else if (0 < UniqueSyms--) 
		{
            int s = 0;
            do 
				R2S[s] = R2S[s + 1];
            while(++s < UniqueSyms);
            sym = R2S[0];
        }
    }
	memcpy(&RankArray[0], &T[0], len * sizeof(unsigned char));
	free(T);
}
