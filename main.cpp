/*********************************************
* Jampack - General purpose compression algorithm by Lucas Marsh (c) 2017
* Jampack uses a hybrid of bwt, lz77, prefix modeling, and filters to achieve very high compression at reasonable decode speed and memory.
* Encode memory is 6NK, decode memory is 3N by default (using all threads on a single block), or 3NK with multi-block enabled (using all threads with multiple parallel decoders).
*
Sorry for the long wait, jampack v0.8 is now out, it features new filters (inline delta, hueristic encoding, brute force is now an option), more aggressive local prefix induction (variable order), 
and a neat match-finder mode for induction of anti-contexts (non-markovian correlations, eg: sparse and positional contexts) via lz77 though since this it's pretty new so there's quite a bit of room for better anti-context parsing hueristics, 
also burrows wheeler decoding is now reduced from 6N to 3N in memory without any speed loss. Let me know if you manage to break it :)
* Note: 2N inverse alias bwt is on the way.
**********************************************/
#include "jampack.hpp"

int main(int argc, char** argv)
{	
	if (argc < 4)
	{
	#ifdef __CUDACC__
		printf("Jampack v%.2f by Lucas Marsh (c) 2017\n \n\
Usage: Jampack.exe <c|d> input output <options>\n \n\
Arguments:\n\
    c   Compress\n\
    d   Decompress\n \n\
Default options:\n\
   -b8 -m1 -f1\n \n\
Options:\n\
   -t#  Threads                     (1 to hardware maximum)\n\
   -b#  Block size in MB            (1 to 1000) \n\
   -m#  Match finder                (0 = dedupe, 1 = positional context hash chain, 2 = anti-context suffix array)\n\
   -f#  Generic filters             (0 = disable, 1 = heuristic, 2 = brute force)\n\
   -T   Enable multi-block decoding (Default disabled, uses all threads on one block instead of multiple blocks)\n\
   -g   Enable GPU decoding         (Default disable)\n \n\
Press 'enter' to continue", JAM_VERSION);
	#else
		printf("Jampack v%.2f by Lucas Marsh (c) 2017\n \n\
Usage: Jampack.exe <c|d> input output <options>\n \n\
Arguments:\n\
    c   Compress\n\
    d   Decompress\n \n\
Default options:\n\
   -b8 -m0 -f1\n \n\
Options:\n\
   -t#  Threads                      (1 to hardware maximum)\n\
   -b#  Block size in MB             (1 to 1000) \n\
   -m#  Match finder                 (0 = dedupe, 1 = positional context hash chain, 2 = anti-context suffix array)\n\
   -f#  Generic filters              (0 = disable, 1 = heuristic, 2 = brute force)\n\
   -T   Enable limited memory decode (Default disabled, uses all threads on one block instead of multiple blocks)\n \n\
Press 'enter' to continue", JAM_VERSION);
	#endif
		getchar();
		return 0;
	}
	
	if(strcmp(argv[2], argv[3]) == 0)
		Error("Refusing to write to input, change the output directory.");
	
	FILE* input = fopen(argv[2], "rb");
	if (input == NULL) return perror(argv[2]), 1;
	FILE* output = fopen(argv[3], "wb");
	if (output == NULL) return perror(argv[3]), 1;
	
	// Options are defined in format.hpp
	Options Opt;
	Opt.MatchFinder = 0;
	Opt.Threads = MAX_THREADS;
	Opt.BlockSize = DEFAULT_BLOCKSIZE;
	Opt.Filters = 1;
	Opt.Gpu = false;
	Opt.Multiblock = true;
	
	int cur_opt = 4;
	if (argc > 4)
	{
		while(cur_opt != argc)
		{
			const char* p=argv[cur_opt++];
			if(*p == '-')
			{
				p++;
				while (*p) 
				{
					switch (*p) 
					{
						case 'b': Opt.BlockSize = atoi(p+1) << 20; break;
						case 't': Opt.Threads = atoi(p+1); break;
						case 'm': Opt.MatchFinder = atoi(p+1); break;
						case 'f': Opt.Filters = atoi(p+1); break;
						case 'g': Opt.Gpu = true; break;
						case 'T': Opt.Multiblock = false; break;
					}
					p++;
				}
			}
		}
	}
	
	Jampack *Jam = new Jampack();
	
	time_t start;
	start = clock();
	switch (argv[1][0])
	{
		case 'c': Jam->Compress (input, output, Opt); break;
		case 'd': Jam->Decompress (input, output, Opt); break;
		default: printf("Invalid option!\n"); exit(0);
	}
	printf("Completed in %.2f seconds",  ((double)clock() - (double)start) / CLOCKS_PER_SEC);
	delete Jam;
	fclose(input);
	fclose(output);
	return EXIT_SUCCESS;
}
