/*********************************************
* Run length encoding of zeroes
**********************************************/
#ifndef RLE_H
#define RLE_H

#include "format.hpp"

class RLE
{
public:
	void encode(unsigned char *in, unsigned short *out, int *len);
	void decode(unsigned short *in, unsigned char *out, int *len, int real_len);
private:
	inline int getMSB(unsigned int v);
};
#endif // RLE_H