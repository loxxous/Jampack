/*********************************************
* Full compression and decompression code
**********************************************/
#include "format.h"
#include "lz77.cpp"
#include "bwt.cpp"
#include "mtf.cpp"
#include "ans.cpp"
#include "checksum.h"

class Jampack
{
public:

	Buffer Input;
	Buffer Output;
	
private:

	BlockSort *bwt;
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

	void Comp()
	{
		cmethod = 0;
		fmethod = 0;

		crc = Chk->IntegrityCheck(Input);
		lz->compress(Input, Output, 64);
		bwt->ForwardBWT(Output, Input, Indices);
		entropy->encode(Input, Output);
	}
	
	void Decomp()
	{
		entropy->decode(Input, Output, Threads);
		bwt->InverseBWT(Output, Input, Indices, Threads);
		lz->decompress(Input, Output);	
		if(crc != Chk->IntegrityCheck(Output)) Error("Detected corrupt block!"); 
	}
	
	static void* CompLaunch(void* object)
	{
        	Jampack* obj = reinterpret_cast<Jampack*>(object);
        	obj->Comp();
		return NULL;
	}
	
	static void* DecompLaunch(void* object)
	{
        	Jampack* obj = reinterpret_cast<Jampack*>(object);
        	obj->Decomp();
		return NULL;
	}
	
	void ThreadedComp(pthread_t *thread)
	{
		int err = pthread_create(thread, NULL, &Jampack::CompLaunch, this); 
		if(err) Error("Unable to create thread!");
	}
	
	void ThreadedDecomp(pthread_t *thread)
	{
		int err = pthread_create(thread, NULL, &Jampack::DecompLaunch, this); 
		if(err) Error("Unable to create thread!");
	}
	
	void InitComp(int bsize)
	{
		entropy = new ANS();
		bwt = new BlockSort();
		lz = new Lz77();
		Chk = new Checksum();
		
		if(bsize < 0 || bsize < MIN_BLOCKSIZE || bsize > MAX_BLOCKSIZE) Error("Invalid blocksize!"); 
		BlockSize = bsize;
		int Buf = (int)(BlockSize) * 1.05;
		Input.block = (unsigned char*)calloc(Buf, sizeof(unsigned char));
		Output.block = (unsigned char*)calloc(Buf, sizeof(unsigned char));	
		if (Input.block == NULL || Output.block == NULL) Error("Couldn't allocate Buffers!");
		
		Indices = (Index*)calloc(BWT_UNITS, sizeof(Index));
		Input.size = (int*)calloc(1, sizeof(int));
		Output.size = (int*)calloc(1, sizeof(int));
	}
	
	void InitDecomp(int t)
	{
		Threads = t;
		entropy = new ANS();
		bwt = new BlockSort();
		lz = new Lz77();
		Chk = new Checksum();
		Indices = (int*)calloc(BWT_UNITS, sizeof(int));
		Input.size = (int*)calloc(1, sizeof(int));
		Output.size = (int*)calloc(1, sizeof(int));
	}
	
	void Free()
	{
		delete entropy;
		delete bwt;
		delete lz;
		delete Chk;
		free(Output.block);
		free(Input.block);
		free(Indices);
		free(Input.size);
		free(Output.size);
	}
	
	int CompReadBlock(FILE *in)
	{
		return *Input.size = fread(Input.block, 1, BlockSize, in);
	}
	
	void CompWriteBlock(FILE *out)
	{
		fwrite(&Magic, 1, strlen(Magic), out);
		fwrite(&crc, 1, sizeof(int), out);
		fwrite(Output.size, 1, sizeof(int), out);
		for(int i = 0; i < BWT_UNITS; i++) fwrite(&Indices[i], 1, sizeof(Index), out);
		fwrite(&cmethod, 1, sizeof(unsigned char), out);
		fwrite(&fmethod, 1, sizeof(unsigned char), out);
		fwrite(&BlockSize, 1, sizeof(Index), out);
		fwrite(Output.block, 1, *Output.size, out); 
	}
	
