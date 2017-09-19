/*********************************************
* Embedded delta and linear prediction filters
* These are blockwise adaptive transforms which lower the entropy of complex semi-static structures
* 
* Definitions of compression arguments:
* -f0 : no filter, do not preprocess.
* -f1 : heuristic filter, detect the width of channels and pick whatever transform is the best, if any.
* -f2 : brute force, apply all possible transformations and pick the best one.
*
* There are 3 filter types:
* 1) delta : standard reorder of channels with delta encoder.
* 2) linear prediction : standard reorder of channels with linear prediction encoder.
* 3) inline delta : no reordering, deltas are encoded in place by a table. (context preserving, simd friendly)
*
* Each filter can encode a width of 1 to 32, width 0 means raw. 
* In total there are 96 filters + raw configuration, in brute force mode it tries all filters and picks the best, in heuristic mode
* it checks for repeated symbols at pos%32 in O(n) time via a stride histogram with sorted entropy calculation (generalized bwt).
**********************************************/
#include "filters.hpp"

void Filters::DeltaEncode(unsigned char *in, int len)
{
	unsigned char previous = 0, cur = 0;
	for(int i = 0; i < len; i++)
	{
		cur = in[i];
		in[i] = cur - previous;
		previous = cur;
	}
}

void Filters::DeltaDecode(unsigned char *in, int len)
{
	unsigned char previous = 0;
	for(int i = 0; i < len; i++)
	{
		previous = in[i] += previous;
	}
}

void Filters::UpdateWeight(unsigned char err, int *weight)
{
	const int upper = 0xff;
	const int rate = 6;
	*weight += (err - *weight) >> rate;
	assert(*weight >= -upper && *weight <= upper);
}

void Filters::LpcEncode(unsigned char *in, int len)
{
	int weight = 0;
	unsigned char p1 = 0;
	unsigned char p2 = 0;
	unsigned char cur = 0;
	unsigned char err = 0;
	for(int i = 0; i < len; i++)
	{
		cur = in[i];
		err = weight + (((p1 - p2) + p1) - cur);
		in[i] = err;
		UpdateWeight(err, &weight);
		p2 = p1;
		p1 = cur;
	}
}

void Filters::LpcDecode(unsigned char *in, int len)
{
	int weight = 0;
	unsigned char p1 = 0;
	unsigned char p2 = 0;
	unsigned char cur = 0;
	unsigned char err = 0;
	for(int i = 0; i < len; i++)
	{
		err = in[i];
		cur = weight + (((p1 - p2) + p1) - err);
		in[i] = cur;
		UpdateWeight(err, &weight);
		p2 = p1;
		p1 = cur;
	}
}

void Filters::Reorder(unsigned char *in, unsigned char *out, int width, int len)
{
	Index i, j, pos = 0;
	for(i = 0; i < width; i++)
		for(j = i; j < len; j += width)
			out[pos++] = in[j];
}

void Filters::Unreorder(unsigned char *in, unsigned char *out, int width, int len)
{
	Index i, j, pos = 0;
	for(i = 0; i < width; i++)
		for(j = i; j < len; j += width)
			out[j] = in[pos++];
}

void Filters::InlineDelta(unsigned char *in, unsigned char *out, int width, int len)
{
	unsigned char p[MAX_CHANNEL_WIDTH] = {0};

	Index i = 0;
	Index align = len % width;
	for(; i < align; i++) 
		out[i] = in[i]; 
	
	while(i < len) // process a bunch of deltas at once without reorder (cache friendly, simd optimizable, context preserving)
	{
		for(Index j = 0; j < width; j++)
		{
			out[i + j] = in[i + j] - p[j];
			p[j] = in[i + j];
		}
		i += width;
	}
}

void Filters::InlineUndelta(unsigned char *in, unsigned char *out, int width, int len)
{
	unsigned char p[MAX_CHANNEL_WIDTH] = {0};
	
	Index i = 0;
	Index align = len % width;
	for(; i < align; i++) 
		out[i] = in[i]; 
	
	while(i < len) // process a bunch of deltas at once without reorder
	{
		for(Index j = 0; j < width; j++)
		{
			out[i + j] = in[i + j] + p[j];
			p[j] = out[i + j];
		}
		i += width;
	}
}

