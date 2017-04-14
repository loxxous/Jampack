/*********************************************
* Jampack - general purpose compression algorithm by Lucas Marsh (c) 2017
*
* Compression memory usage is 8MB + 6 x block size x number of threads.
* Decompression memory usage is 6 x block size.
* Regardless of the number of threads decoding memory is always constant in size.
**********************************************/
#include "jampack.hpp"

int main(int argc, char** argv)
{	
	if (argc < 4)
	{
		printf("Jampack v%.2f by Lucas Marsh (c) 2017\n \n\
Usage: Jampack.exe <c|d> input output <options>\n \n\
Options:\n\
   -t# : Threads\n\
   -b# : Block size in MB\n\
   -g# : Enable GPU decoding\n   \n\
Press 'enter' to continue", JAM_VERSION);

		getchar();
		return 0;
	}
	
	if(strcmp(argv[2], argv[3]) == 0)
		Error("Refusing to write to input.");
	
	FILE* input = fopen(argv[2], "rb");
	if (input == NULL) return perror(argv[2]), 1;
	FILE* output = fopen(argv[3], "wb");
	if (output == NULL) return perror(argv[3]), 1;
	
	int threads = MAX_THREADS;
	int blocksize = DEFAULT_BLOCKSIZE;
	bool gpu = false;
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
						case 'g': gpu = true; break;
					}
					p++;
				}
			}
		}
	}
	
	Jampack* Jam = new Jampack();
	
	time_t start;
	start = clock();
	switch (argv[1][0])
	{
		case 'c': Jam->Compress (input, output, threads, blocksize); break;
		case 'd': Jam->Decompress (input, output, threads, gpu); break;
		default: printf("Invalid option!\n"); exit(0);
	}
	printf("Completed in %.2f seconds\n",  ((double)clock() - (double)start) / CLOCKS_PER_SEC);

	delete Jam;
	
	fclose(input);
	fclose(output);
	return 0;
}
