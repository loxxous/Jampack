/*********************************************
* Cumulative Frequency and Statistical Model
**********************************************/
#ifndef MODEL_H
#define MODEL_H

class Model
{
public:
    static const unsigned int ProbBits = 15; // Max accuracy, 16 can cause overflow in 16 bit type due to 1 << 16
    static const unsigned int ProbScale = 1 << ProbBits;
	
private:
	static const unsigned int Order_1_States = 1;
	static const unsigned int Order_0_State = Order_1_States - 1;
	static const unsigned int AlphabetSize = 257;
	
	unsigned int Freqs[Order_1_States][AlphabetSize] = {{0}};
	unsigned int PreInheritedFreqs[Order_1_States][AlphabetSize] = {{0}};
	unsigned int CumFreqs[Order_1_States][AlphabetSize + 1] = {{0}};
	unsigned int Chain_1 = 0;
	
	unsigned short cum2sym[Order_1_States][ProbScale];
	RansEncSymbol esyms[Order_1_States][AlphabetSize];
	RansDecSymbol dsyms[Order_1_States][AlphabetSize];
	void StretchAndFitFreqs();
	void InheritStatistics();

public:
	int WriteHeader(unsigned char* outbuf, int* olen, int* clen, int* rlen);
	int ReadHeader(unsigned char* inbuf, int* olen, int* clen, int* rlen, int StackSize);
	void Build(unsigned short *buf, int len);
	void Clear();
	void SetEncodeTable();
	void SetDecodeTable();
	RansEncSymbol EncGetRange(unsigned short c);
	RansDecSymbol* DecGetNext(unsigned short c);
	int DecGetSym(int range);
};

RansEncSymbol Model::EncGetRange(unsigned short c)
{
	RansEncSymbol R = esyms[Chain_1][c];
	Chain_1 = (c < (Order_1_States - 1)) ? c : Order_1_States - 1;
	return R;
}

RansDecSymbol* Model::DecGetNext(unsigned short c)
{
	RansDecSymbol* R = &dsyms[Chain_1][c];
	Chain_1 = (c < (Order_1_States - 1)) ? c : Order_1_States - 1;
	return R;
}

int Model::DecGetSym(int range)
{
	return cum2sym[Chain_1][range];
}

void Model::SetEncodeTable()
{
	Chain_1 = 0;
	for(int j = 0; j < Order_1_States; j++)
	{
		for(int i = 0; i < AlphabetSize; i++)
		{
			RansEncSymbolInit(&esyms[j][i], CumFreqs[j][i], Freqs[j][i], ProbBits);			
		}
	}
}

void Model::SetDecodeTable()
{
	Chain_1 = 0;
	for(int j = 0; j < Order_1_States; j++)
	{
		for(int i = 0; i < AlphabetSize; i++)
			RansDecSymbolInit(&dsyms[j][i], CumFreqs[j][i], Freqs[j][i]);
	
		for (int s = 0; s < AlphabetSize; s++)
		{
			for (uint32_t i = CumFreqs[j][s]; i < CumFreqs[j][s + 1]; i++) 
				cum2sym[j][i] = s; 
		}
	}
}

/*
	Write out a header containing the length of the compressed block, order-0, and order-1 stats.
*/
int Model::WriteHeader(unsigned char* outbuf, int* olen, int* clen, int* rlen)
{
	// Write out order-0 table
	int HSize = 0;
	for(int i = 0; i < AlphabetSize; i++)
	{
		outbuf[HSize] = PreInheritedFreqs[Order_0_State][i] >> 8;
		outbuf[HSize+1] = PreInheritedFreqs[Order_0_State][i] & 0xFF;
		HSize += sizeof(uint16_t);
	}
	// Write out order-1 table
	for(int j = 0; j < Order_1_States - 1; j++)
	{
		for(int i = 0; i < Order_1_States; i++)
		{
			outbuf[HSize] = PreInheritedFreqs[j][i] >> 8;
			outbuf[HSize+1] = PreInheritedFreqs[j][i] & 0xFF;
			HSize += sizeof(uint16_t);
		}
	}
	
	// Write out length of the block
	outbuf[HSize++] = (*olen >> 24) & 0xff;
	outbuf[HSize++] = (*olen >> 16) & 0xff;
	outbuf[HSize++] = (*olen >> 8) & 0xff;
	outbuf[HSize++] = (*olen >> 0) & 0xff;
	
	// Write out size of compressed block
	outbuf[HSize++] = (*clen >> 24) & 0xff;
	outbuf[HSize++] = (*clen >> 16) & 0xff;
	outbuf[HSize++] = (*clen >> 8) & 0xff;
	outbuf[HSize++] = (*clen >> 0) & 0xff;
	
	// Write out size of rle buffer
	outbuf[HSize++] = (*rlen >> 24) & 0xff;
	outbuf[HSize++] = (*rlen >> 16) & 0xff;
	outbuf[HSize++] = (*rlen >> 8) & 0xff;
	outbuf[HSize++] = (*rlen >> 0) & 0xff;
	
	return HSize;
}

