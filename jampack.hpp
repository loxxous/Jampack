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
#include "filters.hpp"
#include "lpx.hpp"

class Jampack
{
	public:
	Buffer Input;
	Buffer Output;
	
	private:
	BlockSort::Bwt *Bwt;			// Burrows wheeler transform
	Lz77 *Lz;				// Lz77 codec for special cases
	Ans *Entropy;				// Structured rANS coder
	Filters *Filter;			// Generic filters 
	Lpx *LocalModel;			// Local bijective low-order match model
	Checksum *Chk;				// Fast checksum implementation
	Options Option;				// Optional compression arguments are passed through the 'Options' type
	Index BlockSize;
	unsigned int crc;
	
	public:
	void Comp(); 				// Compress buffer
	void Decomp(); 				// Decompress buffer
	void InitComp(Options Opt); 		// Initialize compressor with blocksize bsize
	void InitDecomp(Options Opt); 		// Initialize decoder with t threads on either cpu or gpu
	void Free(); 				// Release memory from the compressor or decompressor
	
	int CompReadBlock(FILE *in);		// Read raw input to compressor
	int DecompReadBlock(FILE *in); 		// Read compressed block to decompressor
	void CompWriteBlock(FILE *out); 	// Write compressed contents to output
	void DecompWriteBlock(FILE *out); 	// Write out extracted data
	
	void SwapStreams();			// Swap input stream with output stream
	void DisplayHeaderContents(); 		// Only really used for debugging
	
	void Compress(FILE *in, FILE *out, Options Opt); 	// Compress input file to output file
	void Decompress(FILE *in, FILE *out, Options Opt); 	// Decompress input to output 
};
#endif // JAM_H //
