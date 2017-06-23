/*********************************************
* LZ77 compression and decompression header
**********************************************/

#ifndef LZ77_H
#define LZ77_H

#include "format.hpp"
#include "utils.hpp"

class Lz77
{
public:
	void Compress(Buffer Input, Buffer Output, int minmatch);
	void Decompress(Buffer Input, Buffer Output);
private:
	bool Compressible(int look_ahead, int match_len, int offset, int minmatch);
	const unsigned int constants[4] = 
	{0xff >> 1, (0xffff >> 2) + (0xff >> 1), 
	(0xffffff >> 3) + (0xffff >> 2) + (0xff >> 1), 
	(0xffffffff >> 4) + (0xffffff >> 3) + (0xffff >> 2) + (0xff >> 1)};
	
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
