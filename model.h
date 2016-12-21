/*********************************************
*	CUMULATIVE FREQUENCY MODEL
**********************************************/
#ifndef MODEL_H
#define MODEL_H

struct Model
{
public:
	int AlphabetSize = 256;
    static const uint32_t ProbBits = 16;
    static const uint32_t ProbScale = 1 << ProbBits;
	unsigned int Freqs[256] = {0};
	unsigned int CumFreqs[257] = {0};
	void Init();
	void UpdateModel();
	void ResetFreqs();
	void ReduceFreqs();
};

/*
	Intialize ranges to be of equal probability
*/
void Model::Init()
{
	int total = 0;
	for (int i = 0; i < 256; i++) total += Freqs[i] = (ProbScale / AlphabetSize);	
	int remainder = ProbScale - total;
	if(remainder > 0)
	{
		Freqs[0] += remainder;
		total += remainder;
	}
	assert(total == ProbScale);
    for (int i=0; i < 256; i++) CumFreqs[i+1] = CumFreqs[i] + Freqs[i];
}

void Model::ResetFreqs()
{
	for (int i=0; i < 256; i++) Freqs[i] = 1;
}

void Model::ReduceFreqs()
{
	for (int i=0; i < 256; i++) Freqs[i] = (1 + Freqs[i]) >> 1;
}

/*
	This function updates the encode table using the new counts.
	While the sum of the frequencies is greater than our target of 2^n, reduce the frequencies by half and keep non-zeros.
	Fit these newly scaled probabilities into their respective ranges from 0 to 2^n.
	After scaling there's usually a remainder that needs to be filled, so it's assigned to the highest probability symbol.
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
	int max = 0;
	int bsym = 0;
	CumFreqs[0] = 0;
	for (int i=0; i < 256; i++)
	{
		total += Freqs[i] = round((ProbScale * Freqs[i]) / sum);
		if(max < Freqs[i]) { max = Freqs[i]; bsym = i; }
	}
	int remainder = ProbScale - total;
	if(remainder > 0)
	{
		Freqs[bsym]+=remainder;
		total += remainder;
	}
	assert(total == ProbScale);
	for (int i=0; i <= 256; i++) { CumFreqs[i] = 0; }
	for (int i=0; i < 256; i++) CumFreqs[i+1] = CumFreqs[i] + Freqs[i]; // set the new cumulative table
}

#endif // MODEL_H
