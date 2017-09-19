/*********************************************
* Cyclic hashed history model
* This is a very memory efficient way of tracking the probability history of arbitrarily large values in constant space.
* This class is used to model offset redundancy and perform anti-context parsing quickly in the lz77 stage.
**********************************************/

#ifndef CYCLICHHM_H
#define CYCLICHHM_H

#include "format.hpp"

class CyclicHashHistory
{
	private:
	unsigned short *CircularBuffer;
	unsigned int SizeOfCB = 0;
	unsigned int PosInCB = 0;
	
	unsigned int *History;
	unsigned int HashBits = 16;
	unsigned int HashSize = 1 << HashBits;
	inline unsigned int Hash(Index value);
	
	unsigned int *ModDensity; 			// Helps us figure out the standard structure size of a file
	unsigned int ModSize = 1 << 16; 
	unsigned int PreviousValue = 0; 	// This helps us find the lowest common multiple in O(1) time
	unsigned int AverageDensity = 0; 	// Average density across all valid entries
	unsigned int UniqueDensities = 0; 	// How many entries in ModDensity were filled with a non-zero value
	unsigned int StructureWidth = 1; 	// The width of the detected structure
	
	public:
	CyclicHashHistory(int size);
	~CyclicHashHistory();
	
	void Update(Index value);
	void BuildModel();
	void CleanModel();
	unsigned int GetHistory(Index value);
	bool FindPeaks(Index value);
	void Assert();
};

#endif // CYCLICHHM_H 