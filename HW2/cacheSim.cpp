#include "cacheSim.hpp"

// CacheLine definitions
CacheLine::CacheLine(unsigned int initialLRU) : tag(0), valid(false),
												dirty(false), LRU(initialLRU) {	
}

CacheLine::~CacheLine() {
}

unsigned int CacheLine::getTag() {
	return this->tag;
}

bool CacheLine::getValid() {
	return this->valid;
}

bool CacheLine::getDirty() {
	return this->dirty;
}

unsigned int CacheLine::getLRU() {
	return this->LRU;
}

void CacheLine::setTag(unsigned int tag) {
	this->tag = tag;
}

void CacheLine::setValid(bool valid) {
	this->valid = valid;
}

void CacheLine::setDirty(bool dirty) {
	this->dirty = dirty;
}

void CacheLine::setLRU(unsigned int LRU) {
	this->LRU = LRU;
}

bool CacheLine::compareTag(unsigned int outsideTag) {
	return this->tag == outsideTag;
}

// CacheSet definitions
CacheSet::CacheSet() : numWays(0), lineCount(0), lines(nullptr) {
}

CacheSet::CacheSet(unsigned int numOfWays) : numWays(numWays), lineCount(0) {
	this->lines = new CacheLine[numOfWays];
}

CacheSet::~CacheSet() {
	if (this->lines != nullptr) {	
		delete[] this->lines;
	}
}

void CacheSet::initSet(unsigned int numOfWays) {
	this->numWays = numOfWays;
	this->lines = new CacheLine[numOfWays];
}

void CacheSet::updateLRU(unsigned int way) {
	int LRU = this->lines[way].getLRU();
	for (unsigned int i = 0; i < this->numWays; i++) {
		unsigned int currentLRU = this->lines[i].getLRU();
		if ((i != way) && (currentLRU > LRU) && (this->lines[i].getValid())) {
			this->lines[i].setLRU(currentLRU - 1);
		}
	}
	this->lines[way].setLRU(this->lineCount - 1);
}

bool CacheSet::insertLine(unsigned int tag) {
	if (this->lineCount == this->numWays) {
		return false;
	}
	for (unsigned int i = 0; i < this->numWays; i++) {
		if (!this->lines[i].getValid()) {
			this->lines[i].setTag(tag);
			this->lines[i].setValid(true);
			this->lines[i].setDirty(false);
			this->lines[i].setLRU(++this->lineCount); // need to change
			return true;
		}
	}
}

CacheLine* CacheSet::findLine(unsigned int tag) {
	for (unsigned int i = 0; i < this->numWays; i++) {
		if (this->lines[i].compareTag(tag) && this->lines[i].getValid()) {
			this->updateLRU(i);
			return &this->lines[i];
		}
	}
	return nullptr;
}

CacheLine* CacheSet::removeLine(unsigned int way) {
	CacheLine* line = new CacheLine();
	*line = this->lines[way];
	this->updateLRU(way);
	this->lines[way].setValid(false);
	this->lines[way].setDirty(false);
	this->lines[way].setTag(0);
	this->lines[way].setLRU(0);
	this->lineCount--;
	return line;
}

bool CacheSet::writeToLine(unsigned int tag) {
	CacheLine* line = this->findLine(tag);
	if (line != nullptr) {
		line->setDirty(true);
		return true;
	}
	return false;
}

bool CacheSet::readFromLine(unsigned int tag) {
	CacheLine* line = this->findLine(tag);
	if (line != nullptr) {
		return true;
	}
	return false;
}

