/*
	This is just a fast checksum to replace the old CRC32, nothing much here.
	It hashes 4x4 (16) bytes at a time then merges them at the end.
*/

#ifndef CHECKSUM_H
#define CHECKSUM_H

#include "format.hpp"

class Checksum
{	
private:
	inline unsigned int Load32(unsigned char *p);

public:
	unsigned int IntegrityCheck(Buffer Input);
};
#endif // CHECKSUM_H