	int DecompReadBlock(FILE *in)
	{
		char Magic_check[MagicLength+1] = {0};
		fread(&Magic_check, 1, strlen(Magic), in);
		fread(&crc, 1, sizeof(int), in);
		fread(Input.size, 1, sizeof(int), in);
		for(int i = 0; i < BWT_UNITS; i++) 
		{
				fread(&Indices[i], 1, sizeof(Index), in);
				if(Indices[i] < 0 || Indices[i] > MAX_BLOCKSIZE)
					Error("Refusing to read from corrupt header!");
		}
		fread(&cmethod, 1, sizeof(unsigned char), in);
		fread(&fmethod, 1, sizeof(unsigned char), in);
		int e = fread(&BlockSize, 1, sizeof(Index), in);
		if (e == 0 ) return 0;
		else 
		{
			if (BlockSize < MIN_BLOCKSIZE || BlockSize > MAX_BLOCKSIZE || strcmp(Magic_check, Magic) != 0 ||
			*Input.size < 0 || *Input.size > MAX_BLOCKSIZE)
			{
				Error("Refusing to read from corrupt header!");
				return 0;
			} 
			else
			{
				int Buf = (int)(BlockSize) * 1.05;
				Input.block = (unsigned char*)realloc(Input.block, Buf * sizeof(unsigned char));
				Output.block = (unsigned char*)realloc(Output.block, Buf * sizeof(unsigned char));	
				if (Input.block == NULL || Output.block == NULL) Error("Couldn't allocate Buffers!");
				return fread(Input.block, 1, *Input.size, in);
			}
		}
	}
	
	void DecompWriteBlock(FILE *out)
	{
		fwrite(Output.block, 1, *Output.size, out); 
	}

	void DisplayHeaderContents() 
	{
		printf("CRC32: %08X\n", crc);
		printf("In size: %i\n", *Input.size);
		printf("Out size: %i\n", *Output.size);
		printf("Comp method: %i\n", cmethod);
		printf("Filt method: %i\n", fmethod);
		printf("------------------\n");
	}
	
	/*
		This handles the IO and compressor instances shared among parallel threads. 
		Multi-threading is written as instances of the compressor, in the case of the decompressor it uses multiple threads on a single compressed block.
		Each instance can either directly compress using Jampack::Comp() or be given a posix thread to run on with Jampack::ThreadedComp(pthread). 
	*/
	void Compress(FILE *in, FILE *out, int t, int bsize)
	{
		if(t < MIN_THREADS) t = MIN_THREADS;
		if(t > MAX_THREADS) t = MAX_THREADS;
		if(bsize < MIN_BLOCKSIZE) bsize = MIN_BLOCKSIZE;
		if(bsize > MAX_BLOCKSIZE) bsize = MAX_BLOCKSIZE;
		
		Jampack *jam = new Jampack[t];  if(jam == NULL) Error("Couldn't allocate model!");
		pthread_t threads[t];
		for(int n = 0; n < t; n++) jam[n].InitComp(bsize);
		
		uint64_t raw = 0, comp = 0, ri = 0, ro = 0;
		double ratio;
		time_t start, cur;
		start = clock();
		
		while(1)
		{ 
			if(feof(in)) break;
			int s = 0;
			while(!feof(in) && (s < t))
			{
				jam[s].CompReadBlock(in);
				ri += *jam[s].Input.size;
				s++;
			}
			for(int n = 0; n < s; n++)
			{
				jam[n].ThreadedComp(&threads[n]);
			}
			for(int n = 0; n < s; n++)
			{
				pthread_join( threads[n], NULL );
			}
			for(int n = 0; n < s; n++)
			{
				jam[n].CompWriteBlock(out);
				ro += *jam[n].Output.size;
			}
			
			cur = clock();
			raw = ri >> 10;
			comp = ro >> 10;
			ratio = ((float)comp / (float)raw) * 100;
			double rate = (raw>>10) / (((double)cur - (double)start) / CLOCKS_PER_SEC);
			printf("Read: %i MB => %i MB (%.2f%%) @ %.2f MB/s        \r", (int)(raw >> 10), (int)(comp >> 10), ratio, rate);
		}
		printf("Read: %i MB => %i MB (%.2f%%)\n", (int)(raw >> 10), (int)(comp >> 10), ratio);
		for(int n = 0; n < t; n++) jam[n].Free();
		delete[] jam;
	}

