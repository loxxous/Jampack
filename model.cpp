#include "model.hpp"

/**
* Return row of element within a malloc'd 1D array as 2D
*/
inline int* AdaptiveModel::Ptr1Dto2D(int *ptr, int x, int xMax)
{
	return &ptr[x * xMax];
}

AdaptiveModel::AdaptiveModel(int Alpha)
{	
	AlphabetSize = (int*)malloc(1 * sizeof(int));
	if(Alpha <= 0)
		Error("Alphabet size must be at least 1!");
	*AlphabetSize = Alpha;
	
	Mix = (int*)malloc((*AlphabetSize * (*AlphabetSize + 1)) * sizeof(int)); 
	CumFreqs = (int*)malloc((*AlphabetSize + 1) * sizeof(int)); 
	
	if(Mix == NULL) 
		Error("Failed to allocate mixing table!");
	if(CumFreqs == NULL) 
		Error("Failed to allocate cumulative table!");
}

AdaptiveModel::~AdaptiveModel()
{	
	free(AlphabetSize);
	free(Mix);
	free(CumFreqs);
}

unsigned int AdaptiveModel::SymToLow(unsigned short sym)
{
	return CumFreqs[sym];
}

unsigned int AdaptiveModel::SymToFreq(unsigned short sym)
{
	return CumFreqs[sym + 1] - CumFreqs[sym];
}

unsigned short AdaptiveModel::RangeToSym(unsigned int range)
{
	// Linear search
	for(int i = 0; i < *AlphabetSize; i++)
	{
		if(CumFreqs[i] <= range && range < CumFreqs[i + 1])
			return i;
	}
	Error("Range to symbol failure!");
	return 0;
}

/**
* CDF transformation-style update (always remains a total of ProbScale)
* As long as both CDF's have non-zero probabilities and are both a signed type then it'll mix just fine
*/
void AdaptiveModel::Update(int symbol)
{
	/*
	// Only for fixed sized alphabets
	__m128i CDF1 = _mm_loadu_si128((__m128i*)&CumFreqs[0]);
	__m128i CDF2 = _mm_loadu_si128((__m128i*)&CumFreqs[4]);
	__m128i MIX1 = _mm_loadu_si128((__m128i*)&Mix[symbol][0]);
	__m128i MIX2 = _mm_loadu_si128((__m128i*)&Mix[symbol][4]);
	CDF1 = _mm_add_epi32(_mm_srai_epi32(_mm_sub_epi32(MIX1, CDF1), Rate), CDF1);
	CDF2 = _mm_add_epi32(_mm_srai_epi32(_mm_sub_epi32(MIX2, CDF2), Rate), CDF2);
	_mm_storeu_si128((__m128i *)&CumFreqs[0], CDF1);
	_mm_storeu_si128((__m128i *)&CumFreqs[4], CDF2);
	*/
	
	int *MixRow = Ptr1Dto2D(Mix, symbol, *AlphabetSize + 1); // *t+(row*width)+column
	for(int i = 1; i < *AlphabetSize; i++)
        CumFreqs[i] += (MixRow[i] - CumFreqs[i]) >> Rate;
}

/**
* Reinitialize the state of the model back to equal probabilities
* Build mixing table
*/
void AdaptiveModel::Reset()
{	
	int *freqs = (int*)malloc(*AlphabetSize * sizeof(int));		
	int scale = ProbScale / *AlphabetSize;
	for(int i = 0; i < *AlphabetSize; i++)
		freqs[i] = scale;
	
	int ActualScale = scale * *AlphabetSize;
	freqs[0] += ProbScale - ActualScale;
	
	memset(CumFreqs, 0, (*AlphabetSize + 1) * sizeof(unsigned int)); 
	for(int i = 0; i < *AlphabetSize; i++) 
		CumFreqs[i + 1] = CumFreqs[i] + freqs[i];
	assert(CumFreqs[*AlphabetSize] == ProbScale);
	free(freqs);
	
	for(int sym = 0; sym < *AlphabetSize; sym++)
	{
		int rm = 0;
		int *MixRow = Ptr1Dto2D(Mix, sym, *AlphabetSize + 1);
		for(int state = 0; state <= *AlphabetSize; state++)
		{
			MixRow[state] = rm;
			if(state == sym)
				rm += ProbScale - *AlphabetSize + 1; 
			else
				rm += 1;
		}
		assert(MixRow[*AlphabetSize] == ProbScale);
	}
}

