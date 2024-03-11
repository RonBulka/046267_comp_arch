#include "cacheSim.hpp"

/**********************************************************************************************/
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

/**********************************************************************************************/
// CacheSet definitions
CacheSet::CacheSet() : numWays(0), lineCount(0), lines(nullptr) {
}

CacheSet::CacheSet(unsigned int numWays) : numWays(numWays), lineCount(0) {
	this->lines = new CacheLine[this->numWays];
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

unsigned int CacheSet::getWay(unsigned int tag) {
	for (unsigned int i = 0; i < this->numWays; i++) {
		if (this->lines[i].compareTag(tag) && this->lines[i].getValid()) {
			return i;
		}
	}
	return (unsigned int)-1;
}

void CacheSet::updateLRU(unsigned int way) {
	// update the LRU of all the lines in the set
	unsigned int LRU = this->lines[way].getLRU();
	for (unsigned int i = 0; i < this->numWays; i++) {
		unsigned int currentLRU = this->lines[i].getLRU();
		if ((i != way) && (currentLRU > LRU) && (this->lines[i].getValid())) {
			this->lines[i].setLRU(currentLRU - 1);
		}
	}
	this->lines[way].setLRU(this->lineCount - 1);
}

bool CacheSet::insertLine(unsigned int tag) {
	// check if the set is full
	if (this->lineCount == this->numWays) {
		return false;
	}
	// find the first invalid line
	for (unsigned int i = 0; i < this->numWays; i++) {
		if (!this->lines[i].getValid()) {
			this->lines[i].setTag(tag);
			this->lines[i].setValid(true);
			this->lines[i].setDirty(false);
			this->lines[i].setLRU(this->lineCount++);
			return true;
		}
	}
	return false;
}

bool CacheSet::isLineInSet(unsigned int tag) {
	for (unsigned int i = 0; i < this->numWays; i++) {
		if (this->lines[i].compareTag(tag) && this->lines[i].getValid()) {
			return true;
		}
	}
	return false;
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

CacheLine* CacheSet::removeLine() {
	unsigned int way = 0;
	// find first valid line
	while (!this->lines[way].getValid()) {
		way++;
	}
	// find the least recently used line
	for (unsigned int i = way + 1; i < this->numWays; i++) {
		if ((this->lines[i].getLRU() < this->lines[way].getLRU()) && 
			(this->lines[i].getValid())) {
			way = i;
		}
	}
	// way is the least recently used line (LRU = 0)
	CacheLine* line = new CacheLine();
	// copy the line
	*line = this->lines[way];
	// invalidate the line
	this->updateLRU(way);
	this->lines[way].setValid(false);
	this->lines[way].setDirty(false);
	this->lines[way].setTag(0);
	this->lines[way].setLRU(0);
	this->lineCount--;
	return line;
}

CacheLine* CacheSet::removeLine(unsigned int way) {
	CacheLine* line = new CacheLine();
	// copy the line
	*line = this->lines[way];
	// invalidate the line
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

void CacheSet::updateDirty(unsigned int way, bool dirty) {
	this->lines[way].setDirty(dirty);
}

/**********************************************************************************************/
// Cache definitions
Cache::Cache(unsigned int MemCyc, unsigned int BSize, unsigned int L1Size, unsigned int L2Size,
            unsigned int L1Assoc, unsigned int L2Assoc, unsigned int L1Cyc, unsigned int L2Cyc,
            unsigned int WrAlloc) : MemCyc(MemCyc), BSize(BSize), L1Size(L1Size), L2Size(L2Size),
			L1Assoc(L1Assoc), L2Assoc(L2Assoc), L1Cyc(L1Cyc), L2Cyc(L2Cyc), WrAlloc(WrAlloc), 
			L1Reads(0), L1ReadMisses(0), L1Writes(0), L1WriteMisses(0),
			L2Reads(0), L2ReadMisses(0), L2Writes(0), L2WriteMisses(0),
			totalL1Cycles(0), totalL2Cycles(0), totalMemCycles(0) {	
	// calculate the number of bits for the tag, index and offset
    this->BlockSize = 1 << this->BSize; 
	this->L1OffsetBits = this->BSize;
	this->L2OffsetBits = this->BSize;
	this->L1IndexBits = this->L1Size - this->L1OffsetBits - this->L1Assoc;
	this->L2IndexBits = this->L2Size - this->L2OffsetBits - this->L2Assoc;
	this->L1TagBits = FULL_TAG_SIZE - this->L1IndexBits - this->L1OffsetBits;
	this->L2TagBits = FULL_TAG_SIZE - this->L2IndexBits - this->L2OffsetBits;

	// calculate the number of blocks, sets and ways
	this->L1NumBlocks = 1 << (this->L1Size - this->BSize);
	this->L2NumBlocks = 1 << (this->L2Size - this->BSize);
	this->L1NumSets = 1 << this->L1IndexBits;
	this->L2NumSets = 1 << this->L2IndexBits;
	this->L1NumWays = 1 << this->L1Assoc;
	this->L2NumWays = 1 << this->L2Assoc;

	// create the cache sets
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

unsigned int Cache::getL1Reads() {
	return this->L1Reads;
}

unsigned int Cache::getL1ReadMisses() {
	return this->L1ReadMisses;
}

unsigned int Cache::getL1Writes() {
	return this->L1Writes;
}

unsigned int Cache::getL1WriteMisses() {
	return this->L1WriteMisses;
}

unsigned int Cache::getL2Reads() {
	return this->L2Reads;
}

unsigned int Cache::getL2ReadMisses() {
	return this->L2ReadMisses;
}

unsigned int Cache::getL2Writes() {
	return this->L2Writes;
}

unsigned int Cache::getL2WriteMisses() {
	return this->L2WriteMisses;
}

unsigned int Cache::getTotalL1Cycles() {
	return this->totalL1Cycles;
}

unsigned int Cache::getTotalL2Cycles() {
	return this->totalL2Cycles;
}

unsigned int Cache::getTotalMemCycles() {
	return this->totalMemCycles;
}

void Cache::readFromCache(unsigned int fullTag) {
	// calculate the address of the line in L1
	unsigned int L1Index = (fullTag >> this->L1OffsetBits) & ((1 << this->L1IndexBits) - 1);
	unsigned int L1Tag = fullTag >> (this->L1OffsetBits + this->L1IndexBits);
	CacheSet* L1Set = &this->L1Sets[L1Index];
	// read from L1
	this->L1Reads++;
	this->totalL1Cycles += this->L1Cyc;
	if (L1Set->readFromLine(L1Tag)) { // L1 hit
		L1Set->updateLRU(L1Set->getWay(L1Tag));
	} 
	else { // L1 miss
		this->L1ReadMisses++;
		// calculate the address of the line in L2
		unsigned int L2Index = (fullTag >> this->L2OffsetBits) & ((1 << this->L2IndexBits) - 1);
		unsigned int L2Tag = fullTag >> (this->L2OffsetBits + this->L2IndexBits);
		CacheSet* L2Set = &this->L2Sets[L2Index];
		// read from L2
		this->L2Reads++;
		this->totalL2Cycles += this->L2Cyc;
		if (L2Set->readFromLine(L2Tag)) { // L2 hit
			unsigned int L2Way = L2Set->getWay(L2Tag);
			L2Set->updateLRU(L2Way);
			L2Set->updateDirty(L2Way, false);	
			// L2 hit, L1 miss, need to insert to L1
			this->L1MissHandler(L1Tag, L1Index);
		}
		else { // L2 miss
			this->L2ReadMisses++;
			this->totalMemCycles += this->MemCyc;
			// L2 miss, need to insert to L2 and L1
			this->L2MissHandler(L1Tag, L1Index, L2Tag, L2Index);
		}
	}
}

void Cache::writeToCache(unsigned int fullTag) {
	// calculate the address of the line in L1
	unsigned int L1Index = (fullTag >> this->L1OffsetBits) & ((1 << this->L1IndexBits) - 1);
	unsigned int L1Tag = fullTag >> (this->L1OffsetBits + this->L1IndexBits);
	CacheSet* L1Set = &this->L1Sets[L1Index];
	// write to L1
	this->L1Writes++;
	this->totalL1Cycles += this->L1Cyc;
	if (L1Set->writeToLine(L1Tag)) { // L1 hit
		L1Set->updateLRU(L1Set->getWay(L1Tag));
	}
	else { // L1 miss
		this->L1WriteMisses++;
		// calculate the address of the line in L2
		unsigned int L2Index = (fullTag >> this->L2OffsetBits) & ((1 << this->L2IndexBits) - 1);
		unsigned int L2Tag = fullTag >> (this->L2OffsetBits + this->L2IndexBits);
		CacheSet* L2Set = &this->L2Sets[L2Index];
		// write to L2
		this->L2Writes++;
		this->totalL2Cycles += this->L2Cyc;
		// if write allocate - need to bring the line into L1 and write only to L1
		if (this->WrAlloc == WRITE_ALLOCATE) {
			if (L2Set->isLineInSet(L2Tag)) { // L2 hit
				// update LRU in L2
				L2Set->updateLRU(L2Set->getWay(L2Tag));
				// insert line to L1
				this->L1MissHandler(L1Tag, L1Index);
				// write to L1
				L1Set->writeToLine(L1Tag);
				L1Set->updateLRU(L1Set->getWay(L1Tag));
			}
			else { // L2 miss
				this->L2WriteMisses++;
				this->totalMemCycles += this->MemCyc;
				// insert to L2 and L1 
				this->L2MissHandler(L1Tag, L1Index, L2Tag, L2Index);
				// write to L1
				L1Set->writeToLine(L1Tag);
				L1Set->updateLRU(L1Set->getWay(L1Tag));
			}
		}
		// if no write allocate - search for the line in L2 and write to L2. if not found, write to memory
		else { // no write allocate
			if (L2Set->isLineInSet(L2Tag)) { // L2 hit
				// write to L2
				L2Set->writeToLine(L2Tag);
				L2Set->updateLRU(L2Set->getWay(L2Tag));
			}
			else { // L2 miss
				this->L2WriteMisses++;
				// write to memory
				this->totalMemCycles += this->MemCyc;
			}
		}
	}
}

void Cache::L1MissHandler(unsigned int L1Tag, unsigned int L1Index) {
	CacheSet* L1Set = &this->L1Sets[L1Index];
	if (L1Set->insertLine(L1Tag)) {
		// successful insert
	}
	// L1 is full, need to evict
	else {
		CacheLine* evictedLine = L1Set->removeLine();
		if (evictedLine->getDirty()) {
			// calculate the address of the evicted line
			unsigned int evictedFullTag = (evictedLine->getTag() << (this->L1IndexBits)) | L1Index;
			unsigned int evictedL2Index = (evictedFullTag) & ((1 << this->L2IndexBits) - 1);
			unsigned int evictedL2Tag = evictedFullTag >> (this->L2IndexBits);
			// write to L2
			CacheSet* L2SetEvicted = &this->L2Sets[evictedL2Index];
			unsigned int evictedL2Way = L2SetEvicted->getWay(evictedL2Tag);
			L2SetEvicted->updateLRU(evictedL2Way);
			L2SetEvicted->updateDirty(evictedL2Way, false);
		}
		delete evictedLine;
		// write to L1 (there is a free line now)
		L1Set->insertLine(L1Tag);
	}
	L1Set->updateLRU(L1Set->getWay(L1Tag));
}


void Cache::L2MissHandler(unsigned int L1Tag, unsigned int L1Index, 
							  unsigned int L2Tag, unsigned int L2Index) {
	CacheSet* L2Set = &this->L2Sets[L2Index];
	// try to insert to L2
	if (L2Set->insertLine(L2Tag)) {
		// insert to L1
		this->L1MissHandler(L1Tag, L1Index);
	}
	// L2 is full, need to evict
	else {
		// select victim P from L2
		CacheLine* evictedLineL2 = L2Set->removeLine();
		unsigned int evictedFullTag = (evictedLineL2->getTag() << (this->L2IndexBits)) | L2Index;

		// snoop victim P from L1
		unsigned int evictedL1Index = (evictedFullTag) & ((1 << this->L1IndexBits) - 1);
		unsigned int evictedL1Tag = evictedFullTag >> (this->L1IndexBits);

		CacheSet* L1SetEvicted = &this->L1Sets[evictedL1Index];
		unsigned int evictedL1Way = L1SetEvicted->getWay(evictedL1Tag);
		// evict P from L1 if in L1 (update L2 if needed)
		if (evictedL1Way != (unsigned int)-1) { // if in L1
			// evict P from L1
			CacheLine* evictedLineL1 = L1SetEvicted->removeLine(evictedL1Way);
			if (evictedLineL1->getDirty()) {
				evictedLineL2->setDirty(true);
			}
			delete evictedLineL1;
		}
		// evict P from L2
		delete evictedLineL2;
		// write to memeory the evicted line
		
		// insert the line we missed to L2
		L2Set->insertLine(L2Tag);
		L2Set->updateLRU(L2Set->getWay(L2Tag));
		// insert to L1
		this->L1MissHandler(L1Tag, L1Index);
	}
}

/**********************************************************************************************/

int main(int argc, char **argv) {

	if (argc < 19) {
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
	Cache cache(MemCyc, BSize, L1Size, L2Size, L1Assoc, L2Assoc, 
				L1Cyc, L2Cyc, WrAlloc);

	while (getline(file, line)) {

		stringstream ss(line);
		string address;
		char operation = 0; // read (r) or write (w)
		if (!(ss >> operation >> address)) {
			// Operation appears in an Invalid format
			cout << "Command Format error" << endl;
			return 0;
		}

		string cutAddress = address.substr(2); // Removing the "0x" part of the address

		unsigned long int fullTag = 0;
		fullTag = strtoul(cutAddress.c_str(), NULL, 16);

		if (operation == 'r') {
			cache.readFromCache(fullTag);
		} else if (operation == 'w') {
			cache.writeToCache(fullTag);
		} else {
			// Operation appears in an Invalid format
			cout << "Operation Format error" << endl;
			return 0;
		}
	}
	

	// Calculate L1MissRate, L2MissRate, avgAccTime
	double L1MissRate = (double)(cache.getL1ReadMisses() + cache.getL1WriteMisses()) / 
						(cache.getL1Reads() + cache.getL1Writes());
	double L2MissRate = (double)(cache.getL2ReadMisses() + cache.getL2WriteMisses()) / 
						(cache.getL2Reads() + cache.getL2Writes());
	double avgAccTime = (double)(cache.getTotalL1Cycles() + cache.getTotalL2Cycles() + cache.getTotalMemCycles()) / 
						(cache.getL1Reads() + cache.getL1Writes());

	printf("L1miss=%.03f ", L1MissRate);
	printf("L2miss=%.03f ", L2MissRate);
	printf("AccTimeAvg=%.03f\n", avgAccTime);
	return 0;
}
