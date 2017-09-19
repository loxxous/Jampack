/*********************************************
* Localized prefix model
**********************************************/
#include "lpx.hpp"

/**
* Update prefix table, count consecutive hits of order-n suffixes with a single leading prefix
* If there's a lot of local matches, reduce the order and adjust the threshold for more optimal sort ordering.
* If there's too many misses occuring, increase the order and adjust the threshold to prevent distortion. (This is all automatic)
*/
void Lpx::UpdateTable(PrefixRecord **table, unsigned int *cxt, unsigned int pos, unsigned char *order)
{
	unsigned int LP = (*cxt >> (*order * 8)) & 0xff;
	unsigned int LS = *cxt & ((1 << (*order * 8)) - 1);
	int distance = (pos - table[*order - 1][LP].pos); 
	
	int lower = MinThreshold;
	int upper = 0;
	
	if(table[*order - 1][LP].hits < MaxThreshold)
		upper = __max((distance), MinThreshold);
	else
		upper = __min((distance >> *order), MaxThreshold >> *order);
		
	int bound = (distance <= lower) ? lower : (distance > upper) ? upper : distance; 
	
	if(pos > *order)
	{
		if(table[*order - 1][LP].cxt == LS)
		{
			table[*order - 1][LP].pos = pos - *order;
			table[*order - 1][LP].hits++;
			table[*order - 1][LP].miss = 0;
		
			if(table[*order - 1][LP].hits > (unsigned int)((table[*order - 1][LP].threshold << (*order)) << 3) && *order > 1 && *order <= 3)
				order[0]--;
			
			if(table[*order - 1][LP].hits > (table[*order - 1][LP].threshold << 1) && table[*order - 1][LP].miss == 0)
				table[*order - 1][LP].threshold += (bound - table[*order - 1][LP].threshold) >> *order;
		}
		else
		{
			table[*order - 1][LP].hits >>= 2;
			table[*order - 1][LP].miss++;
			table[*order - 1][LP].cxt = LS;
			
			if(table[*order - 1][LP].miss > (table[*order - 1][LP].threshold * table[*order - 1][LP].threshold * *order) && *order >= 1 && *order < 3)
				order[0]++;
			
			if(table[*order - 1][LP].miss > table[*order - 1][LP].threshold)
				table[*order - 1][LP].threshold += (MaxThreshold - table[*order - 1][LP].threshold) >> (4 - *order); 
		}
	}
}

void Lpx::EncodeBlock(unsigned char *input, unsigned char *output, Index len)
{
	PrefixRecord *table[3];
	table[0] = (PrefixRecord*)calloc(256, sizeof(PrefixRecord));
	table[1] = (PrefixRecord*)calloc(256, sizeof(PrefixRecord));
	table[2] = (PrefixRecord*)calloc(256, sizeof(PrefixRecord));
	if(table[0] == NULL || table[1] == NULL || table[2] == NULL )
		Error("Could not allocate prefix model!");
	for(int i = 0; i < 256; i++)
	{
		table[0][i].threshold = MaxThreshold >> 1;
		table[1][i].threshold = MaxThreshold >> 1;
		table[2][i].threshold = MaxThreshold >> 1;
	}
	unsigned int cxt = 0;
	unsigned char order = 3;
	const unsigned int mask = 0xff;
	for (int i = 0; i < len;)
	{
		unsigned int dist = i - table[order - 1][cxt & mask].pos; // Anchor prediction offset
		if (table[order - 1][cxt & mask].hits > table[order - 1][cxt & mask].threshold && dist < MaxRecordSize)
		{
			unsigned char err;
			do
			{
				output[i] = err = input[i - dist] ^ input[i];
				UpdateTable(table, &cxt, i, &order);
				cxt = (cxt << 8) | input[i];
				i++;
			}
			while (err == 0 && i < len);
		}
		else
		{
			output[i] = input[i];
			UpdateTable(table, &cxt, i, &order);
			cxt = (cxt << 8) | input[i];
			i++;
		}
	}
	free(table[0]);
	free(table[1]);
	free(table[2]);
}

void Lpx::DecodeBlock(unsigned char *input, unsigned char *output, Index len)
{
	PrefixRecord *table[3];
	table[0] = (PrefixRecord*)calloc(256, sizeof(PrefixRecord));
	table[1] = (PrefixRecord*)calloc(256, sizeof(PrefixRecord));
	table[2] = (PrefixRecord*)calloc(256, sizeof(PrefixRecord));
	if(table[0] == NULL || table[1] == NULL || table[2] == NULL )
		Error("Could not allocate prefix model!");
	for(int i = 0; i < 256; i++)
	{
		table[0][i].threshold = MaxThreshold >> 1;
		table[1][i].threshold = MaxThreshold >> 1;
		table[2][i].threshold = MaxThreshold >> 1;
	}
	unsigned int cxt = 0;
	unsigned char order = 3;
	const unsigned int mask = 0xff; 
	for (int i = 0; i < len;)
	{
		unsigned int dist = i - table[order - 1][cxt & mask].pos; 
		if (table[order - 1][cxt & mask].hits > table[order - 1][cxt & mask].threshold && dist < MaxRecordSize)
		{
			unsigned char err;
			do
			{
				err = input[i];
				output[i] = output[i - dist] ^ input[i];
				UpdateTable(table, &cxt, i, &order);
				cxt = (cxt << 8) | output[i];
				i++;
			}
			while (err == 0 && i < len);
		}
		else
		{
			output[i] = input[i];
			UpdateTable(table, &cxt, i, &order);
			cxt = (cxt << 8) | output[i];
			i++;
		}
	}

	free(table[0]);
	free(table[1]);
	free(table[2]);
}

void Lpx::Encode(Buffer Input, Buffer Output, Options Opt)
{
	const Index PrefixBlock = *Input.size / 4; // Share workload amoung 4 threads
	#pragma omp parallel for num_threads(Opt.Threads)
	for(Index i = 0; i < *Input.size; i += PrefixBlock)
	{
		Index len = ((i + PrefixBlock) < *Input.size) ? PrefixBlock : (*Input.size - i);
		EncodeBlock(&Input.block[i], &Output.block[i], len);
	}
	*Output.size = *Input.size;
}

void Lpx::Decode(Buffer Input, Buffer Output, Options Opt)
{
	const Index PrefixBlock = *Input.size / 4; // Share workload amoung 4 threads
	#pragma omp parallel for num_threads(Opt.Threads)
	for(Index i = 0; i < *Input.size; i += PrefixBlock)
	{
		Index len = ((i + PrefixBlock) < *Input.size) ? PrefixBlock : (*Input.size - i);
		DecodeBlock(&Input.block[i], &Output.block[i], len);
	}
	*Output.size = *Input.size;
}
