/*********************************************
* Useful generic function class
* Contains:
* 1) ???
* 2) block entropy calculator
* 3) compressed integer read/write in LEB128 with carry (optimal) and carryless byte prefixes (fast)
**********************************************/
#ifndef UTILS_H
#define UTILS_H

#include "format.hpp"
#include "rank.hpp"

class Utils
{	
	private:
	Postcoder *Rank;
	
	const Index constants[4] = 
	{0xff >> 1, (0xffff >> 2) + (0xff >> 1), 
	(0xffffff >> 3) + (0xffff >> 2) + (0xff >> 1), 
	(0xffffffff >> 4) + (0xffffff >> 3) + (0xffff >> 2) + (0xff >> 1)};

	static const int EntScale = 1 << 16;
	double EntLog[EntScale] = {0};
	
	public: 
	
	Utils();
	~Utils();
	
	/**
	* Return code space for a specific value
	*/
	Index SizeOfValue(Index val);
	
	/**
	* Write compressed integer in LEB128 with carry into buffer.
	* Returns new position in the buffer.
	*/
	Index EncodeLeb128(Index val, unsigned char *buf);

	/**
	* Read compressed integer in LEB128 with carry. Returns new position in the buffer and value.
	*/
	Index DecodeLeb128(Index *valptr, unsigned char *buf);

	/**
	* Write compressed integer in byte prefix codes without carry into buffer.
	* Returns new position in the buffer.
	*/
	Index EncodeVaInt(Index val, unsigned char *buf);
	
	/**
	* Read byte prefix codes from buffer. Returns new position in the buffer and value.
	*/
	Index DecodeVaInt(Index *valptr, unsigned char *buf);
	
	/**
	* Find the entropy of a block in memory
	*/
	double CalculateMixedEntropy(unsigned char *ptr, Index len);
	double CalculateSortedEntropy(unsigned char *ptr, Index len);
	double CalculateEntropy(unsigned char *ptr, Index len);
	double CalculateO1Entropy(unsigned char *ptr, Index len);
};
#endif // UTILS_H