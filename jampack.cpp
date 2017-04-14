/*********************************************
* Full compression and decompression code
**********************************************/
#include "jampack.hpp"

void Jampack::Comp()
{
	cmethod = 0;
	fmethod = 0;

	crc = Chk->IntegrityCheck(Input);
	lz->compress(Input, Output, 64);
	bwt->ForwardBWT(Output, Input, Indices);
	entropy->encode(Input, Output);
}
	
void Jampack::Decomp()
{
	entropy->decode(Input, Output, Threads);
	bwt->InverseBWT(Output, Input, Indices, Threads, GPU);
	lz->decompress(Input, Output);	
	if(crc != Chk->IntegrityCheck(Output)) Error("Detected corrupt block!"); 
}
	
void Jampack::InitComp(int bsize)
{
	entropy = new ANS();
	bwt = new BlockSort::BWT();
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
	
void Jampack::InitDecomp(int t, bool gpu)
{
	GPU = gpu;
	Threads = t;
	entropy = new ANS();
	bwt = new BlockSort::BWT();
	lz = new Lz77();
	Chk = new Checksum();
	Indices = (int*)calloc(BWT_UNITS, sizeof(int));
	Input.size = (int*)calloc(1, sizeof(int));
	Output.size = (int*)calloc(1, sizeof(int));
}
	
void Jampack::Free()
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
	
int Jampack::CompReadBlock(FILE *in)
{
	return *Input.size = fread(Input.block, 1, BlockSize, in);
}
	
void Jampack::CompWriteBlock(FILE *out)
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
	
int Jampack::DecompReadBlock(FILE *in)
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
		if (BlockSize < MIN_BLOCKSIZE || BlockSize > MAX_BLOCKSIZE || strcmp(Magic_check, Magic) != 0 || *Input.size < 0 || *Input.size > MAX_BLOCKSIZE)
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

void Jampack::DecompWriteBlock(FILE *out)
{
	fwrite(Output.block, 1, *Output.size, out); 
}

void Jampack::DisplayHeaderContents() 
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
void Jampack::Compress(FILE *in, FILE *out, int t, int bsize)
{
	if(t < MIN_THREADS) t = MIN_THREADS;
	if(t > MAX_THREADS) t = MAX_THREADS;
	if(bsize < MIN_BLOCKSIZE) bsize = MIN_BLOCKSIZE;
	if(bsize > MAX_BLOCKSIZE) bsize = MAX_BLOCKSIZE;
	
	Jampack *jam = new Jampack[t];  if(jam == NULL) Error("Couldn't allocate model!");
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
		#pragma omp parallel for num_threads(s)
		for(int n = 0; n < s; n++)
		{
			jam[n].Comp();
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

void Jampack::Decompress(FILE *in, FILE *out, int t, bool gpu) 
{
	if(t < MIN_THREADS) t = MIN_THREADS;
	if(t > MAX_THREADS) t = MAX_THREADS;
	
	Jampack *jam = new Jampack();
	jam->InitDecomp(t, gpu); // Set decoder to run with the selected number of threads on cpu or gpu
		
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
