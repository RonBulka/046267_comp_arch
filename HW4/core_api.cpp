/* 046267 Computer Architecture - HW #4 */

#include "core_api.h"
#include "sim_api.h"

#include <stdio.h>

// static global variables for blocked MT
static unsigned int blockedInstCount = 0;
static unsigned int blockedCycles = 0;
// static global variables for fine-grained MT
static unsigned int finegrainedInstCount = 0;
static unsigned int finegrainedCycles = 0;

// struct for thread information
class threadInfo {
private:
	tcontext* context;
	unsigned int pc;
	unsigned int idleCyclesLeft;
	bool isHalted;
public:
	threadInfo() : pc(0), idleCyclesLeft(0), isHalted(false) {
		this->context = new tcontext;
		for (int i = 0; i < REGS_COUNT; i++) {
			this->context->reg[i] = 0;
		}
	}
	~threadInfo() {
		delete this->context;
	}
	tcontext* getContext() const {
		return this->context;
	}
	unsigned int getPC() const {
		return this->pc;
	}
	unsigned int getIdleCyclesLeft() const {
		return this->idleCyclesLeft;
	}
	bool getHalted() const {
		return this->isHalted;
	}
	void setPC(unsigned int newPC) {
		this->pc = newPC;
	}
	void setIdleCyclesLeft(unsigned int cycles) {
		this->idleCyclesLeft = cycles;
	}
	void setHalted(bool halted) {
		this->isHalted = halted;
	}
	void setReg(int reg, int value) {
		this->context->reg[reg] = value;
	}
	// copy context to dst context
	void setDstContext(tcontext* dst) {
		for (int i = 0; i < REGS_COUNT; i++) {
			dst->reg[i] = this->context->reg[i];
		}
	}
	void decreaseIdleCycles() {
		this->idleCyclesLeft--;
	}
	void nextPC() {
		this->pc++;
	}
};

// helper functions
// check if all threads are halted
bool areAllThreadsHalted(threadInfo threads[], int threadsNum) {
	for (int i = 0; i < threadsNum; i++) {
		if (!threads[i].getHalted()) {
			return false;
		}
	}
	return true;
}

// operations
// ADD dst <- src1 + src2
void ADD(threadInfo* thread, Instruction* inst) {
	tcontext* context = thread->getContext();
	int result = context->reg[inst->src1_index] + context->reg[inst->src2_index_imm];
	thread->setReg(inst->dst_index, result);
}

// SUB dst <- src1 - src2
void SUB(threadInfo* thread, Instruction* inst) {
	tcontext* context = thread->getContext();
	int result = context->reg[inst->src1_index] - context->reg[inst->src2_index_imm];
	thread->setReg(inst->dst_index, result);
}

// ADDI dst <- src1 + imm
void ADDI(threadInfo* thread, Instruction* inst) {
	tcontext* context = thread->getContext();
	int result = context->reg[inst->src1_index] + inst->src2_index_imm;
	thread->setReg(inst->dst_index, result);
}

// SUBI dst <- src1 - imm
void SUBI(threadInfo* thread, Instruction* inst) {
	tcontext* context = thread->getContext();
	int result = context->reg[inst->src1_index] - inst->src2_index_imm;
	thread->setReg(inst->dst_index, result);
}

// LOAD dst <- Mem[src1 + src2]  (src2 may be an immediate)
void LOAD(threadInfo* thread, Instruction* inst) {
	tcontext* context = thread->getContext();
	int32_t value;
	// check if src2 is immediate
	if (inst->isSrc2Imm) {
		SIM_MemDataRead(context->reg[inst->src1_index] + inst->src2_index_imm, &value);
	}
	else { // src2 is not immediate, read from reg[src2]
		SIM_MemDataRead(context->reg[inst->src1_index] + context->reg[inst->src2_index_imm], &value);
	}
	thread->setReg(inst->dst_index, value);
	// set idle cycles
	thread->setIdleCyclesLeft(SIM_GetLoadLat());
}

