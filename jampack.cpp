/*********************************************
* Full compression and decompression code
*
* Note: The entire jam algorithm is transformation based, this allows for tunable encode parameters for 
* stronger or weaker compression while maintaining fast decoding.
*
* The algorithm in a nutshell: 
* 1) deduplication removes big identical blocks (limited to block size)
* 2) filter can find structural redundancies, linear and non-linear 
* 3) local model induces non-static to static distribution on short bursts of matches
* 4) lz77 finds special cases and encodes them (positional contexts, etc...)
* 5) bwt encodes the entire block
**********************************************/
#include "jampack.hpp"

/**
* 'Input' must always be the first read in and 'Output' must be the last read out.
* If we need to feed the output into an input (for multiple stages) we just swap the pointers.
*/
void Jampack::SwapStreams()
{
	Buffer tmp = Input;
	Input = Output;
	Output = tmp;
}

/**
* Compress a block using all compression stages.
*/
void Jampack::Comp()
{
	crc = Chk->IntegrityCheck(Input);
	
	Options Dedupe = Option;
	Dedupe.MatchFinder = 0; // Set to 0 for deduplication
	Lz->Compress		(Input, Output, Dedupe);	SwapStreams(); 	// Deduplicate big chunks (limited to block size)
	Filter->Encode		(Input, Output, Option); 	SwapStreams(); 	// Filter any fixed points or linear projections (image, audio, triangular meshes, pretty much anything with a structure)
	LocalModel->Encode	(Input, Output, Option); 	SwapStreams();	// Local prefix model (short localized match induction)
	Lz->Compress		(Input, Output, Option);	SwapStreams(); 	// Compress data bwt cannot see (i.e: anti-contexts: sparse contexts, positional context, basically any non-markovian contexts)
	Bwt->ForwardBwt		(Input, Output); 		SwapStreams(); 	// Burrows wheeler transform
	Entropy->Encode		(Input, Output, Option);	// Structured rANS with large alphabet models
}

/**
* Decompress a block using inverse stages.
*/
void Jampack::Decomp()
{
	Entropy->Decode		(Input, Output, Option);	SwapStreams(); 	// Good!
	Bwt->InverseBwt		(Input, Output, Option);	SwapStreams(); 	// Well... Try to add 2N decoding
	Lz->Decompress		(Input, Output);		SwapStreams();	// Good!
	LocalModel->Decode	(Input, Output, Option);	SwapStreams(); 	// Good!
	Filter->Decode		(Input, Output);		SwapStreams();	// Good!	
	Lz->Decompress		(Input, Output);
	
	if(crc != Chk->IntegrityCheck(Output)) 
		Error("Detected corrupt block!"); 
}

void Jampack::InitComp(Options Opt)
{
	Option = Opt;
	Entropy = 	new Ans();
	Bwt = 		new BlockSort::Bwt();
	Lz = 		new Lz77();
	Chk = 		new Checksum();
	Filter = 	new Filters();
	LocalModel = 	new Lpx();
		
	if(Opt.BlockSize < 0 || Opt.BlockSize < MIN_BLOCKSIZE || Opt.BlockSize > MAX_BLOCKSIZE) 
		Error("Invalid blocksize!"); 
	
	BlockSize = Opt.BlockSize;
	int Buf = (int)(BlockSize) * 1.05;
	Input.block = (unsigned char*)calloc(Buf, sizeof(unsigned char));
	Output.block = (unsigned char*)calloc(Buf, sizeof(unsigned char));	
	
	if (Input.block == NULL || Output.block == NULL) 
		Error("Couldn't allocate Buffers!");
	
	Input.size = (int*)calloc(1, sizeof(int));
	Output.size = (int*)calloc(1, sizeof(int));
}

