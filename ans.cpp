/*********************************************
* ANS compression and decompression code
*
* It is MTF + semi-static inherited order-1.
**********************************************/
#ifndef ANS_H
#define ANS_H

#include "rans_byte.h"
#include "model.h"

class ANS
{
public:
	void encode(Buffer Input, Buffer Output);
	void decode(Buffer Input, Buffer Output, int Threads);
private:
	int StackSize = 128 << 10; 	// Desired block size
	int SBufSize = (StackSize * 2); // Size of temporary buffer prior to merging
	void ThreadedDecodeLoad(Buffer Input, Buffer Output, int in_p, int out_p, int clen, int olen, Model *stats);

	class Parallel_ANS
	{
	private:
		Buffer Input; Buffer Output; int in_p; int out_p; int clen; int olen; Model *stats;
		
	public:
		void Load (Buffer _Input, Buffer _Output, int _in_p, int _out_p, int _clen, int _olen, Model *_stats)
		{
			Input = _Input;
			Output = _Output;
			in_p = _in_p;
			out_p = _out_p;
			clen = _clen;
			olen = _olen;
			stats = _stats;
		}

		void Threaded_Decode()
		{
			stats->SetDecodeTable();
			uint8_t *rans_begin = &Input.block[in_p];
			uint8_t* ptr = rans_begin;
			RansState R[4];
			RansDecInit(&R[0], &ptr);
			RansDecInit(&R[1], &ptr);
			RansDecInit(&R[2], &ptr);
			RansDecInit(&R[3], &ptr);
			int sptr = 0;
			for(sptr = 0; sptr < olen; sptr++)
			{
				RansState X = R[0];
				int range = RansDecGet(&X, stats->ProbBits);
				uint32_t s = stats->DecGetSym(range);
				Output.block[out_p+sptr] = s;
				RansDecAdvanceSymbol(&X, &ptr, stats->DecGetNext(s), stats->ProbBits);
				R[0] = R[1];
				R[1] = R[2];
				R[2] = R[3];
				R[3] = X;
			}
			stats->Clear();
			
			Postcoder *mtf = new Postcoder(); if(mtf == NULL) Error("Couldn't allocate postcoder");
			mtf->decode(&Output.block[out_p], olen);
			delete mtf;
		}
		
		static void* Launch(void* object){
			Parallel_ANS* obj = reinterpret_cast<Parallel_ANS*>(object);
			obj->Threaded_Decode();
			return NULL;
		}
		
		void pThread_ANS_Decode(pthread_t *thread){
			int err = pthread_create(thread, NULL, &Parallel_ANS::Launch, this); 
			if(err) Error("Unable to create thread!");
		}
	};
};

void ANS::encode(Buffer Input, Buffer Output)
{
	Postcoder *mtf = new Postcoder(); if(mtf == NULL) Error("Couldn't allocate postcoder!");
	Model *stats = new Model(); if(stats == NULL) Error("Couldn't allocate model!");
	RansEncSymbol *stack = (RansEncSymbol*)calloc(StackSize, sizeof(RansEncSymbol));
	unsigned char *tmp = (unsigned char*)calloc(SBufSize, sizeof(unsigned char));
	int in_p = 0;
	int out_p = 0;
	
	for(; in_p < *Input.size; )
	{
		// Gather statistics for the block
		int len = ((in_p+StackSize) < *Input.size) ? StackSize : (*Input.size - in_p);
		
		mtf->encode(&Input.block[in_p], len);
		
		//stats->Build(&Input.block[in_p], len);
		stats->BuildRLE0(&Input.block[in_p], len);
		stats->SetEncodeTable();
		
		int sptr = 0;
		for(; sptr < len; sptr++)
			stack[sptr] = stats->EncGetRange((int)Input.block[in_p+sptr]);
		
		RansState R[4];
		RansEncInit(&R[0]);
		RansEncInit(&R[1]);
		RansEncInit(&R[2]);
		RansEncInit(&R[3]);
		
		uint8_t *rans_begin;
		uint8_t* ptr = tmp+SBufSize; // *end* of temporary buffer
		for (size_t i=sptr; i > 0; i--) // working in reverse!
		{
			RansState X = R[3];
			RansEncPutSymbol(&X, &ptr, &stack[i-1]);
			R[3] = R[2];
			R[2] = R[1];
			R[1] = R[0];
			R[0] = X;
		}
		RansEncFlush(&R[3], &ptr);
		RansEncFlush(&R[2], &ptr);
		RansEncFlush(&R[1], &ptr);
		RansEncFlush(&R[0], &ptr);
		rans_begin = ptr;
		int csize = &tmp[SBufSize] - rans_begin;
		
		out_p += stats->WriteHeader(&Output.block[out_p], &len, &csize);
		
		// Merge the buffer to the output stream
		for(int k = 0; k < csize; k++) 
			Output.block[out_p+k] = rans_begin[k];

		out_p += csize;
		in_p += len;
		stats->Clear();
	}
	
	*Output.size = out_p;
	free(stack);
	free(tmp);
	delete mtf;
	delete stats;
}

void ANS::decode(Buffer Input, Buffer Output, int Threads)
{
	Model *stats = new Model[Threads]; if(stats == NULL) Error("Couldn't allocate model!");
	Parallel_ANS pANS[Threads];
	pthread_t pthreads[Threads];
	
	int in_p = 0;
	int out_p = 0;
	
	for(; in_p < *Input.size; )
	{
		int olen = 0;
		int clen = 0;
		int s = 0;
		while ((in_p < *Input.size) && (s < Threads))
		{
			in_p += stats[s].ReadHeader(&Input.block[in_p], &olen, &clen, StackSize);
			pANS[s].Load(Input, Output, in_p, out_p, clen, olen, &stats[s]);
			in_p += clen;
			out_p += olen;
			s++;
		}
		
		for(int k = 0; k < s; k++)
			pANS[k].pThread_ANS_Decode(&pthreads[k]);
		
		for(int k = 0; k < s; k++)
			pthread_join( pthreads[k], NULL );
	}

	*Output.size = out_p;
	delete[] stats;
}
#endif // ANS_H
