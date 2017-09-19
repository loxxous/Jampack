/*********************************************
* Lz77 compression and decompression header
*
* In Jampack lz77 is used as a precompressor for complex data,
* it first finds all the best matches and buffers them up in 64 KB chunks,
* then it performs a heuristic analysis on the parsed tokens to find the representaition 
* which doesn't hurt context before encoding.
**********************************************/

#ifndef LZ77_H
#define LZ77_H

#include "format.hpp"
#include "divsufsort.hpp"
#include "utils.hpp"
#include "cyclichhm.hpp"

class Lz77
{
public:
	Lz77();
	~Lz77();
	void Compress(Buffer Input, Buffer Output, Options Opt);
	void Decompress(Buffer Input, Buffer Output);
private:
	Utils *Leb;
	void FastCopy(unsigned char *dest, unsigned char *src, Index length);
	void FastCopyOverlap(unsigned char *dest, unsigned char *src, Index length);
	float Compressible(Index match, Index literal, Index offset);
	Index WriteToken(unsigned char *out, Index match, Index literal, Index offset);
	Index ReadToken(unsigned char *in, Index *match, Index *literal, Index *offset);
	inline unsigned int Load32(unsigned char *ptr);
	inline unsigned int Hash32(unsigned char *ptr);
	inline unsigned int Hash(unsigned int v);
	const int MIN_MATCH = 4;
	const int DUPE_MATCH = 256;
	const int TOKEN_BUFFER_BITS = 16; 
	const int TOKEN_BUFFER_SIZE = 1 << TOKEN_BUFFER_BITS; // 64 KB token chunk
	const int HASH_BITS = 22;
	const int HASH_SIZE = 1 << HASH_BITS;
};
#endif // LZ77_H
