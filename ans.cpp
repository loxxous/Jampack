/*********************************************
* ANS compression and decompression code
*
* We compress the BWT with sorted rank coding, rle0, and structured adaptive rANS.
* The structure is a simple two level model which switches between models (adaptive or quasi-static) based on how complex the data is.
* The first level handles encoding the exponent of the rank, second level handles the mantissa of all possible ranks.
* The second level is either adaptive CDF coding or quasi-static coding selected by exponent context.
* BWT -> Rank -> RLE0 -> bytewise rANS (two encodes per symbol for exp+mant models)
**********************************************/
#include "ans.hpp"

void Ans::ParallelAns::Load (Buffer _Input, Buffer _Output, int _in_p, int _out_p, int _clen, int _olen, int _rlen, int *_freqs)
{
	Input = _Input;
	Output = _Output;
	in_p = _in_p;
	out_p = _out_p;
	clen = _clen;
	olen = _olen;
	rlen = _rlen;
	
	freqs = (int*)malloc(256 * sizeof(int));
	if(freqs == NULL) 
		Error("Failed to alloc freq array!");
	memcpy(&freqs[0], &_freqs[0], 256 * sizeof(int));
}

void Ans::ParallelAns::Threaded_Decode()
{
	AdaptiveModel *ExpModel = new AdaptiveModel(MaxModels);
	AdaptiveModel *MantPrime[ModelSwitchThreshold];
	QuasiModel *MantSec[MaxModels - ModelSwitchThreshold];
	for(int c = 0; c < ModelSwitchThreshold; c++)
		MantPrime[c] = new AdaptiveModel(Exponent[c + 1] - Exponent[c]);
	for(int c = 0; c < (MaxModels - ModelSwitchThreshold); c++)
		MantSec[c] = new QuasiModel(Exponent[c + ModelSwitchThreshold + 1] - Exponent[c + ModelSwitchThreshold]);
	
	ExpModel->Reset();
	for(int c = 0; c < ModelSwitchThreshold; c++)
		MantPrime[c]->Reset();
	for(int c = 0; c < (MaxModels - ModelSwitchThreshold); c++)
		MantSec[c]->Reset();
	
	unsigned short *rlebuf = (unsigned short*)malloc(StackSize * sizeof(unsigned short)); if(rlebuf == NULL) Error("Couldn't allocate rle buffer!");
	
	uint8_t *rans_begin = &Input.block[in_p];
	uint8_t* ptr = rans_begin;
	RansState R[4];
	RansDecInit(&R[0], &ptr);
	RansDecInit(&R[1], &ptr);
	RansDecInit(&R[2], &ptr);
	RansDecInit(&R[3], &ptr);
	for(int sptr = 0; sptr < rlen; sptr++)
	{
		unsigned short e, m;
		RansState X = R[0];
		int range = RansDecGet(&X, ExpModel->ProbBits);
		e = ExpModel->RangeToSym(range);
		RansDecAdvance(&X, &ptr, ExpModel->SymToLow(e), ExpModel->SymToFreq(e), ExpModel->ProbBits);
		ExpModel->Update(e);
		R[0] = R[1];
		R[1] = R[2];
		R[2] = R[3];
		R[3] = X;
		
		X = R[0];
		if(e < ModelSwitchThreshold) // Use adaptive model (best compression)
		{
			range = RansDecGet(&X, MantPrime[e]->ProbBits);
			m = MantPrime[e]->RangeToSym(range);
			RansDecAdvance(&X, &ptr, MantPrime[e]->SymToLow(m), MantPrime[e]->SymToFreq(m), MantPrime[e]->ProbBits);
			MantPrime[e]->Update(m);
		}
		else // Use quasi static model (much faster on complex distributions)
		{
			range = RansDecGet(&X, MantSec[e]->ProbBits);
			m = MantSec[e - ModelSwitchThreshold]->RangeToSym(range);
			RansDecAdvance(&X, &ptr, MantSec[e - ModelSwitchThreshold]->SymToLow(m), MantSec[e - ModelSwitchThreshold]->SymToFreq(m), MantSec[e - ModelSwitchThreshold]->ProbBits);
			MantSec[e - ModelSwitchThreshold]->Update(m);
		}	
		R[0] = R[1];
		R[1] = R[2];
		R[2] = R[3];
		R[3] = X;
		
		rlebuf[sptr] = Exponent[e] + Mantissa[Exponent[e] + m]; // original symbol
	}
	
	for(int c = 0; c < ModelSwitchThreshold; c++)
		delete MantPrime[c];
	for(int c = 0; c < (MaxModels - ModelSwitchThreshold); c++)
		delete MantSec[c];
	delete ExpModel;
	
	RLE *rle0 = new RLE(); if(rle0 == NULL) Error("Couldn't allocate rle0!");
	rle0->decode(rlebuf, &Output.block[out_p], &rlen, olen);
	delete rle0;
	free(rlebuf);
	
	Postcoder *rank = new Postcoder(); if(rank == NULL) Error("Couldn't allocate postcoder!");
	rank->Decode(&Output.block[out_p], freqs, olen);
	delete rank;
	free(freqs);
}

