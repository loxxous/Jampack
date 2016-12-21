/*********************************************
*	FIFO ANS COMPRESSION AND DECOMPRESSION
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
	int StackSize = 64 << 10;
	int SBufSize = (StackSize * 2);
	int UpdateInterval = 8 << 10;
};

/*
	Write Esyms to stack in a FIFO manner, update the Esyms whenever during this part. (As long as the decoder updates the model at the same point then this will always work)
	When the stack is full compress the stack to the output in a LIFO manner.
	Reset rANS state and repeat.
*/
void ANS::encode(byte *inbuf, byte* outbuf, int *in_size, int *out_size)
{	
    Model stats;
	stats.Init();
    RansEncSymbol esyms[256];
	for (int i=0; i < 256; i++) { RansEncSymbolInit(&esyms[i], stats.CumFreqs[i], stats.Freqs[i], stats.ProbBits); }
	stats.ResetFreqs();
	RansEncSymbol *stack = (RansEncSymbol*)calloc(StackSize, sizeof(RansEncSymbol));
	byte *tmp = (byte*)calloc(SBufSize, sizeof(byte));
	int comp = 4;
	for(int n = 0; n < *in_size; )
	{
		int sptr = 0;
		for(; sptr < StackSize && (n+sptr) < *in_size; sptr++) // read input into esym stack using the model's probabilities
		{
			byte c = inbuf[n+sptr];
			stack[sptr] = esyms[c]; 
			
			stats.Freqs[c]++;
			if((sptr % UpdateInterval) == (UpdateInterval - 1))
			{
				stats.UpdateModel();
				for (int i=0; i < 256; i++) 
				{ 
					RansEncSymbolInit(&esyms[i], stats.CumFreqs[i], stats.Freqs[i], stats.ProbBits); 
				}
			}
		}
		RansState rans;
		RansEncInit(&rans);
		uint8_t *rans_begin;
		uint8_t* ptr = tmp+SBufSize; 	// *end* of temporary buffer
		for (size_t i=sptr; i > 0; i--)  		// working in reverse!
		{
			RansEncPutSymbol(&rans, &ptr, &stack[i-1]);
			//printf("%i\n", i);
		}
		RansEncFlush(&rans, &ptr);
		rans_begin = ptr; // final position in buffer
		
		// merge the buffer to the output stream
		int csize = &tmp[SBufSize] - rans_begin;
		for(int k = 0; k < csize; k++)
		{
			outbuf[comp+k] = rans_begin[k];
		}
		comp += csize;
		n += sptr;
	}
	// store original length (tells the decoder how many symbols to extract)
	outbuf[0] = (*in_size >> 24) & 0xff;
	outbuf[1] = (*in_size >> 16) & 0xff;
	outbuf[2] = (*in_size >> 8) & 0xff;
	outbuf[3] = (*in_size >> 0) & 0xff;
	*out_size = 4 + comp;

	free(tmp);
	free(stack);
}

void ANS::decode(byte *inbuf, byte* outbuf, int *in_size, int *out_size)
{
	Model stats;
	stats.Init();
	RansDecSymbol dsyms[256];
	for (int i=0; i < 256; i++) RansDecSymbolInit(&dsyms[i], stats.CumFreqs[i], stats.Freqs[i]);
	stats.ResetFreqs();
    uint8_t cum2sym[stats.ProbScale];
	
	for (int s = 0; s < 256; s++)
	{
		for (uint32_t i = stats.CumFreqs[s]; i < stats.CumFreqs[s + 1]; i++) 
		{
			cum2sym[i] = s; 
		}
	}

	*out_size = inbuf[0] << 24;
	*out_size |= inbuf[1] << 16;
	*out_size |= inbuf[2] << 8;
	*out_size |= inbuf[3] << 0;
	
	uint8_t *rans_begin = &inbuf[4];
	uint8_t* ptr = rans_begin;
	for(int n = 0; n < *out_size; )
	{
		RansState rans;
		RansDecInit(&rans, &ptr);
		int sptr = 0;
		for(sptr = 0; sptr < StackSize; sptr++)
		{
			int range = RansDecGet(&rans, stats.ProbBits);
			uint32_t s = cum2sym[range];
			outbuf[n+sptr] = s;
			RansDecAdvanceSymbol(&rans, &ptr, &dsyms[s], stats.ProbBits);

			stats.Freqs[s]++;
			if((sptr % UpdateInterval) == (UpdateInterval - 1))
			{
				stats.UpdateModel();
				for (int i=0; i < 256; i++) RansDecSymbolInit(&dsyms[i], stats.CumFreqs[i], stats.Freqs[i]);
				for (int s = 0; s < 256; s++)
				{
					for (uint32_t i = stats.CumFreqs[s]; i < stats.CumFreqs[s + 1]; i++) 
					{
						cum2sym[i] = s; 
					}
				}				
			}
		}
		n += sptr;
	}
}

#endif // ANS_H