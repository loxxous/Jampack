/*********************************************
* Localized prefix model
**********************************************/

#ifndef PREFIX_H
#define PREFIX_H

#include "format.hpp"
#include "utils.hpp"

class Lpx
{	
private:
	const int HitThreshold = 32; // Consecutive exact matches for a leading prefix
	const int MaxRecordSize = 16 << 10; // Window for prefix to suffix correllations
	
	struct PrefixRecord
	{
		unsigned int cxt;
		unsigned int pos;
		unsigned int hits;
	};
	void UpdateTable(PrefixRecord *table, unsigned int *cxt, unsigned int pos);
public:
	void Encode(Buffer Input, Buffer Output);
	void Decode(Buffer Input, Buffer Output);
};
#endif // PREFIX_H