	void Decompress(FILE *in, FILE *out, int t) 
	{
		if(t < MIN_THREADS) t = MIN_THREADS;
		if(t > MAX_THREADS) t = MAX_THREADS;
		
		Jampack *jam = new Jampack();
		jam->InitDecomp(t); // Set decoder to run with the selected number of threads
		
		uint64_t raw = 0, comp = 0, ri = 0, ro = 0;
		double ratio;
		time_t start, cur;
		start = clock();
		
		while(1)
		{ 
			if(feof(in)) break;
			if(jam->DecompReadBlock(in) > 0)
			{
				ri += *jam->Input.size;		
				jam->Decomp();
				jam->DecompWriteBlock(out);
				ro += *jam->Output.size;			
			}
			
			cur = clock();
			raw = ri >> 10;
			comp = ro >> 10;
			ratio = ((float)raw / (float)comp) * 100;
			double rate = (comp>>10) / (((double)cur - (double)start) / CLOCKS_PER_SEC);
			printf("Read: %i MB => %i MB (%.2f%%) @ %.2f MB/s        \r", (int)(raw >> 10), (int)(comp >> 10), ratio, rate);
		}
		printf("Read: %i MB => %i MB (%.2f%%)\n", (int)(raw >> 10), (int)(comp >> 10), ratio);
		jam->Free();
		delete jam;
	}
	
	void DecompressParallel(FILE *in, FILE *out, int t) 
	{
		if(t < MIN_THREADS) t = MIN_THREADS;
		if(t > MAX_THREADS) t = MAX_THREADS;
		Jampack *jam[t];
		
		for(int i = 0; i < t; i++)
		{
			jam[i] = new Jampack();
			if(jam[i] == NULL) Error("Cannot allocate decoder!");
		}
		
		pthread_t threads[t];
		for(int n = 0; n < t; n++) jam[n]->InitDecomp(1);
		
		uint64_t raw = 0, comp = 0, ri = 0, ro = 0;
		double ratio;
		time_t start, cur;
		start = clock();
		
		while(1)
		{ 
			if(feof(in)) break;
			int s = 0;
			while(!feof(in) && (s < t))
			{
				if(jam[s]->DecompReadBlock(in) > 0)
				{
					ri += *jam[s]->Input.size;				
					s++;
				}
			}
			for(int n = 0; n < s; n++)
			{
				jam[n]->ThreadedDecomp(&threads[n]);
			}
			for(int n = 0; n < s; n++)
			{
				pthread_join( threads[n], NULL );
			}
			for(int n = 0; n < s; n++) 
			{
				jam[n]->DecompWriteBlock(out);
				ro += *jam[n]->Output.size;
			}	
			
			cur = clock();
			raw = ri >> 10;
			comp = ro >> 10;
			ratio = ((float)raw / (float)comp) * 100;
			double rate = (comp>>10) / (((double)cur - (double)start) / CLOCKS_PER_SEC);
			printf("Read: %i MB => %i MB (%.2f%%) @ %.2f MB/s        \r", (int)(raw >> 10), (int)(comp >> 10), ratio, rate);
		}
		printf("Read: %i MB => %i MB (%.2f%%)\n", (int)(raw >> 10), (int)(comp >> 10), ratio);
		for(int n = 0; n < t; n++) jam[n]->Free();
		for(int i = 0; i < t; i++)
		{
			delete jam[i];
		}
	}
};
