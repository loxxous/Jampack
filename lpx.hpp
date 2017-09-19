/*********************************************
* Localized prefix model
**********************************************/

#ifndef PREFIX_H
#define PREFIX_H

#include "format.hpp"
#include "utils.hpp"

class Lpx
{
private:
	const int MaxThreshold = 128; // Consecutive exact matches for a leading prefix
	const int MinThreshold = 4;
	const unsigned int MaxRecordSize = 64 << 10; // Window for prefix to suffix correllations
	
	struct PrefixRecord
	{
		unsigned int cxt;
		unsigned int pos;
		unsigned int hits;
		unsigned int miss;
		int threshold;
	};
	
	void UpdateTable(PrefixRecord **table, unsigned int *cxt, unsigned int pos, unsigned char *order);
	void EncodeBlock(unsigned char *input, unsigned char *output, Index len);
	void DecodeBlock(unsigned char *input, unsigned char *output, Index len);
	void ReverseBlock(unsigned char *input, Index len);
	
public:
	void Encode(Buffer Input, Buffer Output, Options Opt);
	void Decode(Buffer Input, Buffer Output, Options Opt);
};
#endif // PREFIX_H