void Jampack::InitDecomp(Options Opt)
{
	Option = Opt;
	Entropy = 	new Ans();
	Bwt = 		new BlockSort::Bwt();
	Lz = 		new Lz77();
	Chk = 		new Checksum();
	Filter = 	new Filters();
	LocalModel = 	new Lpx();
	Input.size = (int*)calloc(1, sizeof(int));
	Output.size = (int*)calloc(1, sizeof(int));
	Input.block = (unsigned char*)malloc(sizeof(unsigned char));
	Output.block = (unsigned char*)malloc(sizeof(unsigned char));	
}

void Jampack::Free()
{
	delete Filter;
	delete Entropy;
	delete Bwt;
	delete Lz;
	delete Chk;
	delete LocalModel;
	free(Output.block);
	free(Input.block);
	free(Input.size);
	free(Output.size);
}

int Jampack::CompReadBlock(FILE *in)
{
	return *Input.size = fread(Input.block, 1, BlockSize, in);
}
	
/**
* Write header and compressed block
*/
void Jampack::CompWriteBlock(FILE *out)
{
	unsigned char *header = (unsigned char*)malloc((8 << 10) * sizeof(unsigned char));
	if(header == NULL)
		Error("Failed to allocate header space!");
	
	fwrite(&Magic, 1, strlen(Magic), out);
	fwrite(&crc, 1, sizeof(int), out);
	fwrite(Output.size, 1, sizeof(int), out);
	fwrite(&BlockSize, 1, sizeof(Index), out);
	fwrite(Output.block, 1, *Output.size, out); 
	
	free(header);
}

