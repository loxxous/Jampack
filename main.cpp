#include "jampack.cpp"

int main(int argc, char** argv)
{
	if (argc < 4)
	{
		printf("Jampack Archiver v0.3 by Lucas Marsh (c) 2017\n\
Usage: Jampack.exe [option] input output [threads]\n\
Options:\n\
    c : compress\n\
    d : decompress\n\
Press 'ENTER' to continue...");

		getchar();
		return 0;
	}

	FILE* input = fopen(argv[2], "rb");
	if (input == NULL) return perror(argv[2]), 1;
	FILE* output = fopen(argv[3], "wb");
	if (output == NULL) return perror(argv[3]), 1;
	int threads = 0;	// Prompts algorithm to pick using hardware information
	if(argv[4] != NULL) threads = atoi(argv[4]);
	
	time_t start, end;
	start = clock();
	switch (argv[1][0])
	{
		case 'c': compress (input, output, threads); break;
		case 'd': decompress (input, output, threads); break;
		default: printf("invalid option!\n"); exit(0);
	}
	end = clock();
	double time = ((double)end - (double)start) / CLOCKS_PER_SEC;
	printf("Completed in %.2f seconds\n", time);

	fclose(input);
	fclose(output);
	return 0;
}
