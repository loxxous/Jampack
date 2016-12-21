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

/*
    // Compute statistics
    hist8(in, in_size, F);
    double p = (double)TOTFREQ/(double)in_size;

    // Normalise so T[i] == TOTFREQ
    for (m = M = j = 0; j < 256; j++) {
	if (!F[j])
	    continue;

	if (m < F[j])
	    m = F[j], M = j;

	if ((F[j] = F[j]*p+0.499) == 0)
	    F[j] = 1;
	fsum += F[j];
    }

    fsum++; // not needed, but can't remove without removing assert x<TOTFREQ (in old code)
    int adjust = TOTFREQ - fsum;
    if (adjust > 0) {
	F[M] += adjust;
    } else if (adjust < 0) {
	if (F[M] > -adjust) {
	    F[M] += adjust;
	} else {
	    adjust += F[M]-1;
	    F[M] = 1;
	    for (j = 0; adjust && j < 256; j++) {
		if (F[j] < 2) continue;

		int d = F[j] > -adjust;
		int m = d ? adjust : 1-F[j];
		F[j]   += m;
		adjust -= m;
	    }
	}
    }
*/

#endif // MODEL_H