// STORE Mem[dst + src2] <- src1  (src2 may be an immediate)
void STORE(threadInfo* thread, Instruction* inst) {
	tcontext* context = thread->getContext();
	int32_t value = context->reg[inst->src1_index];
	// check if src2 is immediate
	if (inst->isSrc2Imm) {
		SIM_MemDataWrite(context->reg[inst->dst_index] + inst->src2_index_imm, value);
	}
	else { // src2 is not immediate, read from reg[src2]
		SIM_MemDataWrite(context->reg[inst->dst_index] + context->reg[inst->src2_index_imm], value);
	}
	// set idle cycles
	thread->setIdleCyclesLeft(SIM_GetStoreLat());
}

// HALT
void HALT(threadInfo* thread) {
	thread->setHalted(true);
}

// decrease idle cycles if needed
void updateIdleCycles(threadInfo threads[], int threadsNum) {
	for (int i = 0; i < threadsNum; i++) {
		// decrease idle cycles if thread is not halted and has idle cycles left
		if (!threads[i].getHalted() && threads[i].getIdleCyclesLeft() > 0) {
			threads[i].decreaseIdleCycles();
		}
	}
}

// switch threads in blocked MT
int blockedSwitchThreads(threadInfo threads[], int threadsNum, int currThread) {
	// if current thread is not halted and not in idle state
	if (!threads[currThread].getHalted() && 
	    (threads[currThread].getIdleCyclesLeft() == 0)) {
		return currThread;
	}
	// find next thread that is not halted and not in idle state
	int nextThread = (currThread + 1) % threadsNum;
	while (threads[nextThread].getHalted() || (threads[nextThread].getIdleCyclesLeft() > 0)){
		if (nextThread == currThread) { // all threads are halted or in idle state
			return currThread;
		}
		// move to next thread
		nextThread = (nextThread + 1) % threadsNum;
	}
	// switch to next thread, update idle cycles for all threads
	for (int i = SIM_GetSwitchCycles(); i > 0; i--) {
		updateIdleCycles(threads, threadsNum);
		blockedCycles++;
	}
	return nextThread;
}

// switch threads in fine-grained MT
int finegrainedSwitchThreads(threadInfo threads[], int threadsNum, int currThread) {
	// find next thread that is not halted and not in idle state
	int nextThread = (currThread + 1) % threadsNum;
	while (threads[nextThread].getHalted() || (threads[nextThread].getIdleCyclesLeft() > 0)){
		if (nextThread == currThread) { // all threads are halted or in idle state
			return currThread;
		}
		// move to next thread
		nextThread = (nextThread + 1) % threadsNum;
	}
	return nextThread;
}

// print all registers for all threads, for debugging
void printAllRegs(threadInfo threads[], int threadsNum) {
	for (int i = 0; i < threadsNum; i++) {
		tcontext* context = threads[i].getContext();
		printf("Thread %d:\n", i);
		for (int j = 0; j < REGS_COUNT; j++) {
			printf("reg[%d] = %d\n", j, context->reg[j]);
		}
	}
}

// static global variables holding threads information
static threadInfo* blockThreads;
static threadInfo* finegrainedThreads;

// API functions
void CORE_BlockedMT() {
	// get number of threads
	int threadsNum = SIM_GetThreadsNum();
	// initialize variables
	blockThreads = new threadInfo[threadsNum];
	Instruction* currInst = new Instruction;
	int currThread = 0;
	uint32_t currLine = 0;
	// run until all threads are halted
	while (!areAllThreadsHalted(blockThreads, threadsNum))
	{
		// check if current thread is halted or in idle state
		// deals with situation where all threads are halted or in idle state
		if (blockThreads[currThread].getHalted() || 
			blockThreads[currThread].getIdleCyclesLeft() > 0)
		{
			// updateIdleCycles for all threads
			updateIdleCycles(blockThreads, threadsNum);
			blockedCycles++;
			// switch to next thread if available
			currThread = blockedSwitchThreads(blockThreads, threadsNum, currThread);
			continue;
		}
		// get current instruction
		currLine = blockThreads[currThread].getPC();
		SIM_MemInstRead(currLine, currInst, currThread);
		// update idle cycles for all threads
		updateIdleCycles(blockThreads, threadsNum);
		// simulate instruction
		blockThreads[currThread].nextPC();
		blockedCycles++;
		blockedInstCount++;
		switch (currInst->opcode)
		{
			case CMD_NOP:
            	break;
			case CMD_ADD:
				ADD(&blockThreads[currThread], currInst);
				break;
			case CMD_SUB:
				SUB(&blockThreads[currThread], currInst);
				break;
			case CMD_ADDI:
				ADDI(&blockThreads[currThread], currInst);
				break;
			case CMD_SUBI:
				SUBI(&blockThreads[currThread], currInst);
				break;
			case CMD_LOAD:
				LOAD(&blockThreads[currThread], currInst);
				// switch to next thread - blocked MT
				currThread = blockedSwitchThreads(blockThreads, threadsNum, 
												  				currThread);
				break;
			case CMD_STORE:
				STORE(&blockThreads[currThread], currInst);
				// switch to next thread - blocked MT
				currThread = blockedSwitchThreads(blockThreads, threadsNum, 
																currThread);
				break;
			case CMD_HALT:
				HALT(&blockThreads[currThread]);
				// switch to next thread - blocked MT
				currThread = blockedSwitchThreads(blockThreads, threadsNum, 
																currThread);
				break;
			default:
				break;
		}
	}
	delete currInst;
}

