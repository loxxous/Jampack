/*********************************************
* LZ77 compression and decompression code
**********************************************/
#include "lz77.hpp"

inline int Lz77::HashLong(int ctx)
{
	return (ctx * 0x9E3779B1) >> (32 - DUPE_HBITS);
}
	
/** 
* Determine if the current state is compressible 
*/
bool Lz77::Compressible(int look_ahead, int match_len, int offset, int minmatch)
{
	if (match_len < minmatch) return false;
	short cost = 0;
	if (offset < constants[0]) cost = 1;
	else if (offset < constants[1]) cost = 2;
	else if (offset < constants[2]) cost = 3;
	else cost = 4;
	if ((look_ahead < constants[0]) && (match_len >= minmatch + cost + 1)) return true;
	else if ((look_ahead < constants[1]) && (match_len >= minmatch + cost + 2)) return true;
	else if ((look_ahead < constants[2]) && (match_len >= minmatch + cost + 3)) return true;
	else if (match_len >= minmatch + cost + 4) return true;
	return false;
}

/**
* Compress input block to output block
*/
void Lz77::Compress(Buffer Input, Buffer Output, int minmatch)
{
	Utils *leb = new Utils;
	MatchLong *DupeTable;
	DupeTable = (struct MatchLong*)calloc(LZ_DUPE_ELEMENTS, sizeof(struct MatchLong)); 
	if (DupeTable == NULL) 
		Error("Couldn't allocate dedupe table!");
	
	int shift = (minmatch > 32) ? 1 : 32 / minmatch;
	int h = 0;
	int ctx = 0;
	Index l = 0;
	Index pos = 0;
	Index out_pos = 0;
	while (pos < *Input.size)
	{
		h = HashLong(ctx);
		Index prev_pos = DupeTable[h].pos;
		if ((ctx == DupeTable[h].ctx) && ((pos - prev_pos) > 0))
		{
			Index back = 0;
			Index m = 0;
			Index off = pos - prev_pos;
			
			// Search backwards in case of hash miss.
			while (Input.block[prev_pos - back - 1] == Input.block[pos - back - 1] && (prev_pos - back - 1) > 0 && (back < l)) 
				back++;
	
			// Search forward
			while (Input.block[prev_pos + m] == Input.block[pos + m] && (pos + m + minmatch) < *Input.size)
				m++;
			
			if (Compressible(l - back, m + back, off, minmatch))
			{
				//Merge the match data from both
				m += back; // increase match length
				l -= back; // reduce literal run
				pos -= back; // correct current position
			
				out_pos += leb->EncodeLeb128(l, &Output.block[out_pos]);
				out_pos += leb->EncodeLeb128(m, &Output.block[out_pos]);
				out_pos += leb->EncodeLeb128(off, &Output.block[out_pos]);
				memcpy(&Output.block[out_pos], &Input.block[pos - l], l);
				for (Index i = 0; i < m; i++)
				{
					h = HashLong(ctx);
					DupeTable[h].pos = pos + i;
					DupeTable[h].ctx = ctx;
					ctx = (ctx << shift) ^ Input.block[pos + minmatch + i];
				}
				out_pos += l;
				pos += m;
				l = 0;
			}
		}
		DupeTable[h].pos = pos;
		DupeTable[h].ctx = ctx;
		ctx = (ctx << shift) ^ Input.block[pos + minmatch];
		pos++;
		l++;
	}
	out_pos += leb->EncodeLeb128(0, &Output.block[out_pos]);
	out_pos += leb->EncodeLeb128(0, &Output.block[out_pos]);
	out_pos += leb->EncodeLeb128(0, &Output.block[out_pos]);
	memcpy(&Output.block[out_pos], &Input.block[pos - l], l);
	out_pos += l;
	*Output.size = out_pos;
	free(DupeTable);
	delete leb;
}

/**
* Decompress input to output
*/
void Lz77::Decompress(Buffer Input, Buffer Output)
{
	Utils *leb = new Utils;
	int lits = 0;
	int m = 0;
	int off = 0;
	int pos = 0;
	int out_pos = 0;

	while (pos < *Input.size)
	{
		pos += leb->DecodeLeb128(&lits, &Input.block[pos]);
		pos += leb->DecodeLeb128(&m, &Input.block[pos]);
		pos += leb->DecodeLeb128(&off, &Input.block[pos]);
		if (off != 0) // if offset is zero then we're done
		{
			/* copy literals */
			memcpy(&Output.block[out_pos], &Input.block[pos], lits);
			out_pos += lits;
			pos += lits;

			/* goto offset and copy matched data */
			for (int k = 0; k < m; k++) 
				Output.block[out_pos + k] = Output.block[(out_pos - off) + k];
			out_pos += m;
		}
		else
		{
			int remainder = *Input.size - pos;
			memcpy(&Output.block[out_pos], &Input.block[pos], remainder);
			out_pos += remainder;
			break;
		}
	}
	*Output.size = out_pos;
	delete leb;
}
