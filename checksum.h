/*
	This is just a fast checksum to replace the old CRC32, nothing much here.
	It hashes 4x4 (16) bytes at a time then merges them at the end.
*/

#ifndef CHECKSUM_H
#define CHECKSUM_H

class Checksum
{	
private:
	inline unsigned int Load32(unsigned char *p)
	{
		return (p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]); // Endian safe
	}

public:
	unsigned int IntegrityCheck(Buffer Input)
	{
		unsigned char *p = Input.block;
		size_t size = *Input.size;

		unsigned int Prime = 0x9E3779B1;
		unsigned int State = 0U;
		
		unsigned int S[4] = {0U};
		unsigned int j = 0;
		while((j+16) < size)
		{
			S[0] ^= (Load32(&p[j+0]) + 1) * Prime;
			S[1] ^= (Load32(&p[j+4]) + 1) * Prime;
			S[2] ^= (Load32(&p[j+8]) + 1) * Prime;
			S[3] ^= (Load32(&p[j+12]) + 1) * Prime;
			j+=16;
		}
		while(j < size)
		{
			S[0] ^= (p[j] + 1) * Prime;
			j++;
		}
		State = S[0] ^ S[1] ^ S[2] ^ S[3];
		
		return State;
	}
};
#endif // CHECKSUM_H
