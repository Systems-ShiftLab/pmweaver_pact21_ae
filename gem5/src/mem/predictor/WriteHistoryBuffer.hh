#ifndef SHIFTLAB_WRITE_HISTORY_BUFFER_H__
#define SHIFTLAB_WRITE_HISTORY_BUFFER_H__

#include "DataStore.hh"
#include "FixedSizeQueue.hh"
#include "Declarations.hh"
#include "mem/predictor/CacheLine.hh"

#define ALL_SUYASH__
#include "helper_suyash.h"
#undef ALL_SUYASH__

#include <cassert>
#include <cstdint>
#include <deque>

class WriteHistoryBufferEntry {
private:
    uint64_t id;
    PC_t pc;
    CacheLine cacheLine;
    Tick genTick;
    hash_t pathHash;
    size_t size;

    bool hasGenTick = false;
    bool isUsed = false;
    bool hasPathHash = false;
    bool hasSize = true;
public:
    /***********************************/
    /* Fields for diagnostics          */
    Addr_t destAddr_diag;
    Tick insertionTick_diag;
    /***********************************/

    friend std::ostream& operator<<(std::ostream& os, const WriteHistoryBufferEntry& dt);

    WriteHistoryBufferEntry()
        : pc(0UL), cacheLine() {}

    WriteHistoryBufferEntry(PC_t pc, CacheLine cacheLine)
        : pc(pc), cacheLine(cacheLine) {}

    WriteHistoryBufferEntry(PC_t pc, Addr_t addr, DataChunk *dataChunks, 
                            size_t dataChunkCount, hash_t hash)
        : pc(pc) {
            Addr_t addrOffset = get_cacheline_off(addr)/sizeof(DataChunk);
            Addr_t alignedAddr = cacheline_align(addr);

            CacheLine tempCacheline;

            for (int i = addrOffset; i < addrOffset+dataChunkCount; i++) {
                panic_if(i >= DATA_CHUNK_COUNT, "Buffer overflow");
                tempCacheline.get_datachunks()[i].set_chunk_type(ChunkInfo::ChunkType::DATA);
                tempCacheline.get_datachunks()[i].set_data(dataChunks[i-addrOffset]);
                tempCacheline.get_datachunks()[i].set_generating_pc(pc);
                tempCacheline.set_addr(alignedAddr);
            }

            this->set_cacheline(tempCacheline);

            this->pathHash = hash;
            this->hasPathHash = true;
        }

    void set_cacheline(CacheLine &cacheLine);
    CacheLine get_cacheline() const;

    ~WriteHistoryBufferEntry() {
        // Do nothing
        return;
    }

    void set_pc(PC_t pc);
    PC_t get_pc() const;

    void set_gen_tick(Tick tick);
    Tick get_gen_tick() const;

    void set_id(uint64_t id);
    uint64_t get_id() const;

    void use();
    bool is_used() const;

    hash_t get_path_hash() const;
    void set_path_hash(hash_t hash);

    size_t get_size() const;
    void set_size(size_t size);
};

#endif // SHIFTLAB_WRITE_HISTORY_BUFFER_H__
