/*********************************************
* Cumulative Frequency and Statistical Model
* Notes: order-1 makes pretty much no difference on BWT+MTF+RLE0 data, fast adapting order-0 is best due to the skewed distribution.
**********************************************/

#ifndef MODEL_H
#define MODEL_H

#include "rans_byte.hpp"
#include "format.hpp"

class Model
{
public:
    static const unsigned int ProbBits = 15; // Max accuracy, 16 can cause overflow in 16 bit type due to 1 << 16
    static const unsigned int ProbScale = 1 << ProbBits;
	
private:
	static const unsigned int UpdateStep = 32 << 10;
	static const unsigned int Order_1_States = 1;
	static const unsigned int Order_0_State = Order_1_States - 1;
	static const unsigned int AlphabetSize = 257;
	
	unsigned int Freqs[Order_1_States][AlphabetSize] = {{0}};
	unsigned int PreInheritedFreqs[Order_1_States][AlphabetSize] = {{0}};
	unsigned int CumFreqs[Order_1_States][AlphabetSize + 1] = {{0}};
	unsigned int Chain_1 = 0;
	
	unsigned short cum2sym[Order_1_States][ProbScale];
	RansEncSymbol esyms[Order_1_States][AlphabetSize];
	RansDecSymbol dsyms[Order_1_States][AlphabetSize];
	void StretchAndFitFreqs();
	void InheritStatistics();

public:
	int WriteHeader(unsigned char* outbuf, int* olen, int* clen, int* rlen);
	int ReadHeader(unsigned char* inbuf, int* olen, int* clen, int* rlen, int StackSize);
	void Build(unsigned short *buf, int len);
	void Clear();
	void SetEncodeTable();
	void SetDecodeTable();
	RansEncSymbol EncGetRange(unsigned short c);
	RansDecSymbol* DecGetNext(unsigned short c);
	int DecGetSym(int range);
};

#endif // MODEL_H 