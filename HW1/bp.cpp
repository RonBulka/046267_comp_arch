/* 046267 Computer Architecture - HW #1                                 */
/* This file should hold your implementation of the predictor simulator */

#include "bp_api.h"
int power(int base, int exp) {
	int result = 1;
	for (int i = 0; i < exp; i++) {
		result *= base;
	}
	return result;
}

static unsigned flush_num = 0;	// Machine flushes
static unsigned br_num = 0;		// Number of branch instructions
static unsigned btb_size;		// Theoretical allocated BTB and branch predictor size


class FSM_table {
	private:
		uint16_t size;
		int fsmState;
		uint8_t* table;	
	public:
		FSM_table(uint16_t size, int fsmState) : size(size), fsmState(fsmState) {
			table = new uint8_t[size];
			for (int i = 0; i < size; i++) {
				table[i] = fsmState;
			}
		}
		~FSM_table() {
			delete[] table;
		}

		void reset_table() {
			for (int i = 0; i < size; i++) {
				table[i] = fsmState;
			}
		}

		void update_table(bool taken, int index) {
			if(taken) {
				if(table[index] < 3) {
					table[index]++;
				}
			}
			else {
				if(table[index] > 0) {
					table[index]--;
				}
			}
		}

		bool give_decision(int index) {
			return table[index] > 1 ? true : false;
		}
};

void update_history(uint8_t* history, bool taken, unsigned historySize) {
	*history = *history << 1; // shift left once for the new bit
	*history |= (uint8_t)taken; // add the new bit
	// if(taken) {
	// 	*history = *history | 1;
	// }
	*history = *history & ((1 << historySize) - 1); // Mask the historySize least significant bits
}

class BTB_entry {
	private:
		uint32_t tag;
		uint32_t target;
		unsigned historySize;
	public:
		BTB_entry(unsigned historySize) : tag(0), target(0), 
									      historySize(historySize) {
		}
		virtual ~BTB_entry() {
		}
		void update_tag(uint32_t tag) {
			this->tag = tag;
		}
		void update_target(uint32_t target) {
			this->target = target;
		}
};

// local history and local fsm tables
class LHLT_BTB_entry : public BTB_entry {
	private:
		uint8_t history;
		FSM_table* fsm_table;
	public:
		LHLT_BTB_entry(unsigned historySize, unsigned fsmState, uint16_t fsmSize) :
		 			   BTB_entry(historySize) , history(0) {
			this->fsm_table = new FSM_table(fsmSize, fsmState);
		}
		~LHLT_BTB_entry() {
			delete fsm_table;
		}
		void update_fsm_table(bool taken) {
			fsm_table->update_table(taken, history);
		}
};

// local history and global fsm tables
class LHGT_BTB_entry : public BTB_entry {
	private:
		uint8_t history;
		FSM_table* fsm_table;
	public:
		LHGT_BTB_entry(unsigned historySize, FSM_table* fsm_table) :
					   BTB_entry(historySize), history(0), fsm_table(fsm_table) {
		}
		~LHGT_BTB_entry() {
			delete fsm_table;
		}
		void update_fsm_table(bool taken) {
			fsm_table->update_table(taken, history);
		}
};

// global history and local fsm tables
class GHLT_BTB_entry : public BTB_entry {
	private:
		uint8_t* history;
		FSM_table* fsm_table;
	public:
		GHLT_BTB_entry(uint8_t* history, unsigned historySize, 
					   unsigned fsmState, uint16_t fsmSize) :
					   BTB_entry(historySize), history(history) {
			this->fsm_table = new FSM_table(fsmSize, fsmState);
		}
		~GHLT_BTB_entry() {
			delete fsm_table;
		}
		void update_fsm_table(bool taken) {
			fsm_table->update_table(taken, *history);
		}
};

// global history and global fsm tables
class GHGT_BTB_entry : public BTB_entry {
	private:
		uint8_t* history;
		FSM_table* fsm_table;
	public:
		GHGT_BTB_entry(uint8_t* history, unsigned historySize, 
					   FSM_table* fsm_table) : BTB_entry(historySize), 
					   history(history), fsm_table(fsm_table) {
		}
		~GHGT_BTB_entry() {
			delete fsm_table;
		}
		void update_fsm_table(bool taken) {
			fsm_table->update_table(taken, *history);
		}	
};

class BTB {
	private:
		unsigned size;
		unsigned tagSize; 		// Tag size
		unsigned historySize; 	// History register size
		unsigned fsmState;		// FSM initial state
		bool isGlobalHist;		// History register type
		bool isGlobalTable;		// FSM table type
		int Shared;				// Type of shared FSM table (0 = not shared, 1 = xor lsb , 2 = xor mid)
		BTB_entry** btb;		// BTB table
		uint8_t* history;		// Global history register
		FSM_table* fsm_table;	// Global FSM table
		uint16_t fsmSize;		// FSM table size
	public:
		BTB(unsigned size, unsigned tagSize, unsigned historySize, unsigned fsmState,
			bool isGlobalHist, bool isGlobalTable, int Shared) : 
			size(size), tagSize(tagSize), historySize(historySize), 
			fsmState(fsmState), isGlobalHist(isGlobalHist), 
			isGlobalTable(isGlobalTable), Shared(Shared), btb(nullptr),
			history(nullptr), fsm_table(nullptr) 
		{
			uint16_t fsmSize = power(2, historySize);
			btb = new BTB_entry*[size];
			if(isGlobalHist) {
				history = new uint8_t;
			}
			if(isGlobalTable) {
				fsm_table = new FSM_table(fsmSize, fsmState);
			}
		}

		~BTB() {
			delete[] btb;
			// delete history;
			// delete fsm_table;
		}
		
		int init_BTB_entries() {
			// Initialize BTB entries
			if(isGlobalHist && isGlobalTable) {
				for (int i = 0; i < size; i++) {
					btb[i] = new GHGT_BTB_entry(history, historySize, fsm_table);
				}
		
			}
			else if(isGlobalHist && !isGlobalTable) {
				for (int i = 0; i < size; i++) {
					btb[i] = new GHLT_BTB_entry(history, historySize, fsmState,
											    fsmSize);
				}
			}
			else if(!isGlobalHist && isGlobalTable) {
				for (int i = 0; i < size; i++) {
					btb[i] = new LHGT_BTB_entry(historySize, fsm_table);
				}
			}
			else if(!isGlobalHist && !isGlobalTable) {
				for (int i = 0; i < size; i++) {
					btb[i] = new LHLT_BTB_entry(historySize, fsmState, fsmSize);
				}
			}
			return 0;
		}
};

static BTB* btb;

int BP_init(unsigned btbSize, unsigned historySize, unsigned tagSize, 
			unsigned fsmState, bool isGlobalHist, bool isGlobalTable,
			int Shared) {
	btb = new BTB(btbSize, tagSize, historySize, fsmState, isGlobalHist,
				  isGlobalTable, Shared);
	if (btb->init_BTB_entries() == 0) {
		return 0;
	}
	return -1;
}

bool BP_predict(uint32_t pc, uint32_t *dst) {
	return false;
}

void BP_update(uint32_t pc, uint32_t targetPc, bool taken, uint32_t pred_dst) {
	br_num++;
	return;
}

void BP_GetStats(SIM_stats *curStats) {
	curStats->flush_num = flush_num;
	curStats->br_num = br_num;
	curStats->size = btb_size;
	delete btb;
	return;
}