/**
* Detect the width of a standard delta channel
*/
int Filters::FindStride(unsigned char *in, int len)
{
	Index stride = 0;
	Index dist[256] = {0};
	Index *hist = (Index*)calloc((MAX_CHANNEL_WIDTH + 1), sizeof(Index));
	unsigned char sym = 0;
	for(int i = 0; i < len; i++)
	{
		sym = in[i];
		stride = i - dist[sym];
		dist[sym] = i;
		hist[stride % (MAX_CHANNEL_WIDTH + 1)]++;
	}

	Index average = 0;
	for (int j = 0; j <= MAX_CHANNEL_WIDTH; j++)
		average += hist[j];
	average /= (MAX_CHANNEL_WIDTH + 1);
	
	Index smallest = 0;
	double min = hist[0];
	for (int j = 1; j <= MAX_CHANNEL_WIDTH; j++)
	{
		if (hist[j] > (average * 2) && hist[j] > min)
		{
			min = hist[j];
			smallest = j;
		}
	}
	free(hist);
	return smallest;
}

/**
* Detect the width of a linear prediction channel
*/
int Filters::FindProjection(unsigned char *in, int len)
{
	Index stride = 0;
	Index projection = 0;
	Index dist0[256] = {0};
	Index dist1[256] = {0};
	Index *hist = (Index*)calloc((MAX_CHANNEL_WIDTH + 1), sizeof(Index));
	unsigned char sym = 0;
	for(int i = 0; i < len; i++)
	{
		sym = in[i];
		stride = i - dist0[sym];
		projection = i - dist1[stride % 256];
		dist1[stride % 256] = i;
		dist0[sym] = i;
		hist[projection % (MAX_CHANNEL_WIDTH + 1)]++;
	}

	Index average = 0;
	for (int j = 0; j <= MAX_CHANNEL_WIDTH; j++)
		average += hist[j];
	average /= (MAX_CHANNEL_WIDTH + 1);
	
	double min = hist[0];
	Index smallest = 0;
	for (int j = 1; j <= MAX_CHANNEL_WIDTH; j++)
	{
		if (hist[j] > (average * 2) && hist[j] > min)
		{
			min = hist[j];
			smallest = j;
		}
	}
	free(hist);
	return smallest;
}

