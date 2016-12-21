/*********************************************
*	FULL COMPRESSION AND DECOMPRESSION CODE
**********************************************/
#include "format.h"
#include "lz77.cpp"
#include "bwt.cpp"
#include "aic.cpp"
#include "ans.cpp"
#include "crc32.h"
#include "sys_detect.h"

/*
	TODO: add RLE0 + rANS
*/
struct Jampack
{
public:
	byte *in_block;
	byte *out_block;
	int *in_size;
	int *out_size;
private:	
	int *SA;
	int *indices;
	match *table;
	bwt *b;
	lz77 *lz;
	coder *aic;
	ANS *entropy;
	byte cmethod;
	byte fmethod;
	unsigned int crc;
public:
	void Comp()
	{
		cmethod = 1;
		fmethod = 0;
		crc = crc32(in_block, *in_size);
		lz->compress(in_block, in_size, out_block, out_size, table, 64);
		b->forwardBWT(SA, out_block, in_block, out_size, indices);
		aic->encode(in_block, out_size, out_block, out_size);
		*in_size = *out_size;
		entropy->encode(out_block, in_block, in_size, out_size);
		
		byte *tmp = in_block;
		in_block = out_block;
		out_block = tmp;
	}
	
	void Decomp()
	{
		entropy->decode(in_block, out_block, in_size, out_size);
		aic->decode(out_block, out_size, in_block, in_size);
		b->inverseBWT(SA, in_block, out_block, in_size, indices);
		lz->decompress(out_block, out_size, in_block, in_size);
		if(crc != crc32(in_block, *in_size)) { printf("Detected corrupt block!\n"); }
		
		byte *tmp = in_block;
		in_block = out_block;
		out_block = tmp;
		
		int tmpS = *in_size;
		*in_size = *out_size;
		*out_size = tmpS;
		
	}
	
	static void* CompLaunch(void* object)	// This exists because pthreads are extremely picky
    {
        Jampack* obj = reinterpret_cast<Jampack*>(object);
        obj->Comp();
    }
	
	static void* DecompLaunch(void* object)
    {
        Jampack* obj = reinterpret_cast<Jampack*>(object);
        obj->Decomp();
    }
	
	void ThreadedComp(pthread_t *thread)	// Take in a thread and assign it a task
	{
		int err = pthread_create(thread, NULL, &Jampack::CompLaunch, this); 
		if(err) { printf("Unable to create thread!\n"); }
	}
	
	void ThreadedDecomp(pthread_t *thread)
	{
		int err = pthread_create(thread, NULL, &Jampack::DecompLaunch, this); 
		if(err) { printf("Unable to create thread!\n"); }
	}
	
	void InitComp()
	{
		entropy = new ANS();
		b = new bwt();	
		lz = new lz77();
		aic = new coder();
		int BUF = (int)(BLOCKSIZE) + (12 << 10);
		in_block = (byte*)calloc(BUF, sizeof(byte));
		out_block = (byte*)calloc(BUF, sizeof(byte));	
		if (in_block == NULL || out_block == NULL) { printf("Couldn't allocate buffers!\n");  exit(1); }
		SA = (int*)calloc(BLOCKSIZE, sizeof(int)); if (SA == NULL) { printf("Couldn't allocate suffix array!\n"); exit(1); }
		table = (struct match*)calloc(ELEMENTS, sizeof(struct match)); if (table == NULL) { printf("Couldn't allocate match table!\n"); exit(1); }
		indices = (int*)calloc(8, sizeof(int));
		in_size = (int*)calloc(1, sizeof(int));
		out_size = (int*)calloc(1, sizeof(int));
	}
	
	void InitDecomp()
	{
		entropy = new ANS();
		b = new bwt();	
		lz = new lz77();
		aic = new coder();
		int BUF = (int)(BLOCKSIZE) + (12 << 10);
		in_block = (byte*)calloc(BUF, sizeof(byte));
		out_block = (byte*)calloc(BUF, sizeof(byte));	
		if (in_block == NULL || out_block == NULL) { printf("Couldn't allocate buffers!\n");  exit(1); }
		SA = (int*)calloc(BLOCKSIZE, sizeof(int)); if (SA == NULL) { printf("Couldn't allocate suffix array!\n"); exit(1); }
		indices = (int*)calloc(8, sizeof(int));
		in_size = (int*)calloc(1, sizeof(int));
		out_size = (int*)calloc(1, sizeof(int));
	}
	
	void FreeComp()
	{
		delete entropy;
		delete b;
		delete lz;
		delete aic;
		free(SA);
		free(out_block);
		free(in_block);
		free(table);
		free(indices);
		free(in_size);
		free(out_size);
	}
	
	void FreeDecomp()
	{
		delete entropy;
		delete b;
		delete lz;
		delete aic;
		free(SA);
		free(out_block);
		free(in_block);
		free(indices);
		free(in_size);
		free(out_size);
	}
	
