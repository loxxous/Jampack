/*********************************************
* Full compression and decompression code in human friendly form
**********************************************/
#ifndef JAM_H
#define JAM_H

#include "format.hpp"
#include "lz77.hpp"
#include "bwt.hpp"
#include "ans.hpp"
#include "checksum.hpp"

class Jampack
{
	public:
	Buffer Input;
	Buffer Output;
	
	private:
	BlockSort::BWT *bwt;
	Lz77 *lz;
	ANS *entropy;
	Index BlockSize;
	Index *Indices;
	Checksum *Chk;
	int Threads;
	unsigned char cmethod;
	unsigned char fmethod;
	unsigned int crc;
	
	public:
	void Comp(); // Compress buffer
	void Decomp(); // Decompress buffer
	void InitComp(int bsize); // initialize compressor with blocksize bsize
	void InitDecomp(int t); // Initialize decoder with t threads
	void Free(); // Release memory from the compressor or decompressor
	
	int CompReadBlock(FILE *in); // Read raw input to compressor
	int DecompReadBlock(FILE *in); // Read compressed block to decompressor
	void CompWriteBlock(FILE *out); // Write compressed contents to output
	void DecompWriteBlock(FILE *out); // Write out extracted data
	
	void DisplayHeaderContents(); // Only really used for debugging
	
	void Compress(FILE *in, FILE *out, int t, int bsize); // Compress input file to output file with t threads and blocksize bsize
	void Decompress(FILE *in, FILE *out, int t); // Decompress input to output with t internal threads
};
#endif // JAM_H //