QuasiModel::QuasiModel(int Alpha)
{	
	AlphabetSize = (int*)malloc(1 * sizeof(int));
	if(Alpha <= 0)
		Error("Alphabet size must be at least 1!");
	*AlphabetSize = Alpha;
	
	Freqs = (int*)malloc((*AlphabetSize) * sizeof(int)); 
	CumFreqs = (int*)malloc((*AlphabetSize + 1) * sizeof(int)); 
	
	if(Freqs == NULL) 
		Error("Failed to allocate frequency table!");
	if(CumFreqs == NULL) 
		Error("Failed to allocate cumulative table!");
}

QuasiModel::~QuasiModel()
{	
	free(AlphabetSize);
	free(Freqs);
	free(CumFreqs);
}

unsigned int QuasiModel::SymToLow(unsigned short sym)
{
	return CumFreqs[sym];
}

unsigned int QuasiModel::SymToFreq(unsigned short sym)
{
	return CumFreqs[sym + 1] - CumFreqs[sym];
}

/**
* Fast symbol to range, with binary searches we'd require up to ceil(log2(alpha))*n operations per block to get all symbols.
* But with simple array mappings we need at most n+k operations per block, this is typically more efficient due to less operation dependencies.
*/
unsigned short QuasiModel::RangeToSym(unsigned int range)
{
	return RangeToSymbol[range];
}

/**
* Increment symbol freq, conditionally update (stretch and rescale) the model if we've seen enough symbols
*/
void QuasiModel::Update(int symbol)
{
	Freqs[symbol] += ProbBits;
	SEEN++;
	if(SEEN > EXP)
	{
		// New cumulative model
		int Total = 0;
		int Log = 0;
		for(int i = 0; i < *AlphabetSize; i++)
			Total += Freqs[i];
		
		// Scale down
		while(((Total >> Log) + *AlphabetSize) > ProbScale)
			Log++;
		
		// All symbols will now sum to less than ProbScale and be assigned a value of at least one
		Total = 0;
		for(int i = 0; i < *AlphabetSize; i++)
			Total += Freqs[i] = (Freqs[i] >> Log) + 1;	
		
		// Stretch up
		for(int i = 0; i < *AlphabetSize; i++)
			Freqs[i] = ProbScale * Freqs[i] / Total;

		Total = 0;
		for(int i = 0; i < *AlphabetSize; i++)
			Total += Freqs[i];
		Freqs[0] += ProbScale - Total; // Fill the entire range if there's a remainder (this is usually the best symbol)
		
		memset(CumFreqs, 0, (*AlphabetSize + 1) * sizeof(int)); 
		for(int i = 0; i < *AlphabetSize; i++) 
			CumFreqs[i + 1] = CumFreqs[i] + Freqs[i];
		assert(CumFreqs[*AlphabetSize] == ProbScale);
		
		memset(Freqs, 0, *AlphabetSize * sizeof(int)); 
		
		for(int sym = 0; sym < *AlphabetSize; sym++)
			for(unsigned int i = CumFreqs[sym]; i < CumFreqs[sym + 1]; i++)
				RangeToSymbol[i] = sym;
		
		SEEN = 0;
		EXP = (EXP < UPDATE_RATE) ? EXP << 1 : UPDATE_RATE;
	}
}

/**
* Reinitialize the state of the model back to equal probabilities
*/
void QuasiModel::Reset()
{
	memset(Freqs, 0, *AlphabetSize * sizeof(int)); 
	memset(CumFreqs, 0, (*AlphabetSize + 1) * sizeof(int));
	SEEN = 0;
	EXP = 8;
	
	int scale = ProbScale / *AlphabetSize;
	for(int i = 0; i < *AlphabetSize; i++)
		Freqs[i] = scale;
	
	int ActualScale = 0;
	for(int i = 0; i < *AlphabetSize; i++)
		ActualScale += Freqs[i];
	Freqs[0] += ProbScale - ActualScale; // Accommodate for any integer division errors
		
	memset(CumFreqs, 0, (*AlphabetSize + 1) * sizeof(int)); 
	for(int i = 0; i < *AlphabetSize; i++) 
		CumFreqs[i + 1] = CumFreqs[i] + Freqs[i];

	assert(CumFreqs[*AlphabetSize] == ProbScale);
	memset(Freqs, 0, *AlphabetSize * sizeof(int)); 
	
	for(int sym = 0; sym < *AlphabetSize; sym++)
        for(unsigned int i = CumFreqs[sym]; i < CumFreqs[sym + 1]; i++)
            RangeToSymbol[i] = sym;
}
