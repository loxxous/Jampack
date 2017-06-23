/*********************************************
* Localized prefix model
**********************************************/
#include "lpx.hpp"

/**
* Update prefix table, count consecutive hits of suffixes with a single leading prefix
*/
void Lpx::UpdateTable(PrefixRecord *table, unsigned int *cxt, unsigned int pos)
{
	unsigned int LP = *cxt >> 24;
	unsigned int LS = *cxt & 0xffffff;
	
	if(pos > 3)
	{
		if(table[LP].cxt == LS)
		{
			table[LP].hits++;
		}
		else
		{
			table[LP].hits = 0;
			table[LP].cxt = LS;
		}
		table[LP].pos = pos - 3;
	}
}

void Lpx::Encode(Buffer Input, Buffer Output)
{ 	
	PrefixRecord *t = (PrefixRecord*)calloc(256, sizeof(PrefixRecord));
	unsigned int cxt = 0;
	const unsigned int mask = 0xff;
	for (int i = 0; i < *Input.size;)
	{
		int dist = i - t[cxt & mask].pos; // Anchor prediction offset
		if (t[cxt & mask].hits > HitThreshold && dist < MaxRecordSize)
		{
			unsigned char err;
			do
			{
				Output.block[i] = err = Input.block[i - dist] ^ Input.block[i];
				UpdateTable(t, &cxt, i);
				cxt = (cxt << 8) | Input.block[i];
				i++;
			}
			while (err == 0 && i < *Input.size);
		}
		else
		{
			Output.block[i] = Input.block[i];
			UpdateTable(t, &cxt, i);
			cxt = (cxt << 8) | Input.block[i];
			i++;
		}
	}
	free(t);
	*Output.size = *Input.size;
}

void Lpx::Decode(Buffer Input, Buffer Output)
{
	PrefixRecord *t = (PrefixRecord*)calloc(256, sizeof(PrefixRecord));
	unsigned int cxt = 0;
	const unsigned int mask = 0xff; 
	for (int i = 0; i < *Input.size;)
	{
		int dist = i - t[cxt & mask].pos; 
		if (t[cxt & mask].hits > HitThreshold && dist < MaxRecordSize)
		{
			unsigned char err;
			do
			{
				err = Input.block[i];
				Output.block[i] = Output.block[i - dist] ^ Input.block[i];
				UpdateTable(t, &cxt, i);
				cxt = (cxt << 8) | Output.block[i];
				i++;
			}
			while (err == 0 && i < *Input.size);
		}
		else
		{
			Output.block[i] = Input.block[i];
			UpdateTable(t, &cxt, i);
			cxt = (cxt << 8) | Output.block[i];
			i++;
		}
	}
	free(t);
	*Output.size = *Input.size;
}