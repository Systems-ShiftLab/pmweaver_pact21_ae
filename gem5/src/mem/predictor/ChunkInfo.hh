#ifndef SHIFTLAB_CHUNK_INFO_H__
#define SHIFTLAB_CHUNK_INFO_H__

#include "mem/predictor/Declarations.hh"
#include <bits/stdc++.h>
/**
 * Single class that represents either the target address of one of the
 * data chunk
*/
class ChunkInfo {
public:
    /** 
     * ChunkInfo either holds the targetAddr (Addr_t) of the write or the 
     * data (DataChunk) for the write.
    */
    union Content {
        Addr_t targetAddr;
        DataChunk data;
    };

    /** 
     * Defines the type of the chunk, possibly choices are:
     * ADDR: Indicates that this chunk holds the target address of a write
     * DATA: Indicates that this chunk holds one of the DataChunk of a write
     * INVALID: This chunk is invalid and holds neither addr nor data
     * MAX: Not a legal option, used for calculating enum size
    */
    enum class ChunkType {
        ADDR,
        DATA,
        INVALID,
        MAX
    };
private:
    enum Flags : uint64_t {
        HAS_GEN_PC                  = 1<<1,
        HAS_DATA_FIELD_OFF          = 1<<2,
        HAS_CONTENT                 = 1<<3,
        HAS_OWNER_KEY               = 1<<4,
        HAS_TIME_OF_GEN             = 1<<5,

        /* Used for data chunks that are always 0 */
        CONST_0_PREDICTION          = 1<<6,
        HAS_DATA                    = 1<<7,
        IS_FREE_PREDICTION          = 1<<8,
        CONSTANT_PREDICTION         = 1<<9,

#ifdef DIAGNOSTICS_MATCHING_PC
        HAS_MATCHING_PC             = 1<<10,
#endif // DIAGNOSTICS_MATCHING_PC
        HAS_GEN_PC_IN_TICK          = 1<<11,
        WHB_SEARCH                  = 1<<12,
        MAX                         = 1<<13
    };

    /** PC that generates this value */
    PC_t generatingPC;
    Tick genPCTimeOfInsertion;

    /** Offset in the data field of the write by generatingPC */
    size_t dataFieldOffset = 0;

    bool isComplete = false;
    ChunkType chunkType;
    Content content;
    PC_t owner_key;

    /* Holds the validity status of data held by this chunk */
    uint64_t flags = 0UL;

    Tick timeOfGen;

#ifdef DIAGNOSTICS_MATCHING_PC
    std::deque<PC_t> matchingPCs;
#endif // DIAGNOSTICS_MATCHING_PC

public:
    static size_t valid_count(const ChunkInfo *chunks, size_t chunkCount) {
        size_t validCount = 0;
        for (int i = 0; i < chunkCount; i++) {
            if (chunks[i].is_valid()) {
                validCount++;
            }
        }
        return validCount;
    }

    friend std::ostream& operator<<(std::ostream& os, const ChunkInfo& dt);

    ChunkInfo() : generatingPC(0), dataFieldOffset(0), isComplete(false), chunkType(ChunkType::INVALID)  {
        this->content.data = 0;
    }
        
    ChunkInfo(PC_t generatingPC, size_t dataFieldOffset, bool isComplete, ChunkType chunkType)
        : generatingPC(generatingPC), dataFieldOffset(dataFieldOffset), 
          isComplete(isComplete), chunkType(chunkType) {
        panic_if(dataFieldOffset == 24, "=-=");
        if (generatingPC != 0) {
            this->flags |= Flags::HAS_GEN_PC;
        }

        if (dataFieldOffset != -1) {
            this->flags |= Flags::HAS_DATA_FIELD_OFF;
        }
    }
    
    virtual void clear() {
        this->flags = 0UL;

        this->dataFieldOffset = -1;
        this->generatingPC = 1;
        this->isComplete = false;

        this->chunkType = ChunkType::DATA;
        this->set_data(0);
        this->chunkType = ChunkType::INVALID;
    }

    bool has_generating_pc() const {
        return this->flags & Flags::HAS_GEN_PC;
    }

    PC_t get_generating_pc() const {
        panic_if_not(this->has_generating_pc());
        return this->generatingPC;
    }

    void set_gen_pc_in_tick(Tick genPCTimeOfInsertion) {
        this->flags |= Flags::HAS_GEN_PC_IN_TICK;
        this->genPCTimeOfInsertion = genPCTimeOfInsertion;
    }

    Tick get_gen_pc_in_tick() const {
        panic_if_not(this->flags & Flags::HAS_GEN_PC_IN_TICK);
        return this->genPCTimeOfInsertion;
    }

    ChunkInfo &set_generating_pc(PC_t generatingPC) {
        this->flags |= Flags::HAS_GEN_PC;
        this->generatingPC = generatingPC;
        return *this;
    }

    bool has_owner_key () const {
        return this->flags & Flags::HAS_OWNER_KEY;
    }

    PC_t get_owner_key() const {
        panic_if_not(this->has_owner_key());
        return this->owner_key;
    }

    ChunkInfo &set_owner_key(PC_t owner_key) {
        this->flags |= Flags::HAS_OWNER_KEY;
        this->owner_key = owner_key;
        return *this;
    }

