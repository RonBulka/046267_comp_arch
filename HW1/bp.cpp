/* 046267 Computer Architecture - HW #1                                 */
/* This file should hold your implementation of the predictor simulator */

#include "bp_api.h"
#define TARGETSIZE 30
#define VALIDBIT 1
#define STATEMACHINESIZE 2
#define NEXTPC 4
static unsigned flush_num = 0;	// Machine flushes
static unsigned br_num = 0;		// Number of branch instructions
static unsigned btb_size;		// Theoretical allocated BTB and branch predictor size

/**
 * @brief calculate the power of base to the power of exp
 * 
 * @param base base number
 * @param exp  exponent
 * @return result of base^exp
 */
int power(int base, int exp) {
	int result = 1;
	for (int i = 0; i < exp; i++) {
		result *= base;
	}
	return result;
}

/**
 * @brief calculate the log2 of a number
 * 
 * @param n number
 * @return log2 of n
 */
unsigned log2(unsigned n) {
	unsigned result = 0;
	while (n >>= 1) { // shift right until n = 0
		result++;
	}
	return result;
}

/**
 * @brief calculate the size of the branch predictor
 * 
 * @param size BTB size
 * @param tagSize tag size
 * @param historySize history register size
 * @param fsmSize FSM table size
 * @param isGlobalHist global history register
 * @param isGlobalTable global FSM table
 * @return size of the branch predictor
 */
unsigned calculate_size(unsigned size, unsigned tagSize, unsigned historySize, 
						unsigned fsmSize, bool isGlobalHist, bool isGlobalTable) {
	unsigned btb_size = 0;
	// the BTB entry always contains the tag, target and valid bit
	// the tag is <tagSize> bits, the target is 30 bits and the valid bit is 1 bit
	if (isGlobalHist && isGlobalTable) { // global history and global fsm tables
		btb_size = size * (VALIDBIT + tagSize + TARGETSIZE) 
				   + historySize + STATEMACHINESIZE * fsmSize;
	}
	else if (isGlobalHist && !isGlobalTable) { // global history and local fsm tables
		btb_size = size * (VALIDBIT + tagSize + TARGETSIZE + historySize) 
				   + STATEMACHINESIZE * fsmSize;
	}
	else if (!isGlobalHist && isGlobalTable) { // local history and global fsm tables
		btb_size = size * (VALIDBIT + tagSize + TARGETSIZE + STATEMACHINESIZE * fsmSize) 
				   + historySize;
	}
	else if (!isGlobalHist && !isGlobalTable) { // local history and local fsm tables
		btb_size = size * (VALIDBIT + tagSize + TARGETSIZE + historySize + STATEMACHINESIZE * fsmSize);
	}	
	return btb_size;
}

/**
 * @brief Finite State Machine table, used for branch prediction
 * 
 */
class FSM_table {
	private:
		uint16_t size;
		int fsmState;
		uint8_t* table;	
	public:
		// Constructor
		FSM_table(uint16_t size, int fsmState) : size(size), fsmState(fsmState) {
			table = new uint8_t[size];
			for (int i = 0; i < size; i++) {
				table[i] = fsmState;
			}
		}
		// Destructor
		~FSM_table() {
			delete[] table;
		}
		// Reset the FSM table
		void reset_table() {
			for (int i = 0; i < size; i++) {
				table[i] = fsmState;
			}
		}
		// Update the FSM table
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
		// Get the decision from the FSM table
		bool give_decision(int index) {
			return (table[index] >> 1) ? true : false;
		}
};

/**
 * @brief Branch Target Buffer entry, parent of all BTB entries
 * 
 */
