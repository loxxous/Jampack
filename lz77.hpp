/*********************************************
* LZ77 COMPRESSION AND DECOMPRESSION CODE
**********************************************/

#ifndef LZ77_H
#define LZ77_H

#include "format.hpp"

class Lz77
{
public:
	void compress(Buffer Input, Buffer Output, int minmatch);
	void decompress(Buffer Input, Buffer Output);
private:
	int encode_leb128(int look_ahead, int match_length, int offset, unsigned char *buf);
	int decode_leb128(int *look_ahead, int *match_length, int *offset, unsigned char *buf);
	bool compressible(int look_ahead, int match_len, int offset, int minmatch);
	const unsigned int constants[3] = { 0xff >> 1, (0xffff >> 2) + (0xff >> 1), (0xffffff >> 3) + (0xffff >> 2) + (0xff >> 1) };
	const unsigned int MAX_RUN = (1 << 28) - 1;
	
	inline int HashLong(int ctx);
	inline int Hash32(unsigned char *p);
	
	// Basic structure for comparing large contexts
	struct MatchLong
	{
		int pos;
		int ctx;
	};
};
#endif // LZ77_H
