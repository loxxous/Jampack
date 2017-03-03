/*********************************************
* Full compression and decompression code
**********************************************/
#include "format.h"
#include "lz77.cpp"
#include "bwt.cpp"
#include "mtf.cpp"
#include "ans.cpp"
#include "crc32.h"
#include "sys_detect.h"

class Jampack
{
public:

	Buffer Input;
	Buffer Output;
	
private:

	Match *table;
	BlockSort *bwt;
	Lz77 *lz;
	Postcoder *mtf;
	ANS *entropy;
	Index BlockSize;
	int Threads;
	int *Indices;
	unsigned char cmethod;
	unsigned char fmethod;
	unsigned int crc;
	
public:

	void Comp()
	{
		cmethod = 1;
		fmethod = 0;
		crc = crc32(Input);
		lz->compress(Input, Output, table, 64);
		bwt->ForwardBWT(Output, Input, Indices);
		mtf->encode(Input, Output);
		entropy->encode(Output, Input);
		
		Buffer tmp = Input;
		Input = Output;
		Output = tmp;	
	}
	
	void Decomp()
	{
		entropy->decode(Input, Output, Threads);
		mtf->decode(Output, Input);
		bwt->InverseBWT(Input, Output, Indices, Threads);
		lz->decompress(Output, Input);
		if(crc != crc32(Input)) Error("Detected corrupt block!"); 
		
		Buffer tmp = Input;
		Input = Output;
		Output = tmp;	
	}
	
	static void* CompLaunch(void* object)
    	{
        	Jampack* obj = reinterpret_cast<Jampack*>(object);
        	obj->Comp();
    	}
	
	static void* DecompLaunch(void* object)
    	{
        	Jampack* obj = reinterpret_cast<Jampack*>(object);
        	obj->Decomp();
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
		mtf = new Postcoder();
		
		if(bsize < 0 || bsize < MIN_BLOCKSIZE || bsize > MAX_BLOCKSIZE) Error("Invalid blocksize!"); 
		BlockSize = bsize;
		int Buf = (int)(BlockSize) * 1.05;
		Input.block = (unsigned char*)calloc(Buf, sizeof(unsigned char));
		Output.block = (unsigned char*)calloc(Buf, sizeof(unsigned char));	
		if (Input.block == NULL || Output.block == NULL) Error("Couldn't allocate Buffers!");
		
		table = (struct Match*)calloc(LZ_ELEMENTS, sizeof(struct Match)); if (table == NULL) Error("Couldn't allocate match table!");
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
		mtf = new Postcoder();
		Indices = (int*)calloc(BWT_UNITS, sizeof(int));
		Input.size = (int*)calloc(1, sizeof(int));
		Output.size = (int*)calloc(1, sizeof(int));
	}
	
	void FreeComp()
	{
		delete entropy;
		delete bwt;
		delete lz;
		delete mtf;
		free(Output.block);
		free(Input.block);
		free(table);
		free(Indices);
		free(Input.size);
		free(Output.size);
	}
	
	void FreeDecomp()
	{
		delete entropy;
		delete bwt;
		delete lz;
		delete mtf;
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
		char Magic_check[strlen(Magic)+1] = "";
		fread(&Magic_check, 1, strlen(Magic), in);
		fread(&crc, 1, sizeof(int), in);
		fread(Input.size, 1, sizeof(int), in);
		for(int i = 0; i < BWT_UNITS; i++) fread(&Indices[i], 1, sizeof(Index), in);
		fread(&cmethod, 1, sizeof(unsigned char), in);
		fread(&fmethod, 1, sizeof(unsigned char), in);
		int prev_bsize = BlockSize;
		int e = fread(&BlockSize, 1, sizeof(Index), in);
		if (e == 0 ) return 0;
		else 
		{
			if (BlockSize < MIN_BLOCKSIZE || BlockSize > MAX_BLOCKSIZE || strcmp(Magic_check, Magic) != 0)
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
		
		Jampack jam[t];
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
		for(int n = 0; n < t; n++) jam[n].FreeComp();
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
		jam->FreeDecomp();
		delete jam;
	}
};
