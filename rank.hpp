/*********************************************
* Second stage algorithm, Move-to-front
**********************************************/
#ifndef postcoder_H
#define postcoder_H

class Postcoder 
{
public:
	void encode(unsigned char *block, int len);
	void decode(unsigned char *block, int len);
};
#endif // postcoder_H
