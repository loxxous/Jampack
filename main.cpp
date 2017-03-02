/*********************************************
* Jampack - general purpose compression algorithm by Lucas Marsh (c) 2017
*
* Compression memory usage is 8MB + 6 x block size x number of threads.
* Decompression memory usage is 6 x block size.
* Regardless of the number of threads decoding memory is always constant in size.
**********************************************/
#include "jampack.cpp"

int main(int argc, char** argv)
{
	if (argc < 4)
	{
		printf("Jampack v0.4 by Lucas Marsh (c) 2017\n \n\
Usage: Jampack.exe <c|d> input output <options>\n \n\
Options:\n\
   -t# : Threads\n\
   -b# : Block size in MB\n \n\
Press 'enter' to continue");
		getchar();
		return 0;
	}
	
	FILE* input = fopen(argv[2], "rb");
	if (input == NULL) return perror(argv[2]), 1;
	FILE* output = fopen(argv[3], "wb");
	if (output == NULL) return perror(argv[3]), 1;
	
	int threads = DEFAULT_THREADS;
	int blocksize = DEFAULT_BLOCKSIZE;
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
						case 'b': blocksize = atoi(p+1) << 20; break;
						case 't': threads = atoi(p+1); break;
					}
					p++;
				}
			}
		}
		if(threads < MIN_THREADS) threads = MIN_THREADS;
		if(threads > MAX_THREADS) threads = MAX_THREADS;
		if(blocksize < MIN_BLOCKSIZE) blocksize = MIN_BLOCKSIZE;
		if(blocksize > MAX_BLOCKSIZE) blocksize = MAX_BLOCKSIZE;
	}
	
	Jampack* Jam = new Jampack();
	
	time_t start, end;
	start = clock();
	switch (argv[1][0])
	{
		case 'c': Jam->Compress (input, output, threads, blocksize); break;
		case 'd': Jam->Decompress (input, output, threads); break;
		default: printf("invalid option!\n"); exit(0);
	}
	end = clock();
	double time = ((double)end - (double)start) / CLOCKS_PER_SEC;
	printf("Completed in %.2f seconds\n", time);

	delete Jam;
	
	fclose(input);
	fclose(output);
	return 0;
}
