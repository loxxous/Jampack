/*********************************************
*	ANS COMPRESSION AND DECOMPRESSION
**********************************************/
#ifndef ANS_H
#define ANS_H

#include "rans_byte.h"
#include "model.h"

class ANS
{
public:
	void encode(byte *inbuf, byte* outbuf, int *in_size, int *out_size);
	void decode(byte *inbuf, byte* outbuf, int *in_size, int *out_size);
private:
	int StackSize = 128 << 10; 	// Desired block size
	int SBufSize = (StackSize * 2); // Size of temporary buffer prior to merging
};

void ANS::encode(byte *inbuf, byte* outbuf, int *in_size, int *out_size)
{
	Model stats;
	RansEncSymbol *stack = (RansEncSymbol*)calloc(StackSize, sizeof(RansEncSymbol));
	byte *tmp = (byte*)calloc(SBufSize, sizeof(byte));
	int in_p = 0;
	int out_p = 0;

	for(; in_p < *in_size; )
	{
		// Gather statistics for the block
		int len = ((in_p+StackSize) < *in_size) ? StackSize : (*in_size - in_p);
		stats.Build(&inbuf[in_p], len);
		stats.SetEncodeTable();
		
		int sptr = 0;
		for(; sptr < len; sptr++)
			stack[sptr] = stats.EncGetRange((int)inbuf[in_p+sptr]);			
		
		RansState rans;
		RansEncInit(&rans);
		uint8_t *rans_begin;
		uint8_t* ptr = tmp+SBufSize; 	// *end* of temporary buffer
		for (size_t i=sptr; i > 0; i--) // working in reverse!
			RansEncPutSymbol(&rans, &ptr, &stack[i-1]);

		RansEncFlush(&rans, &ptr);
		rans_begin = ptr;
		int csize = &tmp[SBufSize] - rans_begin;
		
		out_p += stats.WriteHeader(&outbuf[out_p], &len, &csize);
		
		// Merge the buffer to the output stream
		for(int k = 0; k < csize; k++) 
			outbuf[out_p+k] = rans_begin[k];

		out_p += csize;
		in_p += len;
		stats.Clear();
	}
	
	*out_size = out_p;
	
	free(stack);
	free(tmp);
}

void ANS::decode(byte *inbuf, byte* outbuf, int *in_size, int *out_size)
{
	Model stats;
	int in_p = 0;
	int out_p = 0;
	
	for(; in_p < *in_size; )
	{
		int olen = 0;
		int clen = 0;
		in_p += stats.ReadHeader(&inbuf[in_p], &olen, &clen, StackSize);		
		stats.SetDecodeTable();
		
		uint8_t *rans_begin = &inbuf[in_p];
		uint8_t* ptr = rans_begin;
		RansState rans;
		RansDecInit(&rans, &ptr);
		int sptr = 0;
		for(sptr = 0; sptr < olen; sptr++)
		{
			int range = RansDecGet(&rans, stats.ProbBits);
			uint32_t s = stats.DecGetSym(range);
			outbuf[out_p+sptr] = s;
			RansDecAdvanceSymbol(&rans, &ptr, stats.DecGetNext(s), stats.ProbBits);
		}
		in_p += clen;
		out_p += olen;
		stats.Clear();
	}
	
	*out_size = out_p;
}

#endif // ANS_H
