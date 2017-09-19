/*********************************************
* Cyclic hashed history model
*
* This is a memory efficient way of tracking the probability history of arbitrarily large values in O(n) space and O(1) time.
* ModDensity figures out common multiples of all offsets so we can utilize it for more accurate parsing.
*
* Update(Word); 				// Update cyclic stack with new symbol, remove oldest symbol, update histogram
* GetHistory(Word); 			// Access histogram
* FindPeaks(Word, limit); 		// Compare word to a limited level of skewness
**********************************************/

#include "cyclichhm.hpp"

/**
* Create a circular buffer of a certain width (width determines how many elements we can store before overwriting old elements)
*/
CyclicHashHistory::CyclicHashHistory(int size)
{
	if(size < 0)
		Error("Cannot initialize a cyclic hash history model of a negative size!");
	
	SizeOfCB = size;
	PosInCB = 0;
	PreviousValue = 0;
	
	CircularBuffer = (unsigned short*)calloc(SizeOfCB, sizeof(unsigned short));
	History = (unsigned int*)calloc(HashSize, sizeof(unsigned int));
	ModDensity = (unsigned int*)calloc(ModSize, sizeof(unsigned int));
	
	if(History == NULL || CircularBuffer == NULL || ModDensity == NULL)
		Error("Failed to allocate cyclic model buffers!");
}

CyclicHashHistory::~CyclicHashHistory()
{
	free(CircularBuffer);
	free(History);
	free(ModDensity);
}

inline unsigned int CyclicHashHistory::Hash(Index value)
{
	return (value * 0x9E3779B1) >> (32 - HashBits);
}

/**
* Stack the value into the next position
*/
void CyclicHashHistory::Update(Index value)
{
	unsigned int h = Hash(value);
	unsigned int oldh = CircularBuffer[PosInCB % SizeOfCB];

	// Add element to buffer and increment the hash histogram
	CircularBuffer[PosInCB % SizeOfCB] = h;
	History[h]++;
		
	// Remove the overwritten element from the hash histogram
	if(PosInCB >= SizeOfCB)
		History[oldh]--;
	
	// Compute a histogram of common multiples
	unsigned int diff = PreviousValue ^ value;
	ModDensity[diff % ModSize]++;
	
	PosInCB++;
}

/**
* Return the histogram associated with the current value's statistics within the circular buffer
*/
unsigned int CyclicHashHistory::GetHistory(Index value)
{
	return History[Hash(value)];
}

/**
* Make sure nothing fucked up
*/
void CyclicHashHistory::Assert()
{
	if(PosInCB >= SizeOfCB)
	{
		int total = 0;
		for(int i = 0; i < HashSize; i++)
			total += History[i];
		if(total != SizeOfCB)
		{
			printf("Hist size: %i, expected: %i\n", total, SizeOfCB);
			Error("Invalid history table!");
		}
	}
}

/**
* Reduce value until we find peaks within the history buffer of direct subsets.
* Return true if the value within histogram is greater than the limit, else return false.
*/
bool CyclicHashHistory::FindPeaks(Index value)
{
	// Decompose value into subsets, if the structure is highly skewed return true, else if the history of the current value is common; return true.
	int k = value;
	int reduce = (StructureWidth <= 1) ? 2 : StructureWidth;
	while(k)
	{
		//int MaxDensity = ModDensity[StructureWidth];
		int div = (AverageDensity == 0) ? 1 : AverageDensity;
		if(ModDensity[k % ModSize] > (UniqueDensities / (div * div)))  // A simple hueristic for anti-context parsing, generally very accurate (low noise)
			return true;
		k /= reduce;
	}
	return false;
}

/**
* TODO: AverageDensity is actually pointless, the best thing to do is create an inverse sorted map for ranks.
* eg: if(rank[value % size] < 128)
*/
void CyclicHashHistory::BuildModel()
{
	// Find the most dense region (this helps us find underlying structures)
	AverageDensity = 0;
	int zeros = 0;
	for (int j = 0; j < ModSize; j++)
	{
		AverageDensity += ModDensity[j];
		if(ModDensity[j] == 0) zeros++;
	}
	if(ModSize > zeros)
		AverageDensity /= (ModSize - zeros);
	
	UniqueDensities = ModSize - zeros;
	
	int max = ModDensity[0];
	int bsym = 0;

	for(int i = 1; i < ModSize; i++)
	{
		int cur = ModDensity[i];
		if(cur > max)
		{
			bsym = i;
			max = cur;
		}
	}
	if(bsym == 0)
		StructureWidth = 1;
	else
		StructureWidth = bsym;
}

void CyclicHashHistory::CleanModel()
{
	AverageDensity = 0;
	for(int i = 0; i < ModSize; i++)
	{
		ModDensity[i] = 0;
	}
	StructureWidth = 1;
}