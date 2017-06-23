/*********************************************
* Statistical model
* This file contains two model classes: one fast adapting and one slow adapting.
* The fast model relies on probabilities getting updated at adapting rates. (CDF transformation based)
* The slow model is more traditional, it starts with initial distribution every now and then updates the table to the real probability. (Quasi-static)
**********************************************/

#ifndef MODEL_H
#define MODEL_H

#include "format.hpp"

/**
* Fast adaptive model performs alphabet-wise rescaling very frequently, normally this is done with OoOE or SSE so it's quite efficient.
*/
class AdaptiveModel
{
	public:
	AdaptiveModel(int Alpha);
	~AdaptiveModel();
	
	int *AlphabetSize;
    static const unsigned int ProbBits = 16;
    static const unsigned int ProbScale = 1 << ProbBits;
	
	void Update(int symbol);
	void Reset();
	unsigned int SymToLow(unsigned short sym);
	unsigned int SymToFreq(unsigned short sym);
	unsigned short RangeToSym(unsigned int range);

	private:
	inline int* Ptr1Dto2D(int *ptr, int x, int xMax);
	int Rate = 5;
	int *Mix;
	int *CumFreqs;
};

/**
* Quasi-static model performs alphabet-wise rescaling once per n-symbols
*/
class QuasiModel
{
	private:
	int *AlphabetSize;
	static const int UPDATE_RATE = 64 << 10;
	int SEEN = 0;
	int EXP = 8;
public:
	QuasiModel(int Alpha);
	~QuasiModel();

    static const unsigned int ProbBits = 16;
    static const unsigned int ProbScale = 1 << ProbBits;
	int *Freqs;
	int *CumFreqs;
	unsigned short RangeToSymbol[ProbScale] = {0};
	void Update(int symbol);
	void Reset();
	unsigned int SymToLow(unsigned short sym);
	unsigned int SymToFreq(unsigned short sym);
	unsigned short RangeToSym(unsigned int range);
};

#endif // MODEL_H 