/*
	Load tables from buffer into Freqs and CumFreqs, make sure they are valid.
	Return new position in buffer.
*/
int Model::ReadHeader(unsigned char* inbuf, int* olen, int* clen, int* rlen, int StackSize)
{
	int HSize = 0;
	for(int i = 0; i < AlphabetSize; i++)
	{
		Freqs[Order_0_State][i] |= inbuf[HSize] << 8;
		Freqs[Order_0_State][i] |= inbuf[HSize+1];
		HSize += sizeof(uint16_t);
	}
	for(int j = 0; j < Order_1_States - 1; j++)
	{
		for(int i = 0; i < Order_1_States; i++)
		{
			Freqs[j][i] |= inbuf[HSize] << 8;
			Freqs[j][i] |= inbuf[HSize+1];
			HSize += sizeof(uint16_t);
		}
	}
	
	InheritStatistics();
	StretchAndFitFreqs();
	
	// Check all states and freqs
	for(int j = 0; j < Order_1_States; j++)
	{
		int sum = 0;
		for(int i = 0; i < AlphabetSize; i++)
			sum += Freqs[j][i];
		
		if(!(sum==ProbScale)) Error("Misaligned or corrupt header!"); 
		for (int i=0; i <= AlphabetSize; i++) 
			CumFreqs[j][i] = 0;
		for (int i=0; i < AlphabetSize; i++) 
			CumFreqs[j][i+1] = CumFreqs[j][i] + Freqs[j][i];
	}
	*olen = inbuf[HSize++] << 24;
	*olen |= inbuf[HSize++] << 16;
	*olen |= inbuf[HSize++] << 8;
	*olen |= inbuf[HSize++] << 0;

	if(!(*olen >= 0 && *olen <= StackSize)) Error("Misaligned or corrupt header!"); 

	*clen = inbuf[HSize++] << 24;
	*clen |= inbuf[HSize++] << 16;
	*clen |= inbuf[HSize++] << 8;
	*clen |= inbuf[HSize++] << 0;	
	
	*rlen = inbuf[HSize++] << 24;
	*rlen |= inbuf[HSize++] << 16;
	*rlen |= inbuf[HSize++] << 8;
	*rlen |= inbuf[HSize++] << 0;

	return HSize;
}

/*
	Build statistics for the block
*/
void Model::Build(unsigned short *buf, int len)
{
	unsigned int tmpFrq[AlphabetSize] = {0};
	unsigned short c1 = 0, c = 0;
	for(int i = 0; i < len; i++)
	{
		c = buf[i];
		Freqs[c1][c]++;
		tmpFrq[c]++;
		if(!(c < AlphabetSize)) Error("Read an invalid symbol!");
		c1 = (c < Order_1_States - 1) ? c : Order_1_States - 1;
	}
	
	// Inform lower order about high orders that will inherit 
	for(int k = Order_1_States; k < AlphabetSize; k++)
		if(Freqs[Order_0_State][k] == 0 && tmpFrq[k] > 0)
			Freqs[Order_0_State][k] = 1;
		
	// Remove high order redundancy (The header stores only a few high order contexts)
	for(int j = 0; j < Order_1_States - 1; j++)
		for(int k = Order_1_States; k < AlphabetSize; k++)
			Freqs[j][k] = 0;
	
	StretchAndFitFreqs(); // Quantize and scaled to fit 2^15, this gets stored in the header
	memcpy(PreInheritedFreqs, Freqs, AlphabetSize * Order_1_States * sizeof(unsigned int)); 
	InheritStatistics(); // Build statistics from the header data
	StretchAndFitFreqs(); 
}

/*
	Rebuild high orders from lowest order data assuming a zipf-like distribution
*/
void Model::InheritStatistics()
{
	for(int j = 0; j < Order_1_States - 1; j++)
		for(int k = Order_1_States; k < AlphabetSize; k++)
			if(Freqs[Order_0_State][k] > 0)
				Freqs[j][k] = (Freqs[Order_0_State][k] / (Order_1_States - j)) + 1;
}

void Model::StretchAndFitFreqs()
{
	for(int j = 0; j < Order_1_States; j++)
	{
		int sum = 0;
		for (int i = 0; i < AlphabetSize; i++) 
			sum += Freqs[j][i];
			
		while(sum > ProbScale)
		{
			sum = 0;
			for (int i = 0; i < AlphabetSize; i++) 
			{
				Freqs[j][i] = (1 + Freqs[j][i]) >> 1;
				sum += Freqs[j][i];
			}
		}
		
		int total = 0;
		int max = 0;
		int bsym = 0;
		CumFreqs[j][0] = 0;
		for (int i=0; i < AlphabetSize; i++)
		{
			if(Freqs[j][i] > 0)
			{
				total += Freqs[j][i] = ProbScale * Freqs[j][i] / sum;
				if(max < Freqs[j][i]) { max = Freqs[j][i]; bsym = i; }
			}
		}
		int remainder = ProbScale - total;
		if(remainder > 0)
		{
			Freqs[j][bsym]+=remainder;
			total += remainder;
		}
		assert(total == ProbScale);
		for (int i=0; i <= AlphabetSize; i++) CumFreqs[j][i] = 0;
		for (int i=0; i < AlphabetSize; i++) CumFreqs[j][i+1] = CumFreqs[j][i] + Freqs[j][i];
	}
}
/*
	Cleans up the statistics for the next block
*/
void Model::Clear()
{
	Chain_1 = 0;
	memset(Freqs, 0, AlphabetSize * Order_1_States * sizeof(unsigned int)); 
}

#endif // MODEL_H