    Addr_t get_target_addr() const {
        panic_if_not(chunkType == ChunkType::ADDR);
        return this->content.targetAddr;
    }

    ChunkInfo &set_target_addr(Addr_t targetAddr) {
        panic_if_not(chunkType == ChunkType::ADDR);
        this->content.targetAddr = targetAddr;
        return *this;
    }

    DataChunk get_data() const {
        panic_if_not(this->flags & Flags::HAS_DATA);
        panic_if_not(chunkType == ChunkType::DATA);
        return this->content.data; 
    }

    ChunkInfo &set_data(DataChunk data) {
        this->flags |= Flags::HAS_DATA;
        if (DTRACE(ChunkInfo)) {
            std::stringstream ss;
            ss << "Called " << __FILE__ << ":" << __FUNCTION__ << "() using data = " << data << " on " << *this << std::endl;
            DPRINTF(ChunkInfo, "%s", ss.str().c_str());
        }
        
        panic_if_not(chunkType == ChunkType::DATA);
        
        this->content.data = data;
        return *this;
    }

    ChunkInfo &set_time_of_gen(Tick tick) {
        this->flags |= Flags::HAS_TIME_OF_GEN;
        this->timeOfGen = tick;
        return *this;
    }

    bool has_time_of_gen() {
        return this->flags & Flags::HAS_TIME_OF_GEN;
    }

    Tick get_time_of_gen() {
        panic_if_not(this->flags & Flags::HAS_TIME_OF_GEN);
        return this->timeOfGen;
    }

    bool has_data_field_offset() const {
        return this->flags & Flags::HAS_DATA_FIELD_OFF;
    }

    size_t get_data_field_offset() const {
        panic_if_not(this->flags & Flags::HAS_DATA_FIELD_OFF);
        return this->dataFieldOffset;
    }

    ChunkInfo &set_data_field_offset(size_t dataFieldOffset) {
        panic_if(dataFieldOffset == 24, "=-=");
        this->flags |= Flags::HAS_DATA_FIELD_OFF;
        this->dataFieldOffset = dataFieldOffset;
        return *this;
    }

    bool is_valid() const {
        panic_if_not(this->chunkType != ChunkType::MAX);
        return (this->chunkType != ChunkType::INVALID);
    }

    bool is_invalid() const {
        return !this->is_valid();
    }

    /**
     * Indicates if this chunk's generating pc is found
     * @return Bool value indicating if the generating PC for this entry
     *         was found
    */
    bool get_completion() const {
        return this->isComplete;
    }

    /**
     * Sets the completion status of the chunk, used while searching for 
     * generating PC during training
     * @param isComplete bool value representing whether this chunk was    
     *        found during training
     * @return Reference to itself
    */
    ChunkInfo &set_completion(bool isComplete) {
        this->isComplete = isComplete;
        return *this;
    }

    ChunkType get_chunk_type() const {
        return this->chunkType;
    }

    ChunkInfo &set_chunk_type(ChunkType chunkType) {
        this->chunkType = chunkType;
        return *this;
    }

    bool is_const_0_pred() const {
        // std::cout << "Checking for constant 0" << std::endl;
        // std::cout << "flags " << std::bitset<12>(flags) << std::endl;
        return this->flags & Flags::CONST_0_PREDICTION;
    }

    void set_const_0_pred() {
        this->flags |= Flags::CONST_0_PREDICTION;
    }


    void unset_const_0_pred() {
        this->flags &= ~Flags::CONST_0_PREDICTION;
    }

    void set_free_prediction() {
        this->flags |= Flags::IS_FREE_PREDICTION;
    }

    bool is_free_prediction() const {
        return this->flags & Flags::IS_FREE_PREDICTION;
    }

    bool is_constant_pred() const {
        return this->flags & Flags::CONSTANT_PREDICTION;
    }

    void set_constant_pred() {
        this->flags |= Flags::CONSTANT_PREDICTION;
    }

    /**
     * Indicates if the whb search order on insertion to pending table to the 
     * past. This allows searching for data in reverse in the write history 
     * buffer rather than waiting for it in the pending table
    */
    bool is_whb_search() const {
        return this->flags & Flags::WHB_SEARCH;
    }

    /**
     * Sets the whb search order on insertion to pending table to the past.
     * This allows searching for data in reverse in the write history buffer
     * rather than waiting for it in the pending table
    */
    void set_whb_search() {
        this->flags |= Flags::WHB_SEARCH;
    }

//! Define this flag if needed
#ifdef DIAGNOSTICS_MATCHING_PC
    bool has_matching_pcs() const {
        return this->flags & Flags::HAS_MATCHING_PC;
    }

    void set_matching_pcs() {
        this->flags |= Flags::HAS_MATCHING_PC;
    }

    void add_matching_pc(PC_t pc) {
        panic_if_not(has_matching_pcs());
        this->matchingPCs.push_back(pc);
    }

    const std::deque<PC_t> get_matching_pc() const {
        return this->matchingPCs;
    }
#endif // DIAGNOSTICS_MATCHING_PC

};

#endif // SHIFTLAB_CHUNK_INFO_H__
