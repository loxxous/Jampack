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

class ANS
{
public:
	void encode(Buffer Input, Buffer Output);
	void decode(Buffer Input, Buffer Output, int Threads);
private:
	static const int StackSize = 256 << 10;
	static const int SBufSize = (StackSize * 2);
public:
	class Parallel_ANS
	{
	private:
		Buffer Input; Buffer Output; int in_p; int out_p; int clen; int olen; int rlen; Model *stats;
		
	public:
		void Load (Buffer _Input, Buffer _Output, int _in_p, int _out_p, int _clen, int _olen, int _rlen, Model *_stats);
		void Threaded_Decode();
	};
};

#endif // ANS_H
