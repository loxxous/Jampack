/*********************************************
* Second stage algorithm, Move-to-front fused with WFC.
* The 8 most recent characters are sorted by their frequency relative to a 
* fast adapting probability (based on fpaqc's model), all other characters use a MTF 
* variant which moves the index 75% of the way to the front relative to the previous index.
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