/**
* Structural modelling, detect deltas and fixed points within the input and encode it.
*/
void Filters::Encode(Buffer Input, Buffer Output, Options Opt)
{
	if(Opt.Filters < 0)
		Opt.Filters = 0;
	if(Opt.Filters > 2)
		Opt.Filters = 2;
	
	Utils *eCalc = new Utils;
	double *eScores[3];	
	eScores[0] = (double*)malloc(MAX_CHANNEL_WIDTH * sizeof(double)); // Standard delta
	eScores[1] = (double*)malloc(MAX_CHANNEL_WIDTH * sizeof(double)); // Linear prediction
	eScores[2] = (double*)malloc(MAX_CHANNEL_WIDTH * sizeof(double)); // Inline delta
	if(eScores[0] == NULL || eScores[1] == NULL || eScores[2] == NULL)
		Error("Failed to allocate filter prediction buffer!");
	
	// Read all of the input
	Index Outp = 0;
	int DeltaBlocks = 0, RawBlocks = 0;
	int PrevType = 0, PrevWidth = 0;
	for(Index i = 0; i < *Input.size;)
	{
		// Split input into chunks
		int len = ((i + FILTER_BLOCK_SIZE) < *Input.size) ? FILTER_BLOCK_SIZE : (*Input.size - i);
		
		int boostread = len;
		
		for(int k = 0; k <= MAX_CHANNEL_WIDTH; k++)
		{
			eScores[0][k] = 8.0f;
			eScores[1][k] = 8.0f;
			eScores[2][k] = 8.0f;
		}
		
		if(Opt.Filters == 2)
		{
			// Brute force standard delta, linear prediction, and raw entropy calculation
			#pragma omp parallel for num_threads(Opt.Threads)
			for(int Ch = 0; Ch <= MAX_CHANNEL_WIDTH; Ch++)
			{
				unsigned char *dbuf = (unsigned char*)malloc(FILTER_BLOCK_SIZE * sizeof(unsigned char));
				unsigned char *lbuf = (unsigned char*)malloc(FILTER_BLOCK_SIZE * sizeof(unsigned char));
				unsigned char *ibuf = (unsigned char*)malloc(FILTER_BLOCK_SIZE * sizeof(unsigned char));
				if(dbuf == NULL || lbuf == NULL || ibuf == NULL)
					Error("Failed to allocate filter prediction buffers!");
				if(Ch > 0)
				{
					Reorder(&Input.block[i], dbuf, Ch, boostread);
					memcpy(lbuf, dbuf, boostread);
					DeltaEncode(dbuf, boostread);
					LpcEncode(lbuf, boostread);
					InlineDelta(&Input.block[i], ibuf, Ch, boostread);
					eScores[0][Ch] = eCalc->CalculateMixedEntropy(dbuf, boostread);
					eScores[1][Ch] = eCalc->CalculateMixedEntropy(lbuf, boostread);
					eScores[2][Ch] = eCalc->CalculateMixedEntropy(ibuf, boostread);
				}
				else
				{
					eScores[0][Ch] = eCalc->CalculateMixedEntropy(&Input.block[i], boostread);
				}
				free(dbuf);
				free(lbuf);
				free(ibuf);
			}
		}
		if(Opt.Filters == 1)
		{
			double pconfig;
			#pragma omp parallel sections num_threads(Opt.Threads)
			{
				// Compute raw entropy
				#pragma omp section
				{
					eScores[0][0] = eCalc->CalculateSortedEntropy(&Input.block[i], boostread);
				}
				// Compute delta entropy
				#pragma omp section
				{
				   	unsigned char *dbuf = (unsigned char*)malloc(FILTER_BLOCK_SIZE * sizeof(unsigned char));
					if(dbuf == NULL)
						Error("Failed to allocate filter prediction buffer!");
					int Ch = FindStride(&Input.block[i], len);
					if(Ch > 0)
					{
						Reorder(&Input.block[i], dbuf, Ch, boostread);
						DeltaEncode(dbuf, boostread);
						eScores[0][Ch] = eCalc->CalculateSortedEntropy(dbuf, boostread);
					}
					free(dbuf);
				}
				// Compute linear prediction entropy
				#pragma omp section
				{
					unsigned char *lbuf = (unsigned char*)malloc(FILTER_BLOCK_SIZE * sizeof(unsigned char));
					if(lbuf == NULL)
						Error("Failed to allocate filter prediction buffer!");
					int Ch = FindProjection(&Input.block[i], len);
					if(Ch > 0)
					{
						Reorder(&Input.block[i], lbuf, Ch, boostread);
						LpcEncode(lbuf, boostread);
						eScores[1][Ch] = eCalc->CalculateSortedEntropy(lbuf, boostread);
					}
					free(lbuf);
				}
				
				// Compute inline delta entropy
				#pragma omp section
				{
					unsigned char *ibuf = (unsigned char*)malloc(FILTER_BLOCK_SIZE * sizeof(unsigned char));
					if(ibuf == NULL)
						Error("Failed to allocate filter prediction buffer!");
					int Ch = FindStride(&Input.block[i], len);
					if(Ch > 0)
					{
						InlineDelta(&Input.block[i], ibuf, Ch, boostread);
						eScores[2][Ch] = eCalc->CalculateSortedEntropy(ibuf, boostread);
					}
					free(ibuf);
				}
				
				// Compute entropy of previous block configuration on current block
				// Due to the previous state not being taken into account by other threads this can cause scheduling issues when writing the entropy score if it collides with another, 'pconfig' fixes that.
				#pragma omp section
				{
					unsigned char *pbuf = (unsigned char*)malloc(FILTER_BLOCK_SIZE * sizeof(unsigned char));
					if(pbuf == NULL)
						Error("Failed to allocate filter prediction buffer!");
					
					if(PrevWidth > 0)
					{
						Reorder(&Input.block[i], pbuf, PrevWidth, boostread);
					}
					if(PrevType)
					{
						LpcEncode(pbuf, boostread);
					}
					else
					{
						DeltaEncode(pbuf, boostread);
					}
					pconfig = eCalc->CalculateSortedEntropy(pbuf, boostread);
					free(pbuf);
				}
			}
			if(eScores[PrevType][PrevWidth] == 8.0f)
				eScores[PrevType][PrevWidth] = pconfig;
		}

		// Find the best filter configuration (channel width and type)
		unsigned char smallest = 0;
		unsigned char type = 0;
		double min = eScores[0][smallest];
		for(int k = 0; k < MAX_SUPPORTED_CONFIGURATION; k++)
		{
			for (int j = 1; j <= MAX_CHANNEL_WIDTH; j++)
			{
				if (eScores[k][j] < min)
				{
					min = eScores[k][j];
					smallest = j;
					type = k;
				}
			}
		}
		//printf("Using filter %i with a width of %i has entropy %.2f\n", type, smallest, min);
		
		unsigned char *buf = (unsigned char*)malloc(FILTER_BLOCK_SIZE * sizeof(unsigned char));
		if(buf == NULL)
				Error("Failed to allocate filter prediction buffer!");
			
		if(type >= MAX_SUPPORTED_CONFIGURATION || smallest > MAX_CHANNEL_WIDTH)
			Error("Filter is trying to encode from an unsupported configuration!");
			
		if(smallest > 0)
		{
			switch(type)
			{
				case 0 : // Delta
				{
					Reorder(&Input.block[i], buf, smallest, len);
					DeltaEncode(buf, len);
				} break;
				case 1 : // LPC
				{
					Reorder(&Input.block[i], buf, smallest, len);
					LpcEncode(buf, len);
				} break;
				case 2 : // Inline Delta
				{
					InlineDelta(&Input.block[i], buf, smallest, len);
				} break;
			}

			Output.block[Outp++] = type;
			Output.block[Outp++] = smallest;
			memcpy(&Output.block[Outp], buf, len * sizeof(unsigned char));
			DeltaBlocks++;
		}
		else
		{
			Output.block[Outp++] = 0;	
			Output.block[Outp++] = 0;
			memcpy(&Output.block[Outp], &Input.block[i], len * sizeof(unsigned char));
			RawBlocks++;
		}
		free(buf);
		PrevType = type;
		PrevWidth = smallest;
		Outp += len;
		i += len;
	}
	//printf("\ndelta blocks: %i, raw blocks: %i\n", DeltaBlocks, RawBlocks);
	delete eCalc;
	free(eScores[0]);
	free(eScores[1]);
	free(eScores[2]);
	*Output.size = Outp;
}

