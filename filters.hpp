/*********************************************
* Embedded delta and linear prediction filters 
**********************************************/

#ifndef FILTERS_H
#define FILTERS_H

#include "format.hpp"
#include "utils.hpp"

class Filters
{	
private:
	const int FilterBlockSize = 16 << 10;
	const int MaxChannelWidth = 32; // 0 to 32
	void DeltaEncode(unsigned char *in, int len);
	void DeltaDecode(unsigned char *in, int len);
	void LpcEncode(unsigned char *in, int len);
	void LpcDecode(unsigned char *in, int len);
	void UpdateWeight(unsigned char err, int *weight);
	void Reorder(unsigned char *in, unsigned char *out, int width, int len);
	void Unreorder(unsigned char *in, unsigned char *out, int width, int len);
	Index EncodeChunk(unsigned char *in, unsigned char *out, int bufferedChannel, int bufferedType, int bufferedLength);
	
public:
	void Encode(Buffer Input, Buffer Output);
	void Decode(Buffer Input, Buffer Output);
};
#endif // FILTERS_H