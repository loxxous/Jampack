/**
* This is just a fast checksum to replace the old CRC32, nothing much here.
* It hashes 4x4 (16) bytes at a time then merges them at the end.
*/
#include "checksum.hpp"

inline unsigned int Checksum::Load32(unsigned char *p)
{
	return (p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]); // Endian safe
}

unsigned int Checksum::IntegrityCheck(Buffer Input)
{
	unsigned char *p = Input.block;
	size_t size = *Input.size;

	unsigned int Prime = 0x9E3779B1;
		
	unsigned int S[4] = {3U}; // Initial seeds
	unsigned int j = 0;
	while((j+16) < size)
	{
		S[0] ^= (Load32(&p[j+0]) + (1 << (S[0] & 7))) * Prime;
		S[1] ^= (Load32(&p[j+4]) + (1 << (S[1] & 7))) * Prime;
		S[2] ^= (Load32(&p[j+8]) + (1 << (S[2] & 7))) * Prime;
		S[3] ^= (Load32(&p[j+12]) +(1 << (S[3] & 7))) * Prime;
		j+=16;
	}
	while(j < size)
	{
		S[0] ^= (p[j] + (1 << (S[0] & 7))) * Prime;
		j++;
	}
		
	return S[0] ^ S[1] ^ S[2] ^ S[3];
}