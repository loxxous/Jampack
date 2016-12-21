/*********************************************
*	FIFO ANS COMPRESSION AND DECOMPRESSION CODE
**********************************************/
#ifndef RANS_H
#define RANS_H

#include "rans_byte.h"
#include "model.cpp"

struct RansSymbol 
{
  uint16_t start; 		// start of symbol interval
  uint16_t range; 	// size of symbol interval
};

class BufferedRansEncoder 
{
	std::vector<RansSymbol> syms; // or similar

public:
	void encode(uint16_t start, uint16_t range)
	{
		assert(range >= 1);
		assert(start + range <= 0x10000); // no wrap-around

		RansSymbol sym = { start, range };
		syms.push_back(sym);
	}

	void flush_to(RansEncoder &coder);
};

void BufferedRansEncoder::flush_to(RansEncoder &coder)
{
	// Replays the buffered symbols in reverse order to
	// the actual encoder.
	while (!syms.empty()) 
	{
		RansSymbol sym = syms.back();
		coder.encode(sym.start, sym.range);
		syms.pop_back();
	}
}

class ANS
{
public:
	void enc(byte *inbuf, byte* outbuf, int in_size, int out_size);
	void dec(byte *inbuf, byte* outbuf, int in_size, int out_size);
};

void ANS::enc(byte *inbuf, byte* outbuf, int in_size, int out_size)
{

}

void ANS::dec(byte *inbuf, byte* outbuf, int in_size, int out_size)
{

}

#endif // RANS_H