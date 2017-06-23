/*********************************************
* Second stage algorithm : Sorted rank coding
**********************************************/
#ifndef postcoder_H
#define postcoder_H

#include "format.hpp"

class Postcoder 
{
public:
	void Encode(unsigned char* T, int* Freq, int len);
	void Decode(unsigned char* RankArray, int* Freq, int len);
private:
	void GenerateSortedMap(int* Freq, unsigned char* SortedMap);
};
#endif // postcoder_H