void Filters::Decode(Buffer Input, Buffer Output)
{
	unsigned char *dbuf = (unsigned char*)malloc(FILTER_BLOCK_SIZE * sizeof(unsigned char));
	if(dbuf == NULL)
		Error("Failed to allocate filter prediction buffer!");
	
	Index Outp = 0;
	for(Index i = 0; i < *Input.size;)
	{
		unsigned char type = Input.block[i++];
		unsigned char channel = Input.block[i++];

		if(type >= MAX_SUPPORTED_CONFIGURATION || channel > MAX_CHANNEL_WIDTH)
			Error("Filter is trying to decode from unsupported configuration!");
		
		int len = ((i + FILTER_BLOCK_SIZE) < *Input.size) ? FILTER_BLOCK_SIZE : (*Input.size - i);
		if(channel > 0)
		{
			switch(type)
			{
				case 0 : // Delta
				{
					memcpy(dbuf, &Input.block[i], len * sizeof(unsigned char));
					DeltaDecode(dbuf, len);
					Unreorder(dbuf, &Output.block[Outp], channel, len);
				} break;
				case 1 : // LPC
				{
					memcpy(dbuf, &Input.block[i], len * sizeof(unsigned char));
					LpcDecode(dbuf, len);
					Unreorder(dbuf, &Output.block[Outp], channel, len);
				} break;
				case 2 : // Inline Delta
				{
					InlineUndelta(&Input.block[i], &Output.block[Outp], channel, len);
				} break;
			}
		}
		else
		{
			memcpy(&Output.block[Outp], &Input.block[i], len * sizeof(unsigned char));
		}
		Outp += len;
		i += len;
	}
	
	free(dbuf);
	*Output.size = Outp;
}
