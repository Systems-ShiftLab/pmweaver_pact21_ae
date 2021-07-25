#ifndef SHIFTLAB_PENDING_TABLE_H__
#define SHIFTLAB_PENDING_TABLE_H__

#define ALL_SUYASH__
#include "helper_suyash.h"
#undef ALL_SUYASH__

#include "Constants.hh"
#include "DataStore.hh"
#include "Declarations.hh"
#include "mem/packet.hh"
#include "mem/predictor/CacheLine.hh"
#include "mem/predictor/ChunkInfo.hh"
#include "mem/predictor/FixedSizeQueue.hh"
#include "mem/predictor/SharedArea.hh"
#include "mem/predictor/WriteHistoryBuffer.hh"

#include "base/trace.hh"
#include "debug/PendingTable.hh"

#include <unordered_map>
#include <deque>

typedef PC_t PendingTableKey; 

/** Acts as the entry for the pending Table */
class PendingTableEntryParent {
private:
    /* For bookkeeping prediction confidences */
    hash_t generatorHash;
    bool hasGeneratorHash = false;

    CacheLine originalCacheLine;
    bool hasOrigCL = false;

    size_t ihbPatternIndex;
    bool hasIhbPatternIndex = false;
public:
    static const size_t DATA_CHUNK_CNT = 64/sizeof(DataChunk);

    PC_t pc;

    /**
     * Holds the prediction status of the addr chunks for this parent
     * Bool value indicates whether this parent's address is predicted
    */
    bool addrComplete = false;
    ChunkInfo addr;

    /**
     * Holds the prediction status of the data chunks for this parent
     * Bool at index at i indicates whether this datachunk's value is
     * predicted
    */
    bool dataComplete[DATA_CHUNK_CNT] = {false};
    CacheLine cacheline;

    uint64_t allComplete() const {
        uint64_t result = (uint64_t)this->addrComplete;

        for (int i = 0; i < DATA_CHUNK_CNT; i++) {
            // TODO: Uncomment this
            if (this->cacheline.get_datachunks()[i].is_valid()) { 
                /* Increment this block with the dataComplete only if this block is valid */               
                result += (uint64_t)dataComplete[i];
            } else {
                /* An invalid block is always complete */
                result += 1;
            }
        }

        return result;
    }

    void set_generator_hash(hash_t generatorHash) {
        this->hasGeneratorHash = true;
        this->generatorHash = generatorHash;
    }

    hash_t get_generator_hash() const {
        assert(hasGeneratorHash);
        return this->generatorHash;
    }

    void set_original_cacheline(CacheLine originalCacheLine) {
        this->hasOrigCL = true;
        this->originalCacheLine = originalCacheLine;
    }

    CacheLine get_original_cacheline() const {
        assert(hasOrigCL);
        return this->originalCacheLine;
    }

    void set_ihb_pattern_index(size_t index) {
        this->hasIhbPatternIndex = true;
        this->ihbPatternIndex = index;
    }

    size_t get_ihb_pattern_index() const {
        assert(this->hasIhbPatternIndex);
        return this->ihbPatternIndex;
    }

};

/***/
class PendTableChunkInfo : public ChunkInfo {
private:
    /**
     * Points to a data structure represent the parent write of this
     * pending table entry. `parent` is required for identifying when 
     * a write is predicted.
    */
    PendingTableEntryParent *parent = nullptr;
    /**
     * Index at which this chunk sits in it's parent cache line, 
     * for the generating write's offset use this->get_data_field_offset()
     */
    int parentIndex = -1;
    bool hasParentIndex = false;

public:
    PendTableChunkInfo() : parent(nullptr), parentIndex(-1) {}
    PendTableChunkInfo(ChunkInfo chunkInfo, PendingTableEntryParent *parent, int parentIndex) : ChunkInfo(chunkInfo), parent(parent), parentIndex(parentIndex) {
        this->hasParentIndex = true;
    }
    PendTableChunkInfo(PC_t generatingPC, size_t dataFieldOffset, bool isComplete, ChunkType chunkType, PendingTableEntryParent *parent)
        : ChunkInfo(generatingPC, dataFieldOffset, isComplete, chunkType), parent(parent) {
        this->hasParentIndex = false;
        }

    PendingTableEntryParent* get_parent() const {
        return this->parent;
    }

    void set_parent(PendingTableEntryParent* parent) {
        this->parent = parent;
    }

    int get_parent_index() const {
        panic_if(not hasParentIndex, "Parent index was not set");
        return this->parentIndex;
    }

    void set_parent_index(int parentIndex) {
        this->hasParentIndex = true;
        this->parentIndex = parentIndex;
    }

    bool has_parent_index() const {
        return this->hasParentIndex;
    }

    virtual void clear() override {
        ChunkInfo::clear();
        this->parent = nullptr;
        this->parentIndex = -1;
    }
};

using T = PC_t;
using U = PendTableChunkInfo;
// using PTQueuedEntry = std::deque<PendingTableEntry>;

/**
 * Holds all the non-volatile writes that still haven't been generated 
 * using information from volatile writes history 
*/
class PendingTable : DataStore<U> {
private:
    std::unordered_map<T, std::deque<U>> pendingTable;

    /** 
     * pendingVolatilePCs uses an unordered_map to emulate a counting bloom filter. This 
     * filter is used to search for PCs that one or more of the entries of the 
     * pendingTable are waiting on.
     */
    std::unordered_map<PC_t, uint64_t> pendingVolatilePCs;
    Stats::Distribution pendingVolatilePCsSize;

    /**
     * Hacky stuff for implementing fifo order in the pcfilter and the pending table. Sorry.
    */
    std::deque<PC_t> insertionOrder;
    size_t MAX_SIZE = 256/2;
    /* Disables searching whb for marked elements on insertions */
    FixedSizeQueue<WriteHistoryBufferEntry> *whb = nullptr;
public:
    bool DISABLE_WHB_SEARCH = false;

    PendingTable(std::string, FixedSizeQueue<WriteHistoryBufferEntry>*);
    
    bool add(const U elem) override;
    
    bool contains(T key);

    bool remove() override { unimplemented__("") };
    bool remove_elem(const U entry) override { unimplemented__("") };
    bool remove_elem(T key);

    U& get() override { unimplemented__(""); }

    U& get(T key);
    
    bool add_back(const U elem) override { return this->add(elem); };
    bool add_front(const U elem) override { return this->add(elem); };
    
    bool remove_back() override { return this-> remove(); };
    bool remove_front() override { return this-> remove(); };
    
    U& get_back() override { unimplemented__(""); };
    U& get_front() override { unimplemented__(""); };

    size_t get_size() override;

    bool has_pc_waiting(PC_t pc) const;

    std::deque<PendingTableEntryParent*> update_entry_state(PC_t pc, const DataChunk *dataChunks, size_t size);

    PendingTableEntryParent* get_completed_parent(PC_t pc);

};

#endif // SHIFTLAB_PENDING_TABLE_H__
