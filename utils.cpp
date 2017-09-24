#include "utils.hpp"

Index Utils::SizeOfValue(Index val)
{
	if(val < 0)
		Error("Cannot get size of a negative number!");
	
	short i = 0;
	if (val < constants[0])
		i = 1;
	else if (val < constants[1])
		i = 2;
	else if (val < constants[2])
		i = 3;
	else if (val < constants[3])
		i = 4;
	else
		i = 5;
	return i;
}

Index Utils::EncodeLeb128(Index val, unsigned char *buf)
{
	if(val < 0)
		Error("Cannot encode a negative number!");
	
	short i = 0;
	if (val < constants[0])
	{
		buf[i] = (val | 0x80);
		i += 1;
	}
	else if (val < constants[1])
	{
		val -= constants[0];
		buf[i + 0] = (val >> 7) & 0x7F;
		buf[i + 1] = (val & 0x7F) | 0x80;
		i += 2;
	}
	else if (val < constants[2])
	{
		val -= constants[1];
		buf[i + 0] = (val >> 14) & 0x7F;
		buf[i + 1] = (val >> 7) & 0x7F;
		buf[i + 2] = (val & 0x7F) | 0x80;
		i += 3;
	}
	else if (val < constants[3])
	{
		val -= constants[2];
		buf[i + 0] = (val >> 21) & 0x7F;
		buf[i + 1] = (val >> 14) & 0x7F;
		buf[i + 2] = (val >> 7) & 0x7F;
		buf[i + 3] = ((val) & 0x7F) | 0x80;
		i += 4;
	}
	else
	{
		val -= constants[3];
		buf[i + 0] = (val >> 28) & 0x7F;
		buf[i + 1] = (val >> 21) & 0x7F;
		buf[i + 2] = (val >> 14) & 0x7F;
		buf[i + 3] = (val >> 7) & 0x7F;
		buf[i + 4] = ((val) & 0x7F) | 0x80;
		i += 5;
	}
	return i;
}

/**
* Read compressed integer in LEB128 with carry. Returns new position in the buffer and value.
*/
Index Utils::DecodeLeb128(Index *valptr, unsigned char *buf)
{
	int d = 0;
	Index val = 0;
	while((buf[d] & 0x80) == 0)
	{
		#ifndef NDEBUG
		if(d > 4) 
			Error("LEB decoder tried to load a value bigger than the type supports!");
		#endif
		val = (val << 7) | buf[d];
		d++;
	}
	val = (val << 7) | (buf[d] & 0x7F);
	val += ~(d - 1) & constants[d - 1];
	*valptr = val;
	return d + 1;
}

Utils::Utils()
{
	Rank = new Postcoder;
	for(int i = 1; i < EntScale; i++)
	{
		double p = (double)i / EntScale;
		EntLog[i] = (-log(p) / log(2));
	}
	EntLog[0] = 0; // Infinite case, do nothing.
}

Utils::~Utils()
{
	delete Rank;
}

/**
* Compute order0 and order-1 entropy mixed together, this is fast and effective enough for an initial idea of what configuration should be used.
*/
double Utils::CalculateMixedEntropy(unsigned char *ptr, Index len)
{
	double order0 = CalculateEntropy(ptr, len);
	double order1 = CalculateO1Entropy(ptr, len);
	return ((order0 + order1) / 2);
}

/**
* Generate a generalized variant of BWT and rank scheme, return order-1 entropy of it.
* This is used to make a damn close guess in the proper direction for the filter configurations.
*/
double Utils::CalculateSortedEntropy(unsigned char *ptr, Index len)
{
	unsigned char *sbuf = (unsigned char*)malloc(len * sizeof(unsigned char));
	if(sbuf == NULL)
		Error("Failed to allocate sorted entropy buffer!");
	
	Index bucket[257] = {0};
	for(Index i = 0; i < len; i++)
		bucket[ptr[i] + 1]++;
	
	for(Index i = 1; i < 256; ++i)
		bucket[i] += bucket[i - 1];
	
	for(Index i = 0; i < len; i++)
		sbuf[bucket[ptr[i]]++] = ptr[(i - 1) % len]; // Induce a fast BWT-like sort on the block
	
	double SortedEntropy = CalculateO1Entropy(sbuf, len);
	free(sbuf);
	return SortedEntropy;
}

/**
* Calculate order-0 entropy using 16-bit probability tables
*/
double Utils::CalculateEntropy(unsigned char *ptr, Index len)
{
	Index freqs[256] = {0};
	
	for(int i = 0; i < len; i++)
		freqs[ptr[i]]++;
	
	double e[4] = {0};
	int p[4] = {0};
	for (int i = 0; i < 256; i += 4)
	{
		p[0] = ((double)freqs[i+0] / (double)len) * EntScale;
		p[1] = ((double)freqs[i+1] / (double)len) * EntScale;
		p[2] = ((double)freqs[i+2] / (double)len) * EntScale;
		p[3] = ((double)freqs[i+3] / (double)len) * EntScale;
		e[0] += EntLog[p[0]] * (double)freqs[i+0];
		e[1] += EntLog[p[1]] * (double)freqs[i+1];
		e[2] += EntLog[p[2]] * (double)freqs[i+2];
		e[3] += EntLog[p[3]] * (double)freqs[i+3];
	}
	return (e[0] + e[1] + e[2] + e[3]) / (double)len;
}

/**
* Calculate order-1 entropy using 16-bit probability tables
*/
double Utils::CalculateO1Entropy(unsigned char *ptr, Index len)
{
	Index freqs[256][256] = {{0}}, total[256] = {0};
    double e[4] = {0};
	int p[4] = {0};
    int j = 0;
    
    for (int i = 0; i < len; i++) 
	{
        freqs[j][ptr[i]]++;
        total[j]++;
        j = ptr[i];
    }

    for (j = 0; j < 256; j++) 
	{
		for (int i = 0; i < 256; i += 4)
		{
			p[0] = (freqs[j][i+0]) ? ((double)freqs[j][i+0] / (double)total[j]) * EntScale : 0;
			p[1] = (freqs[j][i+1]) ? ((double)freqs[j][i+1] / (double)total[j]) * EntScale : 0;
			p[2] = (freqs[j][i+2]) ? ((double)freqs[j][i+2] / (double)total[j]) * EntScale : 0;
			p[3] = (freqs[j][i+3]) ? ((double)freqs[j][i+3] / (double)total[j]) * EntScale : 0;
			e[0] += EntLog[p[0]] * (double)freqs[j][i+0];
			e[1] += EntLog[p[1]] * (double)freqs[j][i+1];
			e[2] += EntLog[p[2]] * (double)freqs[j][i+2];
			e[3] += EntLog[p[3]] * (double)freqs[j][i+3];
		}
    }

    return (e[0] + e[1] + e[2] + e[3]) / (double)len;
}