/**
* Read header and compressed block
*/	
int Jampack::DecompReadBlock(FILE *in)
{
	char Magic_check[MagicLength+1] = {0};
	fread(&Magic_check, 1, strlen(Magic), in);
	fread(&crc, 1, sizeof(int), in);
	fread(Input.size, 1, sizeof(int), in);
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

/**
* Write out the decoded data
*/
void Jampack::DecompWriteBlock(FILE *out)
{
	fwrite(Output.block, 1, *Output.size, out); 
}

void Jampack::DisplayHeaderContents() 
{
	printf("CRC: %08X\n", crc);
	printf("In size: %i\n", *Input.size);
	printf("Out size: %i\n", *Output.size);
	printf("------------------\n");
}

/**
*	This handles the IO and compressor instances shared among parallel threads. 
*	Multi-threading is written as multiple instances of the compressor running in parallel.
*/
void Jampack::Compress(FILE *in, FILE *out, Options Opt)
{
	if(Opt.Threads < MIN_THREADS) Opt.Threads = MIN_THREADS;
	if(Opt.Threads > MAX_THREADS) Opt.Threads = MAX_THREADS;
	if(Opt.BlockSize < MIN_BLOCKSIZE) Opt.BlockSize = MIN_BLOCKSIZE;
	if(Opt.BlockSize > MAX_BLOCKSIZE) Opt.BlockSize = MAX_BLOCKSIZE;
	
	Jampack *jam = new Jampack[Opt.Threads];  
	if(jam == NULL) 
		Error("Couldn't allocate compressor!");
	
	for(int n = 0; n < (int)Opt.Threads; n++) 
		jam[n].InitComp(Opt);
	
	uint64_t raw = 0, comp = 0;
	double ratio = 0;
	time_t start, cur;
	start = clock();
	
	while(1)
	{
		if(feof(in)) break;
		int s = 0;
		while(!feof(in) && (s < (int)Opt.Threads))
		{
			jam[s].CompReadBlock(in);
			raw += *jam[s].Input.size;
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
			comp += *jam[n].Output.size;
		}
		
		cur = clock();
		ratio = ((double)comp / (double)raw) * 100;
		double rate = (raw / (double)(1000000)) / (((double)cur - (double)start) / CLOCKS_PER_SEC);
		printf("Read: %.2f MB => %.2f MB (%.2f%%) @ %.2f MB/s        \r", (double)raw / (double)(1000000), (double)comp / (double)(1000000), ratio, rate);
	}
	printf("Read: %.2f MB => %.2f MB (%.2f%%)\n", (double)raw / (double)(1000000), (double)comp / (double)(1000000), ratio);

	for(int n = 0; n < (int)Opt.Threads; n++) 
		jam[n].Free();
	delete[] jam;
}

/**
* There are two decoder configurations, parallel on a single block, and parallel multi-block (multiple blocks with their own internal threads as well as external threads).
* By default Jampack is parallel on a single block since this mode requires a constant amount of memory for decoding.
* Multi-block decoding is very memory intensive, is loads multiple blocks to run in "parallel on a single block" mode. Hence the higher memory usage.
*/
void Jampack::Decompress(FILE *in, FILE *out, Options Opt) 
{
	if(Opt.Threads < MIN_THREADS) Opt.Threads = MIN_THREADS;
	if(Opt.Threads > MAX_THREADS) Opt.Threads = MAX_THREADS;
		
	// Decode using multiple threads working on a single block
	if(Opt.Multiblock == false)
	{
		Jampack *jam = new Jampack();
		if(jam == NULL)
			Error("Couldn't allocate decompressor!");
		
		jam->InitDecomp(Opt); // Set decoder to run with the selected number of threads on cpu or gpu (6N Memory)
			
		uint64_t raw = 0, comp = 0;
		double ratio = 0;
		time_t start, cur;
		start = clock();
		
		while(1)
		{ 
			if(feof(in)) break;
			if(jam->DecompReadBlock(in) > 0)
			{
				comp += *jam->Input.size;		
				jam->Decomp();
				jam->DecompWriteBlock(out);
				raw += *jam->Output.size;			
			}
			
			cur = clock();
			ratio = ((double)comp / (double)raw) * 100;
			double rate = (raw / (double)(1000000)) / (((double)cur - (double)start) / CLOCKS_PER_SEC);
			printf("Read: %.2f MB => %.2f MB (%.2f%%) @ %.2f MB/s        \r", (double)comp / (double)(1000000), (double)raw / (double)(1000000), ratio, rate);
		}
		printf("Read: %.2f MB => %.2f MB (%.2f%%)\n", (double)comp / (double)(1000000), (double)raw / (double)(1000000), ratio);

		jam->Free();
		delete jam;
	}
	// Decode using multiple threads on multiple blocks with their own internal threads(6N*K Memory, very fast when there are multiple blocks available)
	else
	{
		Jampack *jam = new Jampack[Opt.Threads];  
		if(jam == NULL) 
			Error("Couldn't allocate decompressor!");
		
		for(int n = 0; n < (int)Opt.Threads; n++) 
			jam[n].InitDecomp(Opt); // This initializes twice the amount of threads the system has but allows for very flexible parallelism to take place
		
		uint64_t raw = 0, comp = 0;
		double ratio = 0;
		time_t start, cur;
		start = clock();
		
		while(1)
		{
			if(feof(in)) break;
			int s = 0;
			while(!feof(in) && (s < Opt.Threads))
			{
				if(jam[s].DecompReadBlock(in) > 0)
				{
					comp += *jam[s].Input.size;		
					s++;
				}
				else
					break;
			}
			
			#pragma omp parallel for num_threads(s)
			for(int n = 0; n < s; n++)
			{
				jam[n].Decomp();
			}
			for(int n = 0; n < s; n++)
			{
				jam[n].DecompWriteBlock(out);
				raw += *jam[n].Output.size;
			}

			cur = clock();
			ratio = ((double)comp / (double)raw) * 100;
			double rate = (raw / (double)(1000000)) / (((double)cur - (double)start) / CLOCKS_PER_SEC);
			printf("Read: %.2f MB => %.2f MB (%.2f%%) @ %.2f MB/s        \r", (double)comp / (double)(1000000), (double)raw / (double)(1000000), ratio, rate);
		}
		printf("Read: %.2f MB => %.2f MB (%.2f%%)\n", (double)comp / (double)(1000000), (double)raw / (double)(1000000), ratio);
		
		
		for(int n = 0; n < (int)Opt.Threads; n++) 
			jam[n].Free();
		delete jam;
	}
}