void CORE_FinegrainedMT() {
	// get number of threads
	int threadsNum = SIM_GetThreadsNum();
	// initialize variables
	finegrainedThreads = new threadInfo[threadsNum];
	Instruction* currInst = new Instruction;
	int currThread = 0;
	uint32_t currLine = 0;
	// run until all threads are halted
	while (!areAllThreadsHalted(finegrainedThreads, threadsNum))
	{
		// check if current thread is halted or in idle state
		// deals with situation where all threads are halted or in idle state
		if (finegrainedThreads[currThread].getHalted() || finegrainedThreads[currThread].getIdleCyclesLeft() > 0)
		{
			//updateIdleCycles for all threads
			updateIdleCycles(finegrainedThreads, threadsNum);
			finegrainedCycles++;
			// switch to next thread if available
			currThread = finegrainedSwitchThreads(finegrainedThreads, threadsNum, 
																	  currThread);
			continue;
		}
		// get current instruction
		currLine = finegrainedThreads[currThread].getPC();
		SIM_MemInstRead(currLine, currInst, currThread);
		// update idle cycles for all threads
		updateIdleCycles(finegrainedThreads, threadsNum);
		// simulate instruction
		finegrainedThreads[currThread].nextPC();
		finegrainedCycles++;
		finegrainedInstCount++;
		switch (currInst->opcode)
		{
			case CMD_NOP:
            	break;
			case CMD_ADD:
				ADD(&finegrainedThreads[currThread], currInst);
				break;
			case CMD_SUB:
				SUB(&finegrainedThreads[currThread], currInst);
				break;
			case CMD_ADDI:
				ADDI(&finegrainedThreads[currThread], currInst);
				break;
			case CMD_SUBI:
				SUBI(&finegrainedThreads[currThread], currInst);
				break;
			case CMD_LOAD:
				LOAD(&finegrainedThreads[currThread], currInst);
				break;
			case CMD_STORE:
				STORE(&finegrainedThreads[currThread], currInst);
				break;
			case CMD_HALT:
				HALT(&finegrainedThreads[currThread]);
				break;
			default:
				break;
		}
		// switch to next thread - fine-grained MT
		currThread = finegrainedSwitchThreads(finegrainedThreads, threadsNum, 
																  currThread);
	}
	delete currInst;
}

void CORE_BlockedMT_CTX(tcontext* context, int threadid) {
	// set context for thread
	blockThreads[threadid].setDstContext(&context[threadid]);
}

void CORE_FinegrainedMT_CTX(tcontext* context, int threadid) {
	// set context for thread
	finegrainedThreads[threadid].setDstContext(&context[threadid]);
}

double CORE_BlockedMT_CPI() {
	// calculate CPI
	double cpi = (double)blockedCycles / blockedInstCount;
	// free memory
	delete[] blockThreads;
	return cpi;
}

double CORE_FinegrainedMT_CPI() {
	// calculate CPI
	double cpi = (double)finegrainedCycles / finegrainedInstCount;
	// free memory
	delete[] finegrainedThreads;
	return cpi;
}