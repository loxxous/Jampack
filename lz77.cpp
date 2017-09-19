/*********************************************
* Lz77 compression and decompression code
*
* Lazy byte aligned lz77 with anti-contextual parsing. (It compresses the anti-context, eg: positional contexts, repeat distances, sparse contexts, all the things bwt cannot handle)
**********************************************/
#include "lz77.hpp"

/**
* Initialize the leb128 reader/writer  
*/
Lz77::Lz77()
{
	Leb = new Utils();
}

/**
* Free leb128 reader/writer 
*/
Lz77::~Lz77()
{
	delete Leb;
}

/**
* Load 4 bytes from a pointer
*/
inline unsigned int Lz77::Load32(unsigned char *ptr)
{
	return (ptr[0] << 24 | ptr[1] << 16 | ptr[2] << 8 | ptr[3]);
}

/**
* Hash 4 bytes from a pointer
*/
inline unsigned int Lz77::Hash32(unsigned char *ptr)
{
	return (Load32(ptr) * 0x9E3779B1) >> (32 - HASH_BITS);
}

/**
* Compute a hash for some arbitrary value
*/
inline unsigned int Lz77::Hash(unsigned int v)
{
	return (v * 0x9E3779B1) >> (32 - HASH_BITS);
}

/**
* Write a compressed token containing the match, length, and offset.
* MMMMM_LLL encodes L in range 0 to 6, M in range 4-36, with 15 as a flag to append more bits using leb128 encoding (0 to 32GB, though limited to 2GB).
* WriteToken encodes all this and returns the new position in the buffer.
*/
Index Lz77::WriteToken(unsigned char *out, Index match, Index literal, Index offset)
{
	unsigned int pos = 0;
	unsigned char token = 0; 
	
	match -= MIN_MATCH;
	
	// Encode token and offset
	out[pos++] = token = (__min(match, 31) << 3 ) | __min(literal, 7);
	pos += Leb->EncodeLeb128(offset, &out[pos]);
	
	// Optionally encode extensions
	if((token >> 3) == 31)
		pos += Leb->EncodeLeb128(match - 31, &out[pos]);	
	if((token & 7) == 7)
		pos += Leb->EncodeLeb128(literal - 7, &out[pos]);
	return pos;
}

/**
* Read a compressed token containing the match, length, and offset
*/
Index Lz77::ReadToken(unsigned char *in, Index *match, Index *literal, Index *offset)
{
	unsigned int pos = 0;
	unsigned char token = 0; 
	*match = 0; 
	*literal = 0;
	
	// Decode token and offset
	token = in[pos++];
	pos += Leb->DecodeLeb128(offset, &in[pos]);
	
	*match = token >> 3;
	if(*match == 31)
	{
		pos += Leb->DecodeLeb128(match, &in[pos]);
		*match += 31;
	}
	*match += MIN_MATCH;
	
	*literal = token & 7;
	if(*literal == 7)
	{
		pos += Leb->DecodeLeb128(literal, &in[pos]);
		*literal += 7;
	}
	
	return pos;
}

/**
* Return the compression ratio of the current token
*/
float Lz77::Compressible(Index match, Index literal, Index offset)
{
	// Calculate the size of the token
	int cost = 1;
	cost += ((match - MIN_MATCH) < 31) ? 0 : Leb->SizeOfValue(match - MIN_MATCH - 31);
	cost += (literal < 7) ? 0 : Leb->SizeOfValue(literal - 7);
	cost += Leb->SizeOfValue(offset);

	if(match < MIN_MATCH || match <= cost)
		return 0;
	
	return (float)match / (float)cost;
}

