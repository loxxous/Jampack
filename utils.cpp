#include "utils.hpp"

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
	short i = 0;
	int d = 0;
	Index val = 0;
	while((buf[i + d] & 0x80) == 0)
	{
		if(d > 4) 
			Error("LEB decoder tried to load a value bigger than the type supports!");
		val = (val << 7) | buf[i + d];
		d++;
	}
	val = (val << 7) | buf[i + d] & 0x7F;
	if(d > 0) 
		val += constants[d - 1];
	i += d + 1;
	*valptr = val;
	return i;
}

/**
* Write compressed integer in variable length prefix form
*/
Index Utils::EncodeVaInt(Index val, unsigned char *buf)
{
	if(val < 253)
	{
		buf[0] = val;
		return 1;
	}
	else
	{
		if(val < 0xffff - 1)
		{
			buf[0] = 253, buf[1] = (val >> 8) & 0xff, buf[2] = val & 0xff;
			return 3;
		}
		else if(val < 0xffffff - 1)
		{
			buf[0] = 254, buf[1] = (val >> 16) & 0xff, buf[2] = (val >> 8)& 0xff, buf[3] = val & 0xff;
			return 4;
		}
		else
		{
			buf[0] = 255, buf[1] = (val >> 24) & 0xff, buf[2] = (val >> 16)& 0xff, buf[3] = (val >> 8)& 0xff, buf[4] = val & 0xff;
			return 5;
		}
	}
}
/**
* Read compressed integer in variable length prefix form (much faster than leb, has a better best case, worse worst case, has prefix properties)
*/
Index Utils::DecodeVaInt(Index *valptr, unsigned char *buf)
{
	Index val = buf[0];
	switch(val)
	{
		case 253 : *valptr = (buf[1] << 8) | buf[2]; return 3;
		case 254 : *valptr = (buf[1] << 16) | (buf[2] << 8) | buf[3]; return 4;
		case 255 : *valptr = (buf[1] << 24) | (buf[2] << 16) | (buf[3] << 8) || buf[4]; return 5;
		default  : *valptr = val; return 1;
	}
}

double Utils::CalculateEntropy(unsigned char *ptr, Index len)
{
	double freqs[256] = {0};
	
	for(int i = 0; i < len; i++)
		freqs[ptr[i]]++;
	
	double entropy = 0;
	for (int i = 0; i < 256; i++)
	{
		double prob = freqs[i] / len;
		if (prob > 0)
		{
			entropy += (prob)*(-log(prob) / log(2));
		}
	}
	return entropy;
}

double Utils::CalculateO1Entropy(unsigned char *ptr, Index len)
{
	Index F[256][256] = {0}, T[256] = {0};
    double e = 0;
    int i, j;
    
    for (j = i = 0; i < len; i++) {
        F[j][ptr[i]]++;
        T[j]++;
        j = ptr[i];
    }

    for (j = 0; j < 256; j++) {
        for (i = 0; i < 256; i++) {
            if (F[j][i]) {
                e += -log((double)F[j][i]/T[j]) * F[j][i];
            }
        }
    }

    return e / log(256);
}