class BTB_entry {
	private:
		uint32_t tag;
		uint32_t target;
		unsigned historySize;
	public:
		// Constructor
		BTB_entry(unsigned historySize) : tag(0), target(0), 
									      historySize(historySize) {
		}
		// Destructor
		virtual ~BTB_entry() {
		}
		// Update the tag and target
		void update_tag(uint32_t tag) {
			this->tag = tag;
		}
		void update_target(uint32_t target) {
			this->target = target;
		}
		// Get the tag, target and history size
		uint32_t get_target() {
			return target;
		}
		uint32_t get_tag() {
			return tag;
		}
		unsigned get_historySize() {
			return historySize;
		}
		// Compare the tag
		bool compare_tag(uint32_t tag) {
			return this->tag == tag;
		}
		// Pure virtual functions
		virtual void update_fsm_table(bool taken) = 0;
		virtual uint8_t& get_history() = 0;
		virtual bool predict_decision() = 0;
		virtual void replace_entry(uint32_t tag, uint32_t target) = 0;
		// Update the history
		void update_history(uint8_t* history, bool taken, unsigned historySize) {
			*history = *history << 1; // shift left once for the new bit
			*history |= (uint8_t)taken; // add the new bit
			*history = *history & ((1 << historySize) - 1); // Mask the historySize least significant bits
		}
};

// local history and local fsm tables
class LHLT_BTB_entry : public BTB_entry {
	private:
		uint8_t history;
		FSM_table* fsm_table;
	public:
		// Constructor
		LHLT_BTB_entry(unsigned historySize, unsigned fsmState, uint16_t fsmSize) :
		 			   BTB_entry(historySize) , history(0) {
			this->fsm_table = new FSM_table(fsmSize, fsmState);
		}
		// Destructor
		~LHLT_BTB_entry() {
			delete fsm_table;
		}
		// Update the FSM table
		void update_fsm_table(bool taken) {
			fsm_table->update_table(taken, history);
		}
		// Get the history
		uint8_t& get_history() {
			return history;
		}
		// Get the decision from the FSM table
		bool predict_decision() {
			return fsm_table->give_decision(history);
		}
		// Replace the entry
		void replace_entry(uint32_t tag, uint32_t target) {
			update_tag(tag);
			update_target(target);
			history = 0;
			fsm_table->reset_table();
		}
};

// local history and global fsm tables
class LHGT_BTB_entry : public BTB_entry {
	private:
		uint8_t history;
		FSM_table* fsm_table;
		uint8_t Shared;
	public:
		// Constructor
		LHGT_BTB_entry(unsigned historySize, FSM_table* fsm_table, uint8_t Shared) :
					   BTB_entry(historySize), history(0), fsm_table(fsm_table), Shared(Shared) {
		}
		// Destructor
		~LHGT_BTB_entry() {
			delete fsm_table;
		}
		// Calculate the index for the FSM table
		uint8_t calc_fsm_index() {
			if(Shared == 1) { // xor historySize lsb
				return history ^ (get_tag() & ((1 << get_historySize()) - 1));
			}
			else if(Shared == 2) { // xor 16 from mid
				return history ^ ((get_tag() >> 16) & ((1 << get_historySize()) - 1));
			}
			return history; // not shared
			
		}
		// Update the FSM table
		void update_fsm_table(bool taken) {
			fsm_table->update_table(taken, calc_fsm_index());
		}
		// Get the history
		uint8_t& get_history() {
			return history;
		}
		// Get the decision from the FSM table
		bool predict_decision() {
			return fsm_table->give_decision(calc_fsm_index());
		}
		// Replace the entry
		void replace_entry(uint32_t tag, uint32_t target) {
			update_tag(tag);
			update_target(target);
			history = 0;
		}
};

// global history and local fsm tables
class GHLT_BTB_entry : public BTB_entry {
	private:
		uint8_t* history;
		FSM_table* fsm_table;
	public:
		// Constructor
		GHLT_BTB_entry(uint8_t* history, unsigned historySize, 
					   unsigned fsmState, uint16_t fsmSize) :
					   BTB_entry(historySize), history(history) {
			this->fsm_table = new FSM_table(fsmSize, fsmState);
		}
		// Destructor
		~GHLT_BTB_entry() {
			delete fsm_table;
		}
		// Update the FSM table
		void update_fsm_table(bool taken) {
			fsm_table->update_table(taken, *history);
		}
		// Get the history
		uint8_t& get_history() {
			return *history;
		}
		bool predict_decision() {
			return fsm_table->give_decision(*history);
		}
		void replace_entry(uint32_t tag, uint32_t target) {
			update_tag(tag);
			update_target(target);
			fsm_table->reset_table();
		}
};

