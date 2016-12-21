/*********************************************
*	DELTA COMPRESSION AND DECOMPRESSION CODE
**********************************************/

#ifndef DELTA_H
#define DELTA_H

class delta 
{
public:
	int encode(byte *inbuf, int *size, byte *outbuf, int *out_size);
	void decode(byte *inbuf, int *size, byte *outbuf, int *out_size, int method);
};

class entropy
{
protected:
	long freq[33][256];
	double len[33];

	double CalcEntropy(int channel)
	{
		double entropy = 0;
		for (int i = 0; i < 256; i++)
		{
			double charProbability = freq[channel][i] / len[channel];
			if (charProbability != 0)
			{
				entropy += (charProbability)*(-log(charProbability) / log(2));
			}
		}
		return entropy;
	}

public:
	void count(int channel, byte *buf, int size)
	{
		for (int i = 0; i < size; i++)
		{
			freq[channel][buf[i]]++;
		}
		len[channel] += size;
	}

	void ResetCounts()
	{
		for (int i = 0; i < 33; i++)
		{
			for (int k = 0; k < 256; k++) freq[i][k] = 0;
			len[i] = 0;
		}
	}

	int FindSmallestChannel()
	{
		double chanEnt[33];
		for (int i = 0; i < 33; i++)
		{
			chanEnt[i] = CalcEntropy(i);
		}
		int index = 0;
		double min = chanEnt[index];
		for (int i = 1; i < 33; i++)
		{
			if (chanEnt[i] < min)
			{
				min = chanEnt[i];
				index = i;
			}
		}
		return index;
	}
};

	/*
	*	Constants used for mapping channels to encode methods
	*/
	static const byte channelmap[] =
	{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
		1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, };

	void updateWeight(byte err, int *weight)
	{
		if (err < 127) 
		{
			if (*weight != 1281 ) *weight++;
		}
		else if (err > 127) 
		{
			if (*weight != -1281) *weight--;
		}
	}

	void lpc_encode(byte *inbuf, int *size, byte *outbuf, int *out_size, int channel)
	{
		int pos = 0;
		for (int i = 0; i < channel; i++)
		{
			int rate = 6;
			int weight = 0;
			short prev = 0;
			for (int j = i; j < *size; j += channel)
			{
				byte p = ((prev & 0xff) - (prev >> 8)) + (prev & 0xff);
				int w = weight >> rate;
				byte error = w + (p - inbuf[j]);

				updateWeight(error, &weight);
				prev = (prev << 8) | (w + inbuf[j]);

				outbuf[pos++] = error;
			}
		}
		*out_size = pos;
	}

	void lpc_decode(byte *inbuf, int *size, byte *outbuf, int *out_size, int channel)
	{
		int pos = 0;
		for (int i = 0; i < channel; i++)
		{
			int rate = 6;
			int weight = 0;
			short prev = 0;
			for (int j = i; j < *size; j += channel)
			{
				byte p = ((prev & 0xff) - (prev >> 8)) + (prev & 0xff);
				int w = weight >> rate;
				byte b = w + (p - inbuf[pos++]);

				updateWeight(inbuf[j], &weight);
				prev = (prev << 8) | (w + b);

				outbuf[j] = b;
			}
		}
		*out_size = pos;
	}

	void delta_encode(byte *inbuf, int *size, byte *outbuf, int *out_size, int channel)
	{
		int pos = 0;
		for (int i = 0; i < channel; i++)
		{
			byte prev = 0;
			for (int j = i; j < *size; j += channel)
			{
				outbuf[pos++] = prev - inbuf[j];
				prev = inbuf[j];
			}
		}
		*out_size = pos;
	}

	void delta_decode(byte *inbuf, int *size, byte *outbuf, int *out_size, int channel)
	{
		int pos = 0;
		for (int i = 0; i < channel; i++)
		{
			byte prev = 0;
			for (int j = i; j < *size; j += channel)
			{
				outbuf[j] = prev + inbuf[pos++];
				prev = outbuf[j];
			}
		}
		*out_size = pos;
	}

	/* skim over the block and determine best encode method, return encode method and encoded block */
	int delta::encode(byte *inbuf, int *size, byte *outbuf, int *out_size)
	{
		// try all transformations on a small portion of the block and store the entropy of each transformation
		entropy e;
		for(int ch = 0; ch < 33; ch++)
		{ 
			int block = 1 << 10; // read 1 KB 
			int step = 1 << 14;	 // every 16 KB
			for(int k = 0; (k+step) < *size; k+=step)
			{
				int channel = channelmap[ch];
				lpc_encode(inbuf, &block, outbuf, out_size, channel);
				e.count(ch, inbuf, block);
			}
		}

		int method = e.FindSmallestChannel();
		e.ResetCounts();
		int channels = channelmap[method];

		// write embedded header of method used

		if (method == 0) // no filter
		{
			memmove(&outbuf, inbuf, *size); // move the old block to the new block
		}
		else if (method <= 16) // standard delta 1-16 channels
		{
			delta_encode(inbuf, size, outbuf, out_size, channels);
		}
		else if (method > 16) // adaptive lpc 1-16 channels
		{
			lpc_encode(inbuf, size, outbuf, out_size, channels);
		}

		return method;
	}

	void delta::decode(byte *inbuf, int *size, byte *outbuf, int *out_size, int method)
	{


	}

#endif // DELTA_H