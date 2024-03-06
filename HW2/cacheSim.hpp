#pragma ONCE
#ifndef CACHE_SIM_HPP
#define CACHE_SIM_HPP

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>

using  namespace std;

enum WriteAllocatePolicy {
    NO_WRITE_ALLOCATE = 0,
    WRITE_ALLOCATE = 1
};


class CacheLine {
private:
    unsigned int tag;
    bool valid;
    bool dirty;
    unsigned int LRU;
public:
    CacheLine(unsigned int initialLRU = 0);
    ~CacheLine();
    unsigned int getTag();
    bool getValid();
    bool getDirty();
    unsigned int getLRU();
    void setTag(unsigned int tag);
    void setValid(bool valid);
    void setDirty(bool dirty);
    void setLRU(unsigned int LRU);
    bool compareTag(unsigned int outsideTag);
};

class CacheSet {
private:
    unsigned int numWays;
    unsigned int lineCount;
    CacheLine* lines;
public:
    CacheSet();
    CacheSet(unsigned int numWays);
    ~CacheSet();
    void initSet(unsigned int numWays);
    void updateLRU(unsigned int way);
    bool insertLine(unsigned int tag);
    CacheLine* findLine(unsigned int tag);
    CacheLine* removeLine(unsigned int way);
    bool writeToLine(unsigned int tag);
    bool readFromLine(unsigned int tag);
};

// class CacheWays {
// private:
//     unsigned int numOfSets;
//     bool writeAllocate;
//     CacheSet* sets;
// public:
//     CacheWays(unsigned int numOfSets, unsigned int numOfWays, bool writeAllocate);
//     ~CacheWays();
//     void readFromCache(unsigned int index, unsigned int tag);
//     void writeToCache(unsigned int index, unsigned int tag);
// };

// Bsize = block size in bytes (given in log2) -> offset bits = Bsize
// Associativity = number of ways (given in log2)
// Lsize = size of the cache in bytes (given in log2)
// num of blocks = 2^LSize/2^Bsize
// num of sets = num of blocks / 2^Associativity
// num of bits for index = log2(num of sets) {Lsize - Bsize - Associativity}
// class Ways (sets), class Set (tag+data)
// dirty bit and LRU bit (for replacement policy, might be more than a bit), valid bit

// make a class for the specific index and tag bits which stores the assoc lines for a specific set
class Cache {
private:
    unsigned int MemCyc; // Memory cycles
    unsigned int BSize; // Block size in bytes (given in log2)
    unsigned int L1Size, L2Size; // L1 and L2 size in bytes (given in log2)
    unsigned int L1Assoc, L2Assoc; // L1 and L2 associativity (given in log2)
    unsigned int L1Cyc, L2Cyc; // L1 and L2 access time in cycles
    unsigned int WrAlloc; // Write allocate policy (0 = no write allocate, 1 = write allocate)

    unsigned int L1Reads, L1ReadMisses, L1Writes, L1WriteMisses, L1WriteBacks; // L1 cache stats (might not need writebacks)
    unsigned int L2Reads, L2ReadMisses, L2Writes, L2WriteMisses, L2WriteBacks; // L2 cache stats (might not need writebacks)
    unsigned int totalL1Cycles, totalL2Cycles, totalMemCycles; // total cycles for each cache and memory

    unsigned int BlockSize; // 2^BSize in bytes
    unsigned int L1OffsetBits, L2OffsetBits; // number of bits for offset (Bsize)
    unsigned int L1IndexBits, L2IndexBits; // number of bits for index (Lsize - Bsize - Associativity)
    unsigned int L1TagBits, L2TagBits; // number of bits for tag (32 - IndexBits - OffsetBits)

    unsigned int L1NumBlocks, L2NumBlocks; // number of blocks in L1 and L2 (2^LSize/2^BSize)
    unsigned int L1NumSets, L2NumSets; // number of sets in L1 and L2 (2^IndexBits)
    unsigned int L1NumWays, L2NumWays; // number of ways in L1 and L2 (2^L1Assoc, 2^L2Assoc)

    CacheSet *L1Sets, *L2Sets;
public:
    Cache(unsigned int MemCyc, unsigned int BSize, unsigned int L1Size, unsigned int L2Size,
            unsigned int L1Assoc, unsigned int L2Assoc, unsigned int L1Cyc, unsigned int L2Cyc,
            unsigned int WrAlloc);
    ~Cache();
    void readFromCache(unsigned int index, unsigned int tag);
    void writeToCache(unsigned int index, unsigned int tag);
};

#endif // CACHE_SIM_HPP