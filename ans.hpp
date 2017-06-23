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

	static constexpr int Exponent[9] = { 0,2,4,8,16,32,64,128,257 };
	
	static constexpr int Log[257] = { 
	0,0,1,1,2,2,2,2,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
	5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
	6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
	6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7 };
	
	static constexpr int Mantissa[257] = { 
	0,1,0,1,0,1,2,3,0,1,2,3,4,5,6,7,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
	0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
	0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
	32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,
	0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
	32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,
	64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,
	96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128 };
	
class Ans
{
	private:
	int WriteHeader(unsigned char* outbuf, int* olen, int* clen, int* rlen, int* A);
	int ReadHeader(unsigned char* inbuf, int* olen, int* clen, int* rlen, int* A, int StackSize);
	
	static const int StackSize = 256 << 10;
	struct Range_t
	{
		int low;
		int freq;
	};
	
	static const int MaxModels = 8;
	static const int ModelSwitchThreshold = 2; // Exp[0 to 1] uses adaptive model, Exp[2 to 7] uses quasi static model
	
	public:
	void Encode(Buffer Input, Buffer Output);
	void Decode(Buffer Input, Buffer Output, int Threads);
	
	class ParallelAns
	{
		private:
		Buffer Input; Buffer Output; int in_p; int out_p; int clen; int olen; int rlen; int *freqs;
		
		public:
		void Load (Buffer _Input, Buffer _Output, int _in_p, int _out_p, int _clen, int _olen, int _rlen, int *_freqs);
		void Threaded_Decode();
	};
};

#endif // ANS_H