// global history and global fsm tables
class GHGT_BTB_entry : public BTB_entry {
	private:
		uint8_t* history;
		FSM_table* fsm_table;
		uint8_t Shared;
	public:
		// Constructor
		GHGT_BTB_entry(uint8_t* history, unsigned historySize, 
					   FSM_table* fsm_table, uint8_t Shared) : BTB_entry(historySize), 
					   history(history), fsm_table(fsm_table), Shared(Shared) {
		}
		// Destructor
		~GHGT_BTB_entry() {
			delete fsm_table;
		}
		// Calculate the index for the FSM table
		uint8_t calc_fsm_index() {
			if(Shared == 1) { // xor historySize lsb
				return *history ^ (get_tag() & ((1 << get_historySize()) - 1));
			}
			else if(Shared == 2) { // xor 16 from mid
				return *history ^ ((get_tag() >> 16) & ((1 << get_historySize()) - 1));
			}
			return *history; // not shared
			
		}
		// Update the FSM table
		void update_fsm_table(bool taken) {
			fsm_table->update_table(taken, calc_fsm_index());
		}
		// Get the history
		uint8_t& get_history() {
			return *history;
		}
		// Get the decision from the FSM table
		bool predict_decision() {
			return fsm_table->give_decision(calc_fsm_index());
		}
		// Replace the entry
		void replace_entry(uint32_t tag, uint32_t target) {
			update_tag(tag);
			update_target(target);
		}

};

/**
 * @brief Branch Target Buffer, stores the BTB entries
 * 
 */
class BTB {
	private:
		unsigned size;				// BTB row number
		unsigned tagSize; 			// Tag size
		unsigned historySize; 		// History register size
		unsigned fsmState;			// FSM initial state
		bool isGlobalHist;			// History register type
		bool isGlobalTable;			// FSM table type
		int Shared;					// Type of shared FSM table (0 = not shared, 1 = xor lsb , 2 = xor mid)
		BTB_entry** btb;			// BTB table
		uint8_t* history;			// Global history register
		FSM_table* fsm_table;		// Global FSM table
		uint16_t fsmSize;			// FSM table size
		unsigned btb_index_size;	// BTB index size
	public:
		// Constructor
		BTB(unsigned size, unsigned tagSize, unsigned historySize, unsigned fsmState,
			bool isGlobalHist, bool isGlobalTable, int Shared) : 
			size(size), tagSize(tagSize), historySize(historySize), 
			fsmState(fsmState), isGlobalHist(isGlobalHist), 
			isGlobalTable(isGlobalTable), Shared(Shared), btb(nullptr),
			history(nullptr), fsm_table(nullptr) 
		{
			// Calculate the size of the FSM table
			uint16_t fsmSize = power(2, historySize);
			// Allocate the BTB table
			btb = new BTB_entry*[size];
			// Allocate the global history register and the global FSM table
			if(isGlobalHist) {
				history = new uint8_t;
			}
			if(isGlobalTable) {
				fsm_table = new FSM_table(fsmSize, fsmState);
			}
			// Calculate the BTB index size
			btb_index_size = log2(size);
			// Calculate the size of the branch predictor
			btb_size = calculate_size(size, tagSize, historySize, 
									  fsmSize, isGlobalHist, isGlobalTable);
		}
		// Destructor
		~BTB() {
			delete[] btb;
			if (isGlobalHist) {
				delete history;
			}
			if (isGlobalTable) {
				delete fsm_table;
			}
		}
		// Initialize the BTB entries
		int init_BTB_entries() {
			// Initialize BTB entries
			if(isGlobalHist && isGlobalTable) {
				for (unsigned i = 0; i < size; i++) {
					btb[i] = new GHGT_BTB_entry(history, historySize, fsm_table,
												Shared);
				}
			}
			else if(isGlobalHist && !isGlobalTable) {
				for (unsigned i = 0; i < size; i++) {
					btb[i] = new GHLT_BTB_entry(history, historySize, fsmState,
											    fsmSize);
				}
			}
			else if(!isGlobalHist && isGlobalTable) {
				for (unsigned i = 0; i < size; i++) {
					btb[i] = new LHGT_BTB_entry(historySize, fsm_table, Shared);
				}
			}
			else if(!isGlobalHist && !isGlobalTable) {
				for (unsigned i = 0; i < size; i++) {
					btb[i] = new LHLT_BTB_entry(historySize, fsmState, fsmSize);
				}
			}
			else {
				return -1;
			}
			return 0;
		}
		// Getters
		unsigned get_size() {
			return size;
		}
		unsigned get_btb_index_size() {
			return btb_index_size;
		}
		BTB_entry* get_entry(uint32_t full_tag) {
			uint8_t tag_index = full_tag & (size - 1);
			return btb[tag_index];
		} 
};