/**
* Compress input block to output block
* Note: unlike any other lz77 encoder this uses anti-context parsing, basically any non-markovian contexts and high lcp strings are encoded here.
* Arguments: -m0 = dedupe, -m1 = hash chain positional modeling, -m2 full anti-context modeling.
*/
void Lz77::Compress(Buffer Input, Buffer Output, Options Opt)
{
	if(Opt.MatchFinder < 0)
		Opt.MatchFinder = 0;
	if(Opt.MatchFinder > 2)
		Opt.MatchFinder = 2;
	int mode = Opt.MatchFinder;

	if(mode == 2) // smallest but slowest (activated with -m2 flag) suffix array modeling, uses ISA, SA, and LPC for O(n)~ish optimal parsing 
	{
		Index *SA = (Index*)malloc(*Input.size * sizeof(Index)); 
		Index *ISA = (Index*)malloc(*Input.size * sizeof(Index)); 
		if(SA == NULL || ISA == NULL)
			Error("Failed to allocate lz77 suffix array components!");
		
		if(divsufsort(Input.block, SA, *Input.size) != 0) 
			Error("Failure computing the Suffix Array!");

		for(Index i = 0; i < *Input.size; i++)
			ISA[SA[i]] = i;
		
		CyclicHashHistory *ChhmOffset = new CyclicHashHistory(TOKEN_BUFFER_SIZE);
		CyclicHashHistory *ChhmMatch = new CyclicHashHistory(TOKEN_BUFFER_SIZE);
		
		struct Token
		{
			Index offset;
			Index match;
			Index position;
		};
		
		Token *TokenBuffer = (Token*)calloc(TOKEN_BUFFER_SIZE, sizeof(Token));
		if(TokenBuffer == NULL)
			Error("Failed to allocate token buffer!");

		unsigned int h = 0;
		Index pos = 0, lit = 0; // positions in match finder
		Index bpos = 0, bbpos = 0; // buffer positions
		Index out_pos = 0;
		Index TokenIterator = 0;
		while (pos < *Input.size)
		{
			// Find all matches in the current token chunk
			while (pos < *Input.size && TokenIterator < TOKEN_BUFFER_SIZE)
			{
				Index forward = 0;
				Index len = 0;
				Index off = 0;
				float lowest_cost = 1.0f;

				for(Index k = 0; k < MIN_MATCH; k++)
				{
					Index distance = 0;
					Index cpos = pos + k;
					Index ptr = ISA[cpos]; // Get position in suffix array and find matches
					Index cc = 0;
					
					Index bucket = 1 << 12;
					Index itr = __max(ptr - bucket, 0); // leftmost node in SA
					Index limit = __min(ptr + bucket, *Input.size); // rightmost node in SA
					while(1)
					{
						if(itr > limit)
							break;
						Index ppos = SA[itr];
						if(ppos < cpos)
						{
							// Search forward
							Index match = 0;
							while (Input.block[ppos + match] == Input.block[cpos + match] && (cpos + match + MIN_MATCH) < *Input.size)
								match++;
							Index curoff = cpos - ppos;
							float ratio = Compressible(match, lit - k, curoff);
							if((ratio > lowest_cost || match >= DUPE_MATCH) )
							{
								lowest_cost = ratio;
								len = match;
								off = curoff;
								forward = k;
							}
						}
						itr++;
					}
				}
				if(lowest_cost > 1.0 || len > DUPE_MATCH)
				{
					//Merge the match data
					pos += forward; // correct current position
					
					// Store the best match in the current position
					TokenBuffer[TokenIterator].match = len;
					TokenBuffer[TokenIterator].offset = off;
					TokenBuffer[TokenIterator].position = pos;
					TokenIterator++;

					pos += len;
					lit = 0;
				}
				pos++;
				lit++;
				if(pos % 1024 == 0)
					printf(" %.2f%%\r", ((double)pos / (double)*Input.size) * 100); // SA match finding is very slow so show progress so user doesn't give up hope
			}
			
			// Use a fast statistical model (chhm) to model the token chunk before encoding anything (find anti-contexts)
			for (Index i = 0; i < TokenIterator; i++)
			{
				ChhmOffset->Update(TokenBuffer[i].offset);
				ChhmMatch->Update(TokenBuffer[i].match);
			}
			ChhmOffset->BuildModel();
			ChhmMatch->BuildModel();
			
			// Figure out the best representation of the tokens using the chhm model, TODO encode only the opposite of stable context tokens (the anti-context)
			// Currently encodes positional redundancies
			for (Index i = 0; i < TokenIterator; i++)
			{
				Index match = TokenBuffer[i].match;
				Index offset = TokenBuffer[i].offset;
				Index position = TokenBuffer[i].position;

				// Encode buffer directly 
				bpos = TokenBuffer[i].position;
				if((ChhmOffset->FindPeaks(offset) || ChhmOffset->FindPeaks(match))|| match > DUPE_MATCH) // Only encode induced contexts and long matches
				{
					Index literal = position - bbpos; // current position minus last encoded token position after match
					out_pos += WriteToken(&Output.block[out_pos], match, literal, offset);
					memcpy(&Output.block[out_pos], &Input.block[position - literal], literal);
					out_pos += literal;
					bpos += match;
					bbpos = position + match;
				}
			}
			ChhmOffset->CleanModel();
			ChhmMatch->CleanModel();
			TokenIterator = 0;
		}
		// flush out remaining data
		Index remainder = pos - bbpos;
		out_pos += WriteToken(&Output.block[out_pos], MIN_MATCH, MIN_MATCH, 0);
		memcpy(&Output.block[out_pos], &Input.block[*Input.size - remainder], remainder);
		out_pos += remainder;
		*Output.size = out_pos;
		free(SA);
		free(ISA);
		free(TokenBuffer);
		delete ChhmOffset;
		delete ChhmMatch;
	}
	// Hash chain match finding 
	// After TOKEN_BUFFER_SIZE tokens have been loaded (via lazy parsing and hash chain) we use the Chhm model to figure out which specific tokens are worth encoding,
	// random tokens are useless and imply that there is context (which bwt can handle far better), repeating tokens imply an underlying structure that bwt cannot see and is deemed worth compressing with lz77.
	else if(mode == 1) // hash chain match finding (activated with -m1 flag)
	{
		CyclicHashHistory *ChhmOffset = new CyclicHashHistory(TOKEN_BUFFER_SIZE);
		CyclicHashHistory *ChhmMatch = new CyclicHashHistory(TOKEN_BUFFER_SIZE);
		
		struct Token
		{
			Index offset;
			Index match;
			Index position;
		};
		
		Token *TokenBuffer = (Token*)calloc(TOKEN_BUFFER_SIZE, sizeof(Token));
		if(TokenBuffer == NULL)
			Error("Failed to allocate token buffer!");
		
		Index Window = *Input.size;
		Index *chain = (Index*)malloc(Window * sizeof(Index)); // Hash chained table
		Index *table = (Index*)malloc(HASH_SIZE * sizeof(Index)); // Auxiliary hash
		if(table == NULL || chain == NULL)
			Error("Failed to allocate lz77 match table!");
		
		memset(table, 0, HASH_SIZE * sizeof(Index));
		memset(chain, 0, Window * sizeof(Index));

		unsigned int h = 0;
		Index pos = 0, lit = 0; // positions in match finder
		Index bpos = 0, bbpos = 0; // buffer positions
		Index out_pos = 0;
		Index TokenIterator = 0;
		while (pos < *Input.size)
		{
			// Find all matches in the current token chunk
			while (pos < *Input.size && TokenIterator < TOKEN_BUFFER_SIZE)
			{
				Index back = 0;
				Index forward = 0;
				Index len = 0;
				Index off = 0;
				float lowest_cost = 1.0f;
				
				for(Index k = 0; k < MIN_MATCH; k++)
				{
					h = Hash32(&Input.block[pos]);
					if (table[h] > 0 && (pos + k) < *Input.size)
					{
						Index cpos = pos + k;
						Index ppos = table[h];
						Index match_forwards = 0;
						Index match_backwards = 0;
						Index chain_len = 32;
						Index miss = 0;
						Index winstart = __max(pos - Window, 0);
						while(ppos > winstart)
						{
							Index distance = cpos - ppos;
							match_forwards = 0;
							match_backwards = 0;
							
							if(Load32(&Input.block[ppos]) == Load32(&Input.block[cpos]))
							{
								// Search backwards in case of hash miss
								while (Input.block[ppos - match_backwards - 1] == Input.block[cpos - match_backwards - 1] && (ppos - match_backwards - 1) > 0 && (match_backwards < lit)) 
									match_backwards++;
							
								// Search forward
								while (Input.block[ppos + match_forwards] == Input.block[cpos + match_forwards] && (cpos + match_forwards + MIN_MATCH) < *Input.size)
									match_forwards++;

								float ratio = Compressible(match_forwards + match_backwards, lit + (match_backwards - k), distance);
								if((ratio > lowest_cost || (match_forwards + match_backwards) >= DUPE_MATCH) ) //|| Chhm->Peak(distance)
								{
									lowest_cost = ratio;
									len = match_forwards;
									back = match_backwards;
									off = distance;
									forward = k;
								}
								if(match_forwards > DUPE_MATCH)
									break;
								miss = 0;
							}
							else
								miss++;

							if(!--chain_len || !(chain_len >> miss)) // Break even point (in terms of energy cost), kill search after a certain number of consecutive misses
								break;
							ppos = chain[ppos % Window]; // Keep searching backwards through the chain for matches
						}
					}
				}
				if(lowest_cost > 1.0 || (len + back ) > DUPE_MATCH) //  && ((lit -= back - forward) != 0)
				{
					//Merge the match data
					len += back; // increase match length
					pos -= back - forward; // correct current position

					// Store the best match in the current position
					TokenBuffer[TokenIterator].match = len;
					TokenBuffer[TokenIterator].offset = off;
					TokenBuffer[TokenIterator].position = pos;
					TokenIterator++;
	
					for (Index i = 0; i < len; i++)
					{
						h = Hash32(&Input.block[pos + i]);
						chain[(pos + i) % Window] = table[h];
						table[h] = pos + i;
					}
					
					pos += len;
					lit = 0;
				}
				h = Hash32(&Input.block[pos]);
				chain[pos % Window] = table[h];
				table[h] = pos;
				pos++;
				lit++;
			}
			
			// Use a fast statistical model (chhm) to model the token chunk before encoding anything
			for (Index i = 0; i < TokenIterator; i++)
			{
				ChhmOffset->Update(TokenBuffer[i].offset);
				ChhmMatch->Update(TokenBuffer[i].match);
			}
			ChhmOffset->BuildModel();
			ChhmMatch->BuildModel();
			
			// Figure out the best representation of the tokens using the chhm model, encode only the useful tokens
			for (Index i = 0; i < TokenIterator; i++)
			{
				Index match = TokenBuffer[i].match;
				Index offset = TokenBuffer[i].offset;
				Index position = TokenBuffer[i].position;

				// Encode buffer directly 
				bpos = TokenBuffer[i].position;
				if((ChhmOffset->FindPeaks(offset) || ChhmOffset->FindPeaks(match))|| match > DUPE_MATCH) // Only encode induced contexts and long matches
				{
					Index literal = position - bbpos; // current position minus last encoded token position after match
					out_pos += WriteToken(&Output.block[out_pos], match, literal, offset);
					memcpy(&Output.block[out_pos], &Input.block[position - literal], literal);
					out_pos += literal;
					bpos += match;
					bbpos = position + match;
				}
			}
			ChhmOffset->CleanModel();
			ChhmMatch->CleanModel();
			TokenIterator = 0;
		}
		// flush out remaining data
		Index remainder = pos - bbpos;
		out_pos += WriteToken(&Output.block[out_pos], MIN_MATCH, MIN_MATCH, 0);
		memcpy(&Output.block[out_pos], &Input.block[*Input.size - remainder], remainder);
		out_pos += remainder;
		*Output.size = out_pos;
		free(chain);
		free(table);
		free(TokenBuffer);
		delete ChhmOffset;
		delete ChhmMatch;
		
		/* // Standard lz77, no anti-context modeling
		while (pos < *Input.size)
		{
			Index back = 0;
			Index forward = 0;
			Index len = 0;
			Index off = 0;
			float lowest_cost = 1.0f;
			
			for(Index k = 0; k < MIN_MATCH; k++)
			{
				h = Hash32(&Input.block[pos]);
				if (table[h] > 0 && (pos + k) < *Input.size)
				{
					Index cpos = pos + k;
					Index ppos = table[h];
					Index match_forwards = 0;
					Index match_backwards = 0;
					Index chain_len = 32 / mode;
					Index miss = 0;
					Index winstart = __max(pos - Window, 0);
					while(ppos > winstart)
					{
						Index distance = cpos - ppos;
						match_forwards = 0;
						match_backwards = 0;
						
						if(Load32(&Input.block[ppos]) == Load32(&Input.block[cpos]))
						{
							// Search backwards in case of hash miss
							while (Input.block[ppos - match_backwards - 1] == Input.block[cpos - match_backwards - 1] && (ppos - match_backwards - 1) > 0 && (match_backwards < lit)) 
								match_backwards++;
						
							// Search forward
							while (Input.block[ppos + match_forwards] == Input.block[cpos + match_forwards] && (cpos + match_forwards + MIN_MATCH) < *Input.size)
								match_forwards++;

							float ratio = Compressible(match_forwards + match_backwards, lit + (match_backwards - k), distance);
							if((ratio > lowest_cost || (match_forwards + match_backwards) >= DUPE_MATCH) ) //|| Chhm->Peak(distance)
							{
								lowest_cost = ratio;
								len = match_forwards;
								back = match_backwards;
								off = distance;
								forward = k;
							}
							if(match_forwards > DUPE_MATCH)
								break;
							miss = 0;
						}
						else
							miss++;

						if(!--chain_len || !(chain_len >> miss)) // Break even point (in terms of energy cost), kill search after a certain number of consecutive misses
							break;
						ppos = chain[ppos % Window]; // Keep searching backwards through the chain for matches
					}
				}
			}
			//if(lowest_cost > 1.0) //  && ((lit -= back - forward) != 0)
			if((lowest_cost > 1.0) || (len + back ) > DUPE_MATCH) //  && ((lit -= back - forward) != 0)
			{
				//Merge the match data
				len += back; // increase match length
				lit -= back - forward; // correct literal run
				pos -= back - forward; // correct current position

				//printf("Token: match length: %i, literal run: %i, offset %i, histo: %i\n", len, lit, off, Chhm->GetHistory(off));
				out_pos += WriteToken(&Output.block[out_pos], len, lit, off);
				memcpy(&Output.block[out_pos], &Input.block[pos - lit], lit);

				out_pos += lit;
				
				for (Index i = 0; i < len; i++)
				{
					h = Hash32(&Input.block[pos + i]);
					chain[(pos + i) % Window] = table[h];
					table[h] = pos + i;
				}
				
				pos += len;
				lit = 0;
			}
			// store the best representation in the Chhm model, use this to find better matches
			h = Hash32(&Input.block[pos]);
			chain[pos % Window] = table[h];
			table[h] = pos;
			pos++;
			lit++;
			
		}
		// flush out remaining data
		out_pos += WriteToken(&Output.block[out_pos], MIN_MATCH, MIN_MATCH, 0);
		memcpy(&Output.block[out_pos], &Input.block[*Input.size - lit], lit);
		out_pos += lit;
		*Output.size = out_pos;
		free(chain);
		free(table);
		free(TokenBuffer);
		delete Chhm;
		*/
	}
	// Fast dedupe (activated with -m0 flag)
	else
	{		
		Index *table = (Index*)malloc(HASH_SIZE * sizeof(Index)); // Auxiliary hash
		if(table == NULL)
			Error("Failed to allocate lz77 dedupe table!");
		memset(table, 0, HASH_SIZE * sizeof(Index));

		int shift = (DUPE_MATCH > 32) ? 1 : 32 / DUPE_MATCH;
		unsigned int h = 0;
		unsigned int cxt = 0;
		Index pos = 0;
		Index lit = 0;
		Index out_pos = 0;
		while (pos < *Input.size)
		{
			Index back = 0;
			Index len = 0;
			Index off = 0;
			float lowest_cost = 0;

			h = Hash(cxt);
			if (table[h] > 0)
			{
				Index cpos = pos;
				Index ppos = table[h];
				Index match_forwards = 0;
				Index match_backwards = 0;
				// Search backwards in case of hash miss
				while (Input.block[ppos - match_backwards - 1] == Input.block[cpos - match_backwards - 1] && (ppos - match_backwards - 1) > 0 && (match_backwards < lit)) 
					match_backwards++;
				
				// Search forward
				while (Input.block[ppos + match_forwards] == Input.block[cpos + match_forwards] && (cpos + match_forwards + MIN_MATCH) < *Input.size)
					match_forwards++;
					
				if((match_forwards + match_backwards) >= DUPE_MATCH)
				{
					lowest_cost = 1;
					len = match_forwards;
					back = match_backwards;
					off = cpos - ppos;
				}
			}

			// 1.0f is the minimum compression threshold, threshold of 4.0 would force it to only encode matches which have a compression ratio of 25% (1/4 of the original size)
			if(lowest_cost > 0) 
			{
				//Merge the match data
				len += back; // increase match length
				lit -= back; // correct literal run
				pos -= back; // correct current position

				out_pos += WriteToken(&Output.block[out_pos], len, lit, off);
				memcpy(&Output.block[out_pos], &Input.block[pos - lit], lit);
				
				out_pos += lit;
				
				for (Index i = 0; i < len; i++)
				{
					h = Hash(cxt);
					table[h] = pos;
					cxt = (cxt << shift) ^ Input.block[pos + DUPE_MATCH + i];
				}
				
				pos += len;
				lit = 0;
			}
			// else add position to table and move forward one
			h = Hash(cxt);
			table[h] = pos;
			cxt = (cxt << shift) ^ Input.block[pos + DUPE_MATCH];
			pos++;
			lit++;
		}
		// flush out remaining data
		out_pos += WriteToken(&Output.block[out_pos], MIN_MATCH, MIN_MATCH, 0);
		memcpy(&Output.block[out_pos], &Input.block[pos - lit], lit);
		out_pos += lit;
		*Output.size = out_pos;
		free(table);
	}
}