	int ReadHeader(FILE *in)
	{
		int e;
		e = fread(&crc, 1, sizeof(int), in);
		e = fread(in_size, 1, sizeof(int), in);
		for(int i = 0; i < 8; i++) e = fread(&indices[i], 1, sizeof(int), in);
		e = fread(&cmethod, 1, sizeof(byte), in);
		e = fread(&fmethod, 1, sizeof(byte), in);
		if (e == 0 ) return 0;
		return 1;
	}

	void WriteHeader(FILE *out)
	{
		fwrite(&crc, 1, sizeof(int), out);
		fwrite(out_size, 1, sizeof(int), out);
		for(int i = 0; i < 8; i++) fwrite(&indices[i], 1, sizeof(int), out);
		fwrite(&cmethod, 1, sizeof(byte), out);
		fwrite(&fmethod, 1, sizeof(byte), out);
	}
	
	int CompReadBlock(FILE *in)
	{
		return *in_size = fread(in_block, 1, BLOCKSIZE, in);
	}
	
	int DecompReadBlock(FILE *in)
	{
		return fread(in_block, 1, *in_size, in);
	}
	
	void WriteBlock(FILE *out)
	{
		fwrite(out_block, 1, *out_size, out); 
	}
	
	void DisplayHeaderContents() 
	{
		printf("CRC32: %08X\n", crc);
		printf("In size: %i\n", *in_size);
		printf("Out size: %i\n", *out_size);
		printf("Indicies: %i, %i, %i, %i, %i, %i, %i, %i\n", indices[0], indices[1], indices[2], indices[3], indices[4], indices[5], indices[6], indices[7]);
		printf("Comp method: %i\n", cmethod);
		printf("Filt method: %i\n", fmethod);
		printf("------------------\n");
	}
};

/*
	This handles the IO and compressor instances shared among parallel threads. 
	Multi-threading is written as instances of the compressor or decompressor.
	Each instance can either directly compress using Comp() or be given a posix thread to run on.
*/
void compress(FILE *in, FILE *out, int t)
{
	if (t < MIN_THREADS || !t) t = DEFAULT_THREADS;
	else if(t > MAX_THREADS ) t = MAX_THREADS;
	Jampack jam[t];
	pthread_t threads[t];
	for(int n = 0; n < t; n++) jam[n].InitComp();
	
	uint64_t raw = 0, comp = 0, ri = 0, ro = 0;
	double ratio;
	time_t start, cur;
	start = clock();
	
	while(1)
	{ 
		if(feof(in)) break;
		int s = 0;
		while(!feof(in) && (s < t))	// read blocks sequentially 
		{
			jam[s].CompReadBlock(in);
			ri += *jam[s].in_size;
			s++;
		}
		for(int n = 0; n < s; n++)	// compress in parallel
		{
			jam[n].ThreadedComp(&threads[n]);
		}
		for(int n = 0; n < s; n++)	// wait for threads to finish
		{
			pthread_join( threads[n], NULL );
		}
		for(int n = 0; n < s; n++) 	// write blocks sequentially
		{
			jam[n].WriteHeader(out);
			jam[n].WriteBlock(out);
			ro += *jam[n].out_size;
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

void decompress(FILE *in, FILE *out, int t) 
{
	if (t < MIN_THREADS || !t) t = DEFAULT_THREADS;
	else if(t > MAX_THREADS ) t = MAX_THREADS;
	Jampack jam[t];
	pthread_t threads[t];
	for(int n = 0; n < t; n++) jam[n].InitDecomp();
	
	uint64_t raw = 0, comp = 0, ri = 0, ro = 0;
	double ratio;
	time_t start, cur;
	start = clock();
	
	while(1)
	{ 
		if(feof(in)) break;
		int s = 0;
		while(!feof(in) && (s < t))	// read blocks sequentially 
		{
			if(jam[s].ReadHeader(in) > 0)
			{
				jam[s].DecompReadBlock(in);	
				ri += *jam[s].in_size;				
				s++;
			}
		}
		for(int n = 0; n < s; n++)	// compress in parallel
		{
			jam[n].ThreadedDecomp(&threads[n]);
		}
		for(int n = 0; n < s; n++)	// wait for threads to finish
		{
			pthread_join( threads[n], NULL );
		}
		for(int n = 0; n < s; n++) 	// write blocks sequentially
		{
			jam[n].WriteBlock(out);
			ro += *jam[n].out_size;
		}	
		
		cur = clock();
		raw = ri >> 10;
		comp = ro >> 10;
		ratio = ((float)raw / (float)comp) * 100;
		double rate = (comp>>10) / (((double)cur - (double)start) / CLOCKS_PER_SEC);
		printf("Read: %i MB => %i MB (%.2f%%) @ %.2f MB/s        \r", (int)(raw >> 10), (int)(comp >> 10), ratio, rate);
	}
	printf("Read: %i MB => %i MB (%.2f%%)\n", (int)(raw >> 10), (int)(comp >> 10), ratio);
	for(int n = 0; n < t; n++) jam[n].FreeDecomp();
}