static BTB* btb;

int BP_init(unsigned btbSize, unsigned historySize, unsigned tagSize, 
			unsigned fsmState, bool isGlobalHist, bool isGlobalTable,
			int Shared) {
	// Initialize the BTB
	btb = new BTB(btbSize, tagSize, historySize, fsmState, isGlobalHist,
				  isGlobalTable, Shared);
	// Initialize the BTB entries
	if (btb->init_BTB_entries() == 0) {
		return 0; // success
	}
	return -1; // failure
}

bool BP_predict(uint32_t pc, uint32_t *dst) {
	// Calculate the full tag from the PC (30 MSB bits of the PC)
	uint32_t full_tag = pc >> 2;
	// Get the BTB entry
	BTB_entry* entry = btb->get_entry(full_tag);
	// Calculate the tag from the full tag (throw away the index bits)
	uint32_t tag = full_tag >> (btb->get_btb_index_size());
	// Check if the tag matches the entry tag
	if (entry->compare_tag(tag)) {
		// If the prediction is taken, return the target
		if (entry->predict_decision()) {
			*dst = entry->get_target();
			return true;
		}
	}
	// If the prediction is not taken or isnt in the BTB, return the next PC
	*dst = pc + NEXTPC;
	return false;
}

void BP_update(uint32_t pc, uint32_t targetPc, bool taken, uint32_t pred_dst) {
	br_num++;
	if (taken) { // if taken, flush if prediction was not taken
		if (pred_dst != targetPc) {
			flush_num++;
		}
	}
	else { // if not taken, flush if prediction was taken
		if (pred_dst != (pc + NEXTPC)) {
			flush_num++;
		}
	}
	// Calculate the full tag from the PC (30 MSB bits of the PC)
	uint32_t full_tag = pc >> 2;
	// Get the BTB entry
	BTB_entry* entry = btb->get_entry(full_tag);
	// Calculate the tag from the full tag (throw away the index bits)
	uint32_t tag = full_tag >> (btb->get_btb_index_size());
	// Update the BTB entry
	if (entry->compare_tag(tag)) {
		entry->update_fsm_table(taken);
		entry->update_target(targetPc); // in case there are 2 branches with the same tag
	}
	// If the tag does not match, replace the entry
	else {
		entry->replace_entry(tag, targetPc);
	}
	// Update the history
	entry->update_history(&entry->get_history(), taken, entry->get_historySize());
	return;
}

void BP_GetStats(SIM_stats *curStats) {
	// Update the statistics
	curStats->flush_num = flush_num;
	curStats->br_num = br_num;
	curStats->size = btb_size;
	// Delete the BTB
	delete btb;
	return;
}