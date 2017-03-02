/*********************************************
* Cumulative frequency and statistical model
**********************************************/
#ifndef MODEL_H
#define MODEL_H

class Model{
public:
    static const unsigned int ProbBits = 15; 
    static const unsigned int ProbScale = 1 << ProbBits;
private:
	static const unsigned int AlphabetSize = 256;
	unsigned int Freqs[AlphabetSize] = {0};
	unsigned int CumFreqs[AlphabetSize+1] = {0};
	RansEncSymbol esyms[AlphabetSize];
	RansDecSymbol dsyms[AlphabetSize];
	unsigned char cum2sym[ProbScale];
	void StretchAndFit();
public:
	// Static functions for block-wise compression
	int WriteHeader(byte* outbuf, int* olen, int* clen);
	int ReadHeader(byte* inbuf, int* olen, int* clen, int StackSize);
	void Build(byte *buf, int len);
	void Clear();
	// Standard calls
	void SetEncodeTable();
	void SetDecodeTable();
	RansEncSymbol EncGetRange(int c);
	RansDecSymbol* DecGetNext(int c);
	int DecGetSym(int range);
};


RansEncSymbol Model::EncGetRange(int c){
	return esyms[c];
}

RansDecSymbol* Model::DecGetNext(int c){
	return &dsyms[c];
}

int Model::DecGetSym(int range){
	return cum2sym[range];
}

void Model::SetEncodeTable(){
	for(int i = 0; i < AlphabetSize; i++)
		RansEncSymbolInit(&esyms[i], CumFreqs[i], Freqs[i], ProbBits);
}

void Model::SetDecodeTable(){
	for(int i = 0; i < AlphabetSize; i++)
		RansDecSymbolInit(&dsyms[i], CumFreqs[i], Freqs[i]);
	
	for (int s = 0; s < AlphabetSize; s++){
		for (uint32_t i = CumFreqs[s]; i < CumFreqs[s + 1]; i++) 
			cum2sym[i] = s; 
	}
}

/*
	Write out a header containing the length of the compressed block and order-0 stats.
*/
int Model::WriteHeader(byte* outbuf, int* olen, int* clen){
	int HSize = 0;
	for(int i = 0; i < AlphabetSize; i++){
		outbuf[HSize] = Freqs[i] >> 8;
		outbuf[HSize+1] = Freqs[i] & 0xFF;
		HSize += sizeof(uint16_t);
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
	
	return HSize;
}

/*
	Load tables from buffer into Freqs and CumFreqs, make sure they are valid.
	Return new position in buffer.
*/
int Model::ReadHeader(byte* inbuf, int* olen, int* clen, int StackSize){
	int HSize = 0;
	for(int i = 0; i < AlphabetSize; i++){
		Freqs[i] |= inbuf[HSize] << 8;
		Freqs[i] |= inbuf[HSize+1];
		HSize += sizeof(uint16_t);
	}
	
	int sum = 0;
	for(int i = 0; i < AlphabetSize; i++)
		sum += Freqs[i];
	
	if(sum != ProbScale) Error("Misaligned or corrupt header!");

	for (int i=0; i <= AlphabetSize; i++) 
		CumFreqs[i] = 0;
	for (int i=0; i < AlphabetSize; i++) 
		CumFreqs[i+1] = CumFreqs[i] + Freqs[i];
	
	*olen = inbuf[HSize++] << 24;
	*olen |= inbuf[HSize++] << 16;
	*olen |= inbuf[HSize++] << 8;
	*olen |= inbuf[HSize++] << 0;

	if(!(*olen >= 0) && !(*olen <= StackSize)) Error("Misaligned or corrupt header!");

	*clen = inbuf[HSize++] << 24;
	*clen |= inbuf[HSize++] << 16;
	*clen |= inbuf[HSize++] << 8;
	*clen |= inbuf[HSize++] << 0;

	return HSize;
}

/*
	Build and Fit statistics for the block
*/
void Model::Build(byte *buf, int len){
	for(int i = 0; i < len; i++){
		Freqs[buf[i]]++;
	}
	StretchAndFit();
}

void Model::StretchAndFit(){
	int sum = 0;
	for (int i = 0; i < AlphabetSize; i++) 
		sum += Freqs[i];
		
	while(sum > ProbScale){
		sum = 0;
		for (int i = 0; i < AlphabetSize; i++) {
			Freqs[i] = (1 + Freqs[i]) >> 1;
			sum += Freqs[i];
		}
	}
	
	int total = 0;
	int max = 0;
	int bsym = 0;
	CumFreqs[0] = 0;
	for (int i=0; i < AlphabetSize; i++){
		total += Freqs[i] = round((ProbScale * Freqs[i]) / sum);
		if(max < Freqs[i]) { max = Freqs[i]; bsym = i; }
	}
	int remainder = ProbScale - total;
	if(remainder > 0){
		Freqs[bsym]+=remainder;
		total += remainder;
	}
	assert(total == ProbScale);
	for (int i=0; i <= AlphabetSize; i++) { CumFreqs[i] = 0; }
	for (int i=0; i < AlphabetSize; i++) CumFreqs[i+1] = CumFreqs[i] + Freqs[i];
}

/*
	Cleans up the statistics for the next block
*/
void Model::Clear(){
	for(int i = 0; i < AlphabetSize; i++)
		Freqs[i] = 0;	
	for(int i = 0; i < AlphabetSize + 1; i++)
		CumFreqs[i] = 0;
}

#endif // MODEL_H 