// Cache
Cache::Cache(unsigned int MemCyc, unsigned int BSize, unsigned int L1Size, unsigned int L2Size,
            unsigned int L1Assoc, unsigned int L2Assoc, unsigned int L1Cyc, unsigned int L2Cyc,
            unsigned int WrAlloc) : MemCyc(MemCyc), BSize(BSize), L1Size(L1Size), L2Size(L2Size),
			L1Assoc(L1Assoc), L2Assoc(L2Assoc), L1Cyc(L1Cyc), L2Cyc(L2Cyc), WrAlloc(WrAlloc), 
			L1Reads(0), L1ReadMisses(0), L1Writes(0), L1WriteMisses(0), L1WriteBacks(0),
			L2Reads(0), L2ReadMisses(0), L2Writes(0), L2WriteMisses(0), L2WriteBacks(0),
			totalL1Cycles(0), totalL2Cycles(0), totalMemCycles(0) {	
	
    this->BlockSize = 1 << this->BSize; 
	this->L1OffsetBits = this->L2OffsetBits = this->BSize;
	this->L1IndexBits = this->L1Size - this->L1OffsetBits - this->L1Assoc;
	this->L2IndexBits = this->L2Size - this->L2OffsetBits - this->L2Assoc;
	this->L1TagBits = 32 - this->L1IndexBits - this->L1OffsetBits;
	this->L2TagBits = 32 - this->L2IndexBits - this->L2OffsetBits;

	this->L1NumBlocks = 1 << (this->L1Size - this->BSize);
	this->L2NumBlocks = 1 << (this->L2Size - this->BSize);
	this->L1NumSets = 1 << this->L1IndexBits;
	this->L2NumSets = 1 << this->L2IndexBits;
	this->L1NumWays = 1 << this->L1Assoc;
	this->L2NumWays = 1 << this->L2Assoc;

	this->L1Sets = new CacheSet[this->L1NumSets];
	for (unsigned int i = 0; i < this->L1NumSets; i++) {
		this->L1Sets[i].initSet(this->L1NumWays);
	}
	
	this->L2Sets = new CacheSet[this->L2NumSets];
	for (unsigned int i = 0; i < this->L2NumSets; i++) {
		this->L2Sets[i].initSet(this->L2NumWays);
	}
}

Cache::~Cache() {
	if (this->L1Sets != nullptr) {
		delete[] this->L1Sets;
	}
	if (this->L2Sets != nullptr) {
		delete[] this->L2Sets;
	}
}

int main(int argc, char **argv) {

	if (argc < 19) { // might need to be 20
		cerr << "Not enough arguments" << endl;
		return 0;
	}

	// Get input arguments

	// File
	// Assuming it is the first argument
	char* fileString = argv[1];
	ifstream file(fileString); //input file stream
	string line;
	if (!file || !file.good()) {
		// File doesn't exist or some other error
		cerr << "File not found" << endl;
		return 0;
	}

	unsigned int MemCyc = 0, BSize = 0, L1Size = 0, L2Size = 0, L1Assoc = 0,
			L2Assoc = 0, L1Cyc = 0, L2Cyc = 0, WrAlloc = 0;

	for (int i = 2; i < 19; i += 2) {
		string s(argv[i]);
		if (s == "--mem-cyc") {
			MemCyc = atoi(argv[i + 1]);
		} else if (s == "--bsize") {
			BSize = atoi(argv[i + 1]);
		} else if (s == "--l1-size") {
			L1Size = atoi(argv[i + 1]);
		} else if (s == "--l2-size") {
			L2Size = atoi(argv[i + 1]);
		} else if (s == "--l1-cyc") {
			L1Cyc = atoi(argv[i + 1]);
		} else if (s == "--l2-cyc") {
			L2Cyc = atoi(argv[i + 1]);
		} else if (s == "--l1-assoc") {
			L1Assoc = atoi(argv[i + 1]);
		} else if (s == "--l2-assoc") {
			L2Assoc = atoi(argv[i + 1]);
		} else if (s == "--wr-alloc") {
			WrAlloc = atoi(argv[i + 1]);
		} else {
			cerr << "Error in arguments" << endl;
			return 0;
		}
	}

	// create cache
	Cache cache(MemCyc, BSize, L1Size, L2Size, L1Assoc, L2Assoc, L1Cyc, L2Cyc, WrAlloc);

	while (getline(file, line)) {

		stringstream ss(line);
		string address;
		char operation = 0; // read (R) or write (W)
		if (!(ss >> operation >> address)) {
			// Operation appears in an Invalid format
			cout << "Command Format error" << endl;
			return 0;
		}

		string cutAddress = address.substr(2); // Removing the "0x" part of the address

		unsigned long int num = 0;
		num = strtoul(cutAddress.c_str(), NULL, 16);

		if (operation == 'R') {
			// Read
			// DEBUG - remove this line
			cout << " (hex) " << hex << num;
		} else if (operation == 'W') {
			// Write
			// DEBUG - remove this line
			cout << " (hex) " << hex << num;
		} else {
			// Operation appears in an Invalid format
			cout << "Operation Format error" << endl;
			return 0;
		}
	}

	double L1MissRate;
	double L2MissRate;
	double avgAccTime;

	printf("L1miss=%.03f ", L1MissRate);
	printf("L2miss=%.03f ", L2MissRate);
	printf("AccTimeAvg=%.03f\n", avgAccTime);

	return 0;
}