/**
* Copy 'length' bytes from 'src' to 'dest', assumes 'dest' and 'src' are part of the same alloc'd space.
* Note: this is a left to right copy algorithm, replaces memmove
*/
void Lz77::FastCopyOverlap(unsigned char *dest, unsigned char *src, Index length)
{
	Index dist = dest - src;
	Index limit = __min(length, (dist >= 4) ? dist : 0); // How many bytes we can copy before we hit the overlap boundary
	while(limit >= 4)
	{
		((unsigned int*)dest)[0] = ((unsigned int*)src)[0];
		length -= 4;
		limit -= 4;
		dest += 4;
		src += 4;
	}
	while(length)
	{
		dest[0] = src[0];
		length--;
		dest++;
		src++;
	}
}

/**
* Memcpy replacement, assumes src and dest are from two seperate areas in memory
*/
void Lz77::FastCopy(unsigned char *dest, unsigned char *src, Index length)
{
	while(length >= 8)
	{
		((unsigned int*)dest)[0] = ((unsigned int*)src)[0];
		((unsigned int*)dest)[1] = ((unsigned int*)src)[1];
		length -= 8;
		dest += 8;
		src += 8;
	}
	while(length)
	{
		dest[0] = src[0];
		length--;
		dest++;
		src++;
	}
}

/**
* Decompress input to output
*/
void Lz77::Decompress(Buffer Input, Buffer Output)
{
	Index lit = 0;
	Index len = 0;
	Index off = 0;
	Index pos = 0;
	Index out_pos = 0;
	while (pos < *Input.size)
	{
		pos += ReadToken(&Input.block[pos], &len, &lit, &off);
		if (off) // while offset isn't zero (end of lz77 code)
		{
			// copy literals
			FastCopy(&Output.block[out_pos], &Input.block[pos], lit);
			out_pos += lit;
			pos += lit;
			
			#ifndef NDEBUG
			// make sure input is valid
			if(pos >= *Input.size || out_pos - off < 0)
				Error("Invalid lz77 token, caught attempt to read outside of the allocated buffer!");
			#endif
			
			// goto offset and copy matched data
			FastCopyOverlap(&Output.block[out_pos], &Output.block[out_pos - off], len);
			out_pos += len;
		}
		else
		{
			Index remainder = *Input.size - pos;
			FastCopy(&Output.block[out_pos], &Input.block[pos], remainder);
			out_pos += remainder;
			break;
		}
	}
	*Output.size = out_pos;
}
