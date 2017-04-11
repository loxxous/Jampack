/*********************************************
* LZ77 COMPRESSION AND DECOMPRESSION CODE
**********************************************/
#include "lz77.hpp"

	inline int Lz77::HashLong(int ctx)
	{
		return (ctx * 0x9E3779B1) >> (32 - DUPE_HBITS);
	}
	
	// Used for hashing short 4 byte contexts
	inline int Lz77::Hash32(unsigned char *p)
	{
		return ((*((const unsigned int*)p)) * 0x9E3779B1) >> (32 - SHORT_HBITS);
	}

	/* determines if the current state is compressible */
	bool Lz77::compressible(int look_ahead, int match_len, int offset, int minmatch)
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
	*   Encodes compressed variable length codes for literals, matches, and offsets as extensions(1+7 bits)...
	*   All extensions start as one byte and grow as needed, limited to 4 growths.
	*   Each extension can either carry the previous 7 bits over or stop.
	*
	*   After encoding the match information the new position in the output is returned.
	*/
	int Lz77::encode_leb128(int look_ahead, int match_length, int offset, unsigned char *buf)
	{
		short i = 0;

		int type[] = { look_ahead, match_length, offset };

		/**
		*   Encode literals, match length, and offset
		*/
		for (int t = 0; t < 3; t++)
		{
			/* 0 to 127 */
			if (type[t] < constants[0])
			{
				buf[i] = (type[t] | 0x80);
				i += 1;
			}
			/* 127 to 16KB */
			else if (type[t] < constants[1])
			{
				type[t] -= constants[0];
				buf[i] = (type[t] >> 7) & 0x7F;
				buf[i + 1] = (type[t] & 0x7F) | 0x80;
				i += 2;
			}
			/* 16KB to 2MB */
			else if (type[t] < constants[2])
			{
				type[t] -= constants[1];
				buf[i] = (type[t] >> 14) & 0x7F;
				buf[i + 1] = (type[t] >> 7) & 0x7F;
				buf[i + 2] = (type[t] & 0x7F) | 0x80;
				i += 3;
			}
			/* 2MB to 256 MB */
			else
			{
				type[t] -= constants[2];
				buf[i] = (type[t] >> 21) & 0x7F;
				buf[i + 1] = (type[t] >> 14) & 0x7F;
				buf[i + 2] = (type[t] >> 7) & 0x7F;
				buf[i + 3] = ((type[t]) & 0x7F) | 0x80;
				i += 4;
			}
		}

		return i;
	}

	/**
	*   Compress input block to output block
	*/
	void Lz77::compress(Buffer Input, Buffer Output, int minmatch)
	{
		MatchLong *DupeTable;
		DupeTable = (struct MatchLong*)calloc(LZ_DUPE_ELEMENTS, sizeof(struct MatchLong)); if (DupeTable == NULL) Error("Couldn't allocate dedupe table!");
		
		int shift = (minmatch > 32) ? 1 : 32 / minmatch;

		int h = 0;
		int ctx = 0;
		int l = 0;
		int pos = 0;
		int out_pos = 0;

		while (pos < *Input.size)
		{
			h = HashLong(ctx);
			int prev_pos = DupeTable[h].pos;

			if ((ctx == DupeTable[h].ctx) && ((pos - prev_pos) > 0) && (pos - prev_pos) < MAX_RUN)
			{
				int m = 0;
				int off = pos - prev_pos;

				while (Input.block[prev_pos + m] == Input.block[pos + m] && (pos + m + minmatch) < *Input.size && m < MAX_RUN) { m++; }

				if (compressible(l, m, off, minmatch))
				{
					out_pos += encode_leb128(l, m, off, &Output.block[out_pos]);
					memcpy(&Output.block[out_pos], &Input.block[pos - l], l);

					for (int i = 0; i < m; i++)
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
		out_pos += encode_leb128(0, 0, 0, &Output.block[out_pos]);
		memcpy(&Output.block[out_pos], &Input.block[pos - l], l);
		out_pos += l;

		*Output.size = out_pos;
		
		free(DupeTable);
	}

	/**
	* 	Decoding is done by maintaining the position of the encoded chain and reading the information from them.
	*	read chain -> copy literals, copy match from offset... (repeat)
	*
	*       If uppermost bit is 1 then stop, if 0 then append more bits.
	*
	* 	If the value is 0 in the match field then there's no more matches and only literals need to be copied out.
	*	After decoding the match information the new position of the input is returned
	*/
	int Lz77::decode_leb128(int *look_ahead, int *match_length, int *offset, unsigned char *buf)
	{
		short i = 0;

		int type[] = { 0, 0, 0 };

		/*
		*   Decode literals, match length, and offset
		*/
		for (int t = 0; t < 3; t++)
		{
			int d = 0;
			while((buf[i + d] & 0x80) == 0)
			{				type[t] = (type[t] << 7) | buf[i + d];
				d++;
			}
			type[t] = (type[t] << 7) | buf[i + d] & 0x7F;
			if(d > 0) type[t] += constants[d - 1];
			i += d + 1;
		}

		*look_ahead = type[0];
		*match_length = type[1];
		*offset = type[2];

		return (type[1] == 0) ? 0 : i; // check for end-of-chain code
	}

	/**
	*   Decompress input to output
	*/
	void Lz77::decompress(Buffer Input, Buffer Output)
	{
		int lits = 0;
		int m = 0;
		int off = 0;
		int pos = 0;
		int out_pos = 0;

		while (pos < *Input.size)
		{
			int token = 0;
			if ((token = decode_leb128(&lits, &m, &off, &Input.block[pos])) != 0)
			{
				pos += token;
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
				/* flush end of block */
				pos += 3;
				int remainder = *Input.size - pos;
				memcpy(&Output.block[out_pos], &Input.block[pos], remainder);
				out_pos += remainder;
				break;
			}
		}
		*Output.size = out_pos;
	}