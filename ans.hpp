/*********************************************
* ANS compression and decompression header
**********************************************/
#ifndef ANS_H
#define ANS_H

#include "format.hpp"
#include "rans_byte.hpp"
#include "model.hpp"
#include "rank.hpp"
#include "rle.hpp"
#include "utils.hpp"
#include "tables.hpp"

class Ans
{
	private:
	int WriteHeader(unsigned char* outbuf, int* olen, int* clen, int* rlen, int* A);
	int ReadHeader(unsigned char* inbuf, int* olen, int* clen, int* rlen, int* A, int StackSize);
	
	static const int StackSize = 1 << 20;
	struct Range_t
	{
		int low;
		int freq;
	};
	
	static const int MaxModels = 8;
	static const int ModelSwitchThreshold = 2; // Exp[0 to 1] uses adaptive model, Exp[2 to 7] uses quasi static model
	
	public:
	void Encode(Buffer Input, Buffer Output, Options Opt);
	void Decode(Buffer Input, Buffer Output, Options Opt);
	
	class ParallelAns
	{
		private:
		Buffer Input; Buffer Output; Index in_p; Index out_p; Index clen; Index olen; Index rlen; Index *freqs;
		
		public:
		void Load (Buffer _Input, Buffer _Output, Index _in_p, Index _out_p, Index _clen, Index _olen, Index _rlen, Index *_freqs);
		void Threaded_Decode();
	};
};

#endif // ANS_H
