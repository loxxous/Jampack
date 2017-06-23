/*********************************************
* Embedded delta and linear prediction filters 
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

/**
* Structural modelling, detect deltas and fixed points within the input and encode it
* Acceleration is an optional parameter that restricts the filter to test only a small portion of each block.
* Acceleration of 1 means use the most aggressive filtering, anything higher will run faster but perform worse.
* The aggressiveness has no impact on decoding speed, just compression ratio and encode time.
*/
void Filters::Encode(Buffer Input, Buffer Output)
{
	unsigned char *dbuf = (unsigned char*)malloc(FilterBlockSize * sizeof(unsigned char));
	if(dbuf == NULL)
		Error("Failed to allocate filter prediction buffer!");

	Utils *eCalc = new Utils;
	
	double *eScores[2];	
	eScores[0] = (double*)malloc(MaxChannelWidth * sizeof(double));
	eScores[1] = (double*)malloc(MaxChannelWidth * sizeof(double));
	if(eScores[0] == NULL || eScores[1] == NULL)
		Error("Failed to allocate filter prediction buffer!");
	
	// Read all of the input
	Index Outp = 0;
	for(Index i = 0; i < *Input.size;)
	{
		// Split input into chunks
		int len = ((i + FilterBlockSize) < *Input.size) ? FilterBlockSize : (*Input.size - i);
		
		int boostread = len;
		
		// Standard delta
		for(int Ch = 0; Ch <= MaxChannelWidth; Ch++)
		{
			if(Ch > 0)
			{
				Reorder(&Input.block[i], dbuf, Ch, boostread);
				DeltaEncode(dbuf, boostread);
				eScores[0][Ch] = eCalc->CalculateEntropy(dbuf, boostread);
			}
			else // order-0 statistics
			{
				eScores[0][Ch] = eCalc->CalculateEntropy(&Input.block[i], boostread);
			}
		}
		// Linear prediction
		for(int Ch = 1; Ch <= MaxChannelWidth; Ch++)
		{
			Reorder(&Input.block[i], dbuf, Ch, boostread);
			LpcEncode(dbuf, boostread);
			eScores[1][Ch] = eCalc->CalculateEntropy(dbuf, boostread);
		}
		
		// Find the best encode method, write a small header and encoded data.
		int smallest = 0;
		int type = 0;
		double min = eScores[0][smallest];
		eScores[1][0] = 8.0f; // unused state, fill with junk max value
		for(int k = 0; k <= 1; k++)
		{
			for (int j = 1; j <= MaxChannelWidth; j++)
			{
				if (eScores[k][j] < min)
				{
					min = eScores[k][j];
					smallest = j;
					type = k;
				}
			}
		}
		Output.block[Outp++] = (unsigned char)type;
		Output.block[Outp++] = (unsigned char)smallest;

		if(smallest > 0)
		{
			if(type == 0)
			{
				Reorder(&Input.block[i], dbuf, smallest, len);
				DeltaEncode(dbuf, len);
				memcpy(&Output.block[Outp], dbuf, len * sizeof(unsigned char));
			}
			else
			{
				Reorder(&Input.block[i], dbuf, smallest, len);
				LpcEncode(dbuf, len);
				memcpy(&Output.block[Outp], dbuf, len * sizeof(unsigned char));
			}
		}
		else // order-0 statistics
		{
			memcpy(&Output.block[Outp], &Input.block[i], len * sizeof(unsigned char));
		}

		Outp += len;
		i += len;
	}
	delete eCalc;
	free(dbuf);
	free(eScores[0]);
	free(eScores[1]);
	*Output.size = Outp;
}

void Filters::Decode(Buffer Input, Buffer Output)
{
	unsigned char *dbuf = (unsigned char*)malloc(FilterBlockSize * sizeof(unsigned char));
	if(dbuf == NULL)
		Error("Failed to allocate filter prediction buffer!");
	
	Index Outp = 0;
	for(Index i = 0; i < *Input.size;)
	{

		int type = (unsigned char)Input.block[i++];
		int channel = (unsigned char)Input.block[i++];

		int len = ((i + FilterBlockSize) < *Input.size) ? FilterBlockSize : (*Input.size - i);
		if(channel > 0)
		{
			if(type == 0)
			{
				memcpy(dbuf, &Input.block[i], len * sizeof(unsigned char));
				DeltaDecode(dbuf, len);
				Unreorder(dbuf, &Output.block[Outp], channel, len);
			}
			else
			{
				memcpy(dbuf, &Input.block[i], len * sizeof(unsigned char));
				LpcDecode(dbuf, len);
				Unreorder(dbuf, &Output.block[Outp], channel, len);
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