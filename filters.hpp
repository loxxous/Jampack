/*********************************************
* Embedded delta and linear prediction filters
* These are blockwise adaptive transforms which lower the entropy of complex semi-static structures
* 
* Definitions of compression arguments:
* -f0 : no filter, do not preprocess.
* -f1 : heuristic filter, detect the width of channels and pick whatever transform is the best, if any.
* -f2 : brute force, apply all possible transformations and pick the best one, repeat until no more configuration helps or until a recursion depth of 4.
*
* There are 3 filter types:
* 1) delta : standard reorder of channels with delta encoder.
* 2) linear prediction : standard reorder of channels with linear prediction encoder.
* 3) inline delta : no reordering, deltas are encoded in place by a table. (context preserving, simd friendly)
* Each filter can encode a width of 1 to 32, width 0 means raw. 
* In total there are 96 filters + raw configuration, in brute force mode it tries all filters and picks the best, in heuristic mode
* it checks for repeated symbols at pos%32 in O(n) time via a stride histogram.
**********************************************/

#ifndef FILTERS_H
#define FILTERS_H

#include "format.hpp"
#include "utils.hpp"

class Filters
{
private:
	const int MAX_SUPPORTED_CONFIGURATION = 3; // Ammount of configurations not including channels (3 because, delta, lpc, and inline delta)
	const int MAX_CHANNEL_WIDTH = 32; // 0 to 32
	const int FILTER_BLOCK_SIZE = 64 << 10;
	
	void DeltaEncode(unsigned char *in, int len);
	void DeltaDecode(unsigned char *in, int len);
	void LpcEncode(unsigned char *in, int len);
	void LpcDecode(unsigned char *in, int len);
	void UpdateWeight(unsigned char err, int *weight);
	void Reorder(unsigned char *in, unsigned char *out, int width, int len);
	void Unreorder(unsigned char *in, unsigned char *out, int width, int len);
	void InlineDelta(unsigned char *in, unsigned char *out, int width, int len);
	void InlineUndelta(unsigned char *in, unsigned char *out, int width, int len);
	int FindStride(unsigned char *in, int len);
	int FindProjection(unsigned char *in, int len);
public:
	void Encode(Buffer Input, Buffer Output, Options Opt);
	void Decode(Buffer Input, Buffer Output);
};
#endif // FILTERS_H
