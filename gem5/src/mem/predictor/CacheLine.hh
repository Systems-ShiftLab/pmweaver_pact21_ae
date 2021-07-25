#ifndef SHIFTLAB_CACHE_LINE_H__
#define SHIFTLAB_CACHE_LINE_H__

#include "mem/packet.hh"
#include "mem/cache/base.hh"
#include "mem/predictor/Declarations.hh"
#include "mem/predictor/ChunkInfo.hh"

class CacheLine {
private:
    /* Memory address of the cacheline */
    Addr_t addr;
    bool hasAddr = false;

    /* Holds the chunks of the cacheline */
    ChunkInfo dataChunks[DATA_CHUNK_COUNT];

    /** 
     * Returns the lower 32 bits of a uint64_t as uint32_t
     */
    static uint32_t get_lower_32_bits(const uint64_t num) {
        return static_cast<uint32_t>((num<<32)>>32);
    }

    /** 
     * Returns the upper 32 bits of a uint64_t as uint32_t 
     */
    static uint32_t get_higher_32_bits(const uint64_t num) {
        return static_cast<uint32_t>(num>>32);
    }

    /**
     * Holds the tick value when this cacheline was created
    */
    Tick timeOfCreation = 0;

    /**
     * Holds the time when one of the datachunks of this cacheline
     * was last updated
    */
    Tick timeOfLastUpdate = 0;
    bool hasTimeOfLastUpdate = false;

    bool isDirty = false;
public:
    friend std::ostream& operator<<(std::ostream& os, const CacheLine& dt);

    CacheLine() {
        this->timeOfCreation = curTick();
    };

    /* Create the cacheline from the cacheObj cache object*/
    CacheLine(Addr_t paddr, BaseCache *cacheObj) : CacheLine() {
        CacheBlk *cacheBlk = cacheObj->tags->findBlock(paddr, false);
        DataChunk *cacheData = nullptr;
        if (cacheBlk == nullptr or not cacheBlk->isValid()) {
            cacheData = new DataChunk[DATA_CHUNK_COUNT];
        } else {
            cacheData = (DataChunk*)cacheBlk->data;
        }
        
        this->addr = paddr;
        bool alignCacheline = false;
        int count = DATA_CHUNK_COUNT;

        this->hasAddr = true;

        volatile size_t offset = get_cacheline_off(paddr)/sizeof(DataChunk);

        /* Set the correct ChunkType of all the data chunks */
        for (int i = 0; i < DATA_CHUNK_COUNT; i++) {
            this->dataChunks[i].set_chunk_type(ChunkInfo::ChunkType::DATA);
        }

        for (int i = 0; i < count; i++) {
            auto index = i;
            if (alignCacheline) {
                index += offset;
            }
            panic_if(index > DATA_CHUNK_COUNT, "Check alignment");
            this->dataChunks[index].set_chunk_type(ChunkInfo::ChunkType::DATA);
            this->dataChunks[index].set_data(cacheData[i]);
        }
    }

    // ~CacheLine() { std::cout << "Destroying " << __FUNCTION__ << std::endl;};

    CacheLine(const Addr_t addr, const DataChunk dataChunks[DATA_CHUNK_COUNT], 
              const size_t count, const bool alignCacheline) : CacheLine() {
        panic_if(count > DATA_CHUNK_COUNT, "Cannot store data larger than a cache line");

        this->addr = addr;
        this->hasAddr = true;

        volatile size_t offset = get_cacheline_off(addr)/sizeof(DataChunk);

        /* Set the correct ChunkType of all the data chunks */
        for (int i = 0; i < DATA_CHUNK_COUNT; i++) {
            this->dataChunks[i].set_chunk_type(ChunkInfo::ChunkType::DATA);
        }

        for (int i = 0; i < count; i++) {
            auto index = i;
            if (alignCacheline) {
                index += offset;
            }
            panic_if(index > DATA_CHUNK_COUNT, "Check alignment");
            this->dataChunks[index].set_chunk_type(ChunkInfo::ChunkType::DATA);
            this->dataChunks[index].set_data(dataChunks[i]);
        }
    }

