/*********************************************
*	CUMULATIVE FREQUENCY MODEL
*
* 	Handles all the range to symbol mappings.
**********************************************/

#ifndef MODEL_H
#define MODEL_H

struct Model
{
    static const uint32_t ProbBits = 14;
    static const uint32_t ProbScale = 1 << ProbBits;
	
	unsigned int Freqs[256];			// Recent freqs
	unsigned int CumFreqs[257];	// Encode table
	
	void Init();
	void UpdateModel();
	int GetRange(byte sym);	// sym->range O(1)
	byte GetSym(int range);		// range->sym O(logn)
};

/*
	Intialize ranges to be of equal probability
*/
void Model::Init()
{
	int iprob = ProbScale >> 8;
	for (int i = 0; i < 256; i++) Freqs[i] = iprob;	
    for (int i=0; i < 256; i++) CumFreqs[i+1] = CumFreqs[i] + Freqs[i];
}

/*
	This function updates the encode table using the new counts.
	While the sum of the frequencies is greater than our target of 2^n, reduce the frequencies by 0.5 and keep non-zeros.
	Fit these newly scaled probabilities into their respective ranges from 0 to 2^n.
*/
void Model::UpdateModel()
{
	int sum;
	do
	{
		sum = 0;
		for (int i = 0; i < 256; i++) 
		{
			Freqs[i] = (1 + Freqs[i]) >> 1;
			sum += Freqs[i];
		}
	}while(sum > ProbScale);

	int total = 0;
	CumFreqs[0] = 0;
    for (int i=0; i < 256; i++)
	{
		Freqs[i] = ((uint64_t)ProbScale * Freqs[i]) / sum;
		total += Freqs[i];
	}
	
	assert(total == ProbScale); // make sure the ranges scaled properly

    for (int i=0; i < 256; i++) CumFreqs[i+1] = CumFreqs[i] + Freqs[i]; // set the new cumulative table
}

/*
	if (c != p)
	{
		if (c != 0) r = c;
		else r = p;
	}
	gets converted into
	r = -(c != p) & (((c == 0) & (p - c)) + c);
*/
byte Model::GetSym(int Range, int Threshold)
{
	int Pivot = 0xff >> 1;
	byte Sym = Pivot;
	
	for(int i = 0; i < 8; i++) // basic binary search
	{
		if(CumFreqs[Sym] <= Range)
		{
			Sym +=  Pivot >>= 1;
		}
		else
		{
			Sym -= Pivot >>= 1;
		}
	}
	
	//Sym = -(CumFreqs[Sym] <= Range) & (Sym ) + Pivot >> 1; // branchless binary search, if 1 increment, if 0 decrement.
	
	Freqs[Sym] += AdaptionRate;
	if(Freqs[Sym] > Threshold) UpdateModel();
	return Sym;
}

int Model::GetRange(byte Sym, int Threshold)
{
	int Range = CumFreqs[Sym];
	Freqs[Sym] += AdaptionRate;
	if(Freqs[Sym] > Threshold) UpdateModel();
	return Range;
}
#endif // MODEL_H