void Ans::Encode(Buffer Input, Buffer Output)
{
	AdaptiveModel *ExpModel = new AdaptiveModel(MaxModels);
	AdaptiveModel *MantPrime[ModelSwitchThreshold];
	QuasiModel *MantSec[MaxModels - ModelSwitchThreshold];
	
	for(int c = 0; c < ModelSwitchThreshold; c++)
		MantPrime[c] = new AdaptiveModel(Exponent[c + 1] - Exponent[c]);
	
	for(int c = 0; c < (MaxModels - ModelSwitchThreshold); c++)
		MantSec[c] = new QuasiModel(Exponent[c + ModelSwitchThreshold + 1] - Exponent[c + ModelSwitchThreshold]);
	
	Postcoder *rank = new Postcoder;
	RLE *rle0 = new RLE;
	Range_t *stack = (Range_t*)malloc(StackSize * 2 * sizeof(Range_t)); if(stack == NULL) Error("Failed to alloc ans encoder stack!");
	int *freqs = (int*)malloc(256 * sizeof(int)); if(freqs == NULL) Error("Failed to alloc rank freq array!");
	unsigned short *rlebuf = (unsigned short*)malloc(StackSize * sizeof(unsigned short)); if(rlebuf == NULL) Error("Couldn't allocate rle buffer!");
	unsigned char *tmp = (unsigned char*)malloc(StackSize * 2 * sizeof(unsigned char)); if(tmp == NULL) Error("Couldn't allocate temporary buffer!");

	unsigned int in_p = 0;
	unsigned int out_p = 0;
	for(; in_p < *Input.size; )
	{
		ExpModel->Reset();
		for(int c = 0; c < ModelSwitchThreshold; c++)
			MantPrime[c]->Reset();
		for(int c = 0; c < (MaxModels - ModelSwitchThreshold); c++)
			MantSec[c]->Reset();
		
		int len = ((in_p + StackSize) < *Input.size) ? StackSize : (*Input.size - in_p);
		rank->Encode(&Input.block[in_p], freqs, len);
		int rlen = len;
		rle0->encode(&Input.block[in_p], rlebuf, &rlen);
		
		// Structured symbol buffer
		int sptr = 0;
		unsigned short sym = 0;
		int e; // exponent
		int m; // mantissa
		for(int i = 0; i < rlen; i++)
		{
			sym = rlebuf[i];
			e = Log[sym]; // 0 to 7
			m = Mantissa[sym]; // 8 models selected by exponent context
			
			stack[sptr].low = ExpModel->SymToLow(e);
			stack[sptr].freq = ExpModel->SymToFreq(e);
			ExpModel->Update(e);
			
			#ifndef NDEBUG
			if(stack[sptr].freq <= 0)
				Error("Exponent model failure (CDF)!");
			#endif
			if(e < ModelSwitchThreshold) // Use adaptive model (best compression)
			{
				stack[sptr + 1].low = MantPrime[e]->SymToLow(m);
				stack[sptr + 1].freq = MantPrime[e]->SymToFreq(m);
				MantPrime[e]->Update(m);
				#ifndef NDEBUG
				if(stack[sptr + 1].freq <= 0)
					Error("Mantissa model failure (CDF)!");
				#endif
			}
			else // Use quasi static model (much faster on complex distributions)
			{
				stack[sptr + 1].low = MantSec[e - ModelSwitchThreshold]->SymToLow(m);
				stack[sptr + 1].freq = MantSec[e - ModelSwitchThreshold]->SymToFreq(m);
				MantSec[e - ModelSwitchThreshold]->Update(m);
				#ifndef NDEBUG
				if(stack[sptr + 1].freq <= 0)
					Error("Mantissa model failure (Quasi)!");
				#endif
			}
			sptr += 2;
		}
		
		RansState R[4];
		RansEncInit(&R[0]);
		RansEncInit(&R[1]);
		RansEncInit(&R[2]);
		RansEncInit(&R[3]);
		uint8_t *rans_begin;
		uint8_t* ptr = tmp + (StackSize * 2); // *end* of temporary buffer
		for (size_t i=sptr; i > 0; i--) // working in reverse!
		{
			RansState X = R[3];
			RansEncPut(&X, &ptr, stack[i-1].low, stack[i-1].freq, ExpModel->ProbBits); // All models use the same number of ProbBits
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
		int csize = &tmp[StackSize*2] - rans_begin;
		out_p += WriteHeader(&Output.block[out_p], &len, &csize, &rlen, &freqs[0]);
		
		// Merge the buffer to the output stream
		for(int k = 0; k < csize; k++) 
			Output.block[out_p+k] = rans_begin[k];
		
		out_p += csize;
		in_p += len;
	}
	*Output.size = out_p;
	free(stack);
	free(tmp);
	free(rlebuf);
	free(freqs);
	for(int c = 0; c < ModelSwitchThreshold; c++)
		delete MantPrime[c];
	for(int c = 0; c < (MaxModels - ModelSwitchThreshold); c++)
		delete MantSec[c];
	delete ExpModel;
	delete rank;
	delete rle0;
}

void Ans::Decode(Buffer Input, Buffer Output, int Threads)
{
	ParallelAns* pANS = new ParallelAns[Threads];
	int *freqs = new int[256]; 
	if(freqs == NULL) 
		Error("Failed to alloc rank freq array!");
	
	int in_p = 0;
	int out_p = 0;
	
	for(; in_p < *Input.size; )
	{
		int olen = 0;
		int clen = 0;
		int rlen = 0;
		int s = 0;
		
		while ((in_p < *Input.size) && (s < Threads))
		{
			in_p += ReadHeader(&Input.block[in_p], &olen, &clen, &rlen, &freqs[0], StackSize);
			pANS[s].Load(Input, Output, in_p, out_p, clen, olen, rlen, &freqs[0]);
			in_p += clen;
			out_p += olen;
			s++;
		}
		#pragma omp parallel for num_threads(s)
		for(int k = 0; k < s; k++)
			pANS[k].Threaded_Decode();
	}

	*Output.size = out_p;
	delete[] pANS;
	delete[] freqs;
}

int Ans::WriteHeader(unsigned char* outbuf, int* olen, int* clen, int* rlen, int* A)
{
	Utils *leb = new Utils;
	int pos = 0;
		
	for(int i = 0; i < 256; i++)
		pos += leb->EncodeLeb128(A[i], &outbuf[pos]);
	pos += leb->EncodeLeb128(*olen, &outbuf[pos]);
	pos += leb->EncodeLeb128(*clen, &outbuf[pos]);
	pos += leb->EncodeLeb128(*rlen, &outbuf[pos]);
	delete leb;
	
	return pos;
}

int Ans::ReadHeader(unsigned char* inbuf, int* olen, int* clen, int* rlen, int* A, int StackSize)
{
	Utils *leb = new Utils;
	int pos = 0;
	
	for(int i = 0; i < 256; i++)
		pos += leb->DecodeLeb128(&A[i], &inbuf[pos]);
	pos += leb->DecodeLeb128(olen, &inbuf[pos]);
	pos += leb->DecodeLeb128(clen, &inbuf[pos]);
	pos += leb->DecodeLeb128(rlen, &inbuf[pos]);
	if(!(*olen >= 0 && *olen <= StackSize) || !(*rlen >= 0 && *rlen <= StackSize)) 
		Error("Misaligned or corrupt header!"); 
	delete leb;
	
	return pos;
}