    CacheLine(const PacketPtr pkt, bool useVaddr = true) : CacheLine() {
        Addr_t addr = -1;
        
        if (useVaddr) {
            addr = pkt->req->getVaddr();
        } else {
            addr = pkt->req->getPaddr();
        }

        const size_t chunkCount = pkt->getSize()/sizeof(DataChunk);
        panic_if(chunkCount > DATA_CHUNK_COUNT, "Cannot store data larger than a cache line");

        const size_t offset = get_cacheline_off(addr)/sizeof(DataChunk);
        DataChunk *pktData = pkt->getPtr<DataChunk>();


        for (int i = 0; i < chunkCount; i++) {
            auto index = i+offset;
            panic_if(index > DATA_CHUNK_COUNT, "Check alignment");

            this->dataChunks[index].set_chunk_type(ChunkInfo::ChunkType::DATA);
            this->dataChunks[index].set_data(pktData[i]);
        }
    }

    ChunkInfo* get_datachunks() const {
        return (ChunkInfo*)dataChunks;
    }

    /** 
     * Can this cacheline predict the addr of a write? 
     * @param addr Address to find in the cacheline
     * @param reverseHalfWords Boolean for reversing the address before lookup 
     *        at half word granularity (32 bits)
     * @return Returns the offset of the address in the cache line, -1 if 
     *         address cannot be predicted using this cache line
    */
    int get_addr_offset(Addr_t addr, bool reverseHalfWords = true) const {
        // std::cout << "Trying to find the addr " << (void*)addr;
        // std::cout << " with cacheline" << *this << "\n";

        int result = -1;

        Addr_t alignedAddr = cacheline_align(addr);

        uint32_t addrLow  = get_lower_32_bits(alignedAddr);
        uint32_t addrHigh = get_higher_32_bits(alignedAddr);    
        
        if (reverseHalfWords) {
            addrLow = addrLow ^ addrHigh;
            addrHigh = addrLow ^ addrHigh;
            addrLow = addrLow ^ addrHigh;
        } 
        
        for (int i = 0; i < DATA_CHUNK_COUNT-1; i++) {
            bool isDataChunk = (dataChunks[i].get_chunk_type() 
                                    == ChunkInfo::ChunkType::DATA);
            bool isNextDataChunk = (dataChunks[i+1].get_chunk_type() 
                                    == ChunkInfo::ChunkType::DATA);
            if (isDataChunk 
                    and isNextDataChunk 
                    /* Only algin the element at ith position */
                    and ((uint32_t)cacheline_align(dataChunks[i].get_data()) == addrHigh) 
                    and (dataChunks[i+1].get_data() == addrLow)) {
                result = i;
                break;
            }
        }
        return result;
    }

    /** 
     * Can this cacheline predict the dataChunk of a write? return the offset if 
     * possible, -1 if it isn't.
    */
    int get_data_offset(DataChunk dataChunk) {
        int result = -1;

        for (int i = 0; i < DATA_CHUNK_COUNT; i++) {
            bool isDataChunk = (dataChunks[i].get_chunk_type() 
                                    == ChunkInfo::ChunkType::DATA);
            if (isDataChunk and this->dataChunks[i].get_data() == dataChunk) {
                result = i;
                break;
            }
        }
        return result;
    }

    /**
     * Returns true if all the dataChunks of this cache line have found a predictor.
     * False otherwise.
     */
    bool are_all_complete() {
        bool result = true;
        for (int i = 0; i < DATA_CHUNK_COUNT; i++) {
            result &= this->get_datachunks()[i].get_completion();
        }
        return result;
    }

    void set_addr(Addr_t addr) {
        this->addr = addr;
        this->hasAddr = true;
    }

    Addr_t get_addr() const {
        assert(hasAddr);
        return this->addr;
    }

    bool all_invalid() {
        bool allInvalid = true;
        for (int i = 0; i < DATA_CHUNK_COUNT; i++) {
            if (this->dataChunks[i].is_valid()) {
                allInvalid = false;
            }
        }
        return allInvalid;
    }

    /**
     * Returns the index of the first valid data chunk in the cache line,
     * (size_t)-1 if no such index exists
    */
    size_t find_first_valid_index() {
        size_t result = -1;
        
        for (int i = 0; i < DATA_CHUNK_COUNT; i++) {
            if (this->get_datachunks()[i].is_valid()) {
                result = i;
                break;
            }
        }

        return result;
    }

    void clear() {
        for (int i = 0; i < DATA_CHUNK_COUNT; i++) {
            this->get_datachunks()[i].clear();
        }
    }

    /**
     * @return the tick value for the generation of this cacheline,
     * time of generation is same as the maximum of the time of generation
     * of all its data chunks. Unlike time of creation, time of generation 
     * refers to the Tick latest tick value at which the data for one of its
     * chunk or addr was found.
    */
    Tick get_time_of_gen() {
        Tick result = 0;
        for (int i = 0; i < DATA_CHUNK_COUNT; i++) {
            if (this->dataChunks[i].is_valid() and this->dataChunks[i].has_time_of_gen()) {
                if (result < this->dataChunks[i].get_time_of_gen()) {
                    result = this->dataChunks[i].get_time_of_gen();
                }
            }
        }

        panic_if(result == 0, "Cannot calculate the time of generation");
        return result;
    }

    bool all_zeros() const {
        bool result = true;
        for (int i = 0; i < DATA_CHUNK_COUNT; i++) {
            if (this->dataChunks[i].is_valid() and this->dataChunks[i].get_data() != 0) {
                result = false;
                break;
            }
        }

        return result;
    }

    /**
     * @return The time this cacheline was initialized.
    */
    Tick get_time_of_creation() const {
        return this->timeOfCreation;
    }
    
    void set_time_of_creation(Tick timeOfCreation) {
        this->timeOfCreation = timeOfCreation;
    }

    Tick get_time_of_last_update() {
        panic_if_not(hasTimeOfLastUpdate);
        return this->timeOfLastUpdate;
    }
    
    void set_time_of_last_update(Tick timeOfLastUpdate)  {
        this->hasTimeOfLastUpdate = true;
        this->timeOfLastUpdate = timeOfLastUpdate;
    }

    /**
     * Overwrites `*this` cacheline with another cacheline, only the 
     * valid chunks from the source cacheline are used.
     * @param srcCacheline Source cacheline
     * @return Number of chunks overwritten in this object, chunks with 
     *         same value in the source and this cacheline are ignored
    */
    size_t overwriteFrom(const CacheLine srcCacheline) {
        ChunkInfo *srcDataChunks = srcCacheline.get_datachunks();
        ChunkInfo *destDataChunks = this->get_datachunks();

        size_t replaceCounter = 0;

        for (int i = 0; i < DATA_CHUNK_COUNT; i++) {
            if (srcDataChunks[i].is_valid()) {
                if (destDataChunks[i].is_invalid()
                        or destDataChunks[i].get_data() != srcDataChunks[i].get_data()) {
                    replaceCounter++;
                }
                destDataChunks[i] = srcDataChunks[i];
            }
        }
        return replaceCounter;
    }

    /**
     * Overwrites the destination cacheline with the data from `*this` cacheline
     * and replaces `*this` with the new destination cacheline. After the function
     * execution, both the cachelines hold the same cacheline.
     * @param destCacheline Cacheline whose data is overwritten, the destCacheline is
     *        also changed
     * @return Number of chunks overwritten in this object, chunks with 
     *         same value in the destination and this cacheline are ignored
    */
    size_t overwriteTo(CacheLine destCacheline) {
        // std::cout << "[Cacheline] Overwritting to " << destCacheline << std::endl;
        // std::cout << "[Cacheline] With " << *this << std::endl;

        ChunkInfo *destDataChunks = destCacheline.get_datachunks();
        ChunkInfo *srcDataChunks = this->get_datachunks();

        size_t replaceCounter = 0;

        for (int i = 0; i < DATA_CHUNK_COUNT; i++) {
            if (srcDataChunks[i].is_valid()) {
                if (destDataChunks[i].is_invalid()
                        or destDataChunks[i].get_data() != srcDataChunks[i].get_data()) {
                    replaceCounter++;
                }
                destDataChunks[i] = srcDataChunks[i];
            }
        }

        for (int i = 0; i < DATA_CHUNK_COUNT; i++) {
            this->get_datachunks()[i] = destCacheline.get_datachunks()[i];
        }
        
        // std::cout << "[Cacheline] This is now: " << *this << std::endl;

        return replaceCounter;
    }

    bool is_dirty() const {
        return this->isDirty;
    }

    void set_dirty() {
        this->isDirty = true;
    }

    void set_clean() {
        this->isDirty = false;
    }

    std::string to_string() const {
        std::stringstream result;
        result << *this;
        return result.str();
    };

    size_t invalid_chunk_count() const {
        size_t invalidChunkCount = 0;
        for (int i = 0; i < DATA_CHUNK_COUNT; i++) {
            if (this->dataChunks[i].is_invalid()) {
                invalidChunkCount++;
            }
        }
        return invalidChunkCount;
    }

    size_t valid_chunk_count() const {
        return DATA_CHUNK_COUNT - this->invalid_chunk_count();
    }
};

#endif // SHIFTLAB_CACHE_LINE_H__
