#ifndef SHIFTLAB_COMPLETED_WRITE_ENTRY_H__
#define SHIFTLAB_COMPLETED_WRITE_ENTRY_H__

#include <boost/stacktrace.hpp>

#include "mem/predictor/CacheLine.hh"
#include "mem/predictor/Common.hh"
#include "mem/predictor/Constants.hh"
#include "mem/predictor/Declarations.hh"
#include "base/statistics.hh"

class CompletedWriteEntry {
private:
    enum Flags : uint64_t {
        NONE                    = 0UL,
        ADDR                    = 1UL << 1,
        GEN_HASH                = 1UL << 2,
        TIME_OF_ADDR_GEN        = 1UL << 3,
        TIME_OF_DATA_GEN        = 1UL << 4,
        HAS_CACHE_LINE          = 1UL << 5,
        VERIFICATION_CACHE_HIT  = 1UL << 6,
        COUNTER_CACHE_HIT       = 1UL << 7,
        ORIGNAL_CACHELINE       = 1UL << 8,
        IHB_PATTERN_MATCH_INDEX = 1UL << 9,
        TIME_OF_CREATION        = 1UL << 10
    };
    
    uint64_t flags = 0UL;

    Addr_t addr;
    CacheLine cacheline;
    hash_t generatorHash;
    Tick timeOfAddrGen  = -1,
         timeOfDataGen  = -1,
         timeOfCreation = -1;

    size_t ihbPatternMatchIndex;

    /** 
     * Cacheline that generated the prediction entry for this prediction.
     * Useful for diagnostics .
     */
    CacheLine orignalCacheLine;

    /* Was the prediction entry used for prediction */
    bool used = false;
public:
    CompletedWriteEntry(): addr(0) {}

    CompletedWriteEntry(const Addr_t addr, const CacheLine cacheline, const hash_t generatorHash)
            : addr(addr), cacheline(cacheline), generatorHash(generatorHash) {
        set_flag(flags, Flags::ADDR);
        set_flag(flags, Flags::HAS_CACHE_LINE);
        set_flag(flags, Flags::GEN_HASH);
    }

    ~CompletedWriteEntry() {
        // std::cerr << "Destroying CompletedWriteEntry" << "\n";
    }

    uint64_t verificationCacheMisses;
    uint64_t counterCacheMisses;

    hash_t get_generator_hash() const {
        panic_if(not is_flag_set(flags, Flags::GEN_HASH), "");
        return this->generatorHash;
    }

    CacheLine &get_cacheline() {
        return this->cacheline;
    }

    bool has_generator_hash() {
        return is_flag_set(flags, Flags::GEN_HASH);
    }

    void set_addr(const Addr_t addr) {
        set_flag(flags, Flags::ADDR);
        this->addr = addr;
    }

    Addr_t get_addr() const {
        panic_if(not is_flag_set(flags, Flags::ADDR), "");
        return this->addr;
    }

    bool has_addr() const {
        return is_flag_set(flags, Flags::ADDR);
    }

    void use() {
        panic_if(this->used, "");
        this->used = true;
    }

    bool is_used() {
        return this->used;
    }

    Tick get_time_of_addr_gen() const {
        panic_if(not is_flag_set(flags, Flags::TIME_OF_ADDR_GEN), "");
        return this->timeOfAddrGen;
    }

    void set_time_of_addr_gen(Tick tick) {
        set_flag(flags, Flags::TIME_OF_ADDR_GEN);
        this->timeOfAddrGen = tick;
    }

    bool has_time_of_addr_gen() const {
        return is_flag_set(flags, Flags::TIME_OF_ADDR_GEN);
    }

    Tick get_time_of_data_gen() const {
        panic_if(not is_flag_set(flags, Flags::TIME_OF_DATA_GEN), "");
        return this->timeOfDataGen;
    }

    void set_time_of_data_gen(Tick tick) {
        set_flag(flags, Flags::TIME_OF_DATA_GEN);
        this->timeOfDataGen = tick;
    }

    bool has_time_of_data_gen() const {
        return is_flag_set(flags, Flags::TIME_OF_DATA_GEN);
    }

    Tick get_time_of_creation() const {
        panic_if(not is_flag_set(flags, Flags::TIME_OF_CREATION), "");
        return this->timeOfCreation;
    }

    void set_time_of_creation(Tick tick) {
        set_flag(flags, Flags::TIME_OF_CREATION);
        this->timeOfCreation = tick;
    }

    bool has_time_of_creation() const {
        return is_flag_set(flags, Flags::TIME_OF_CREATION);
    }

    Tick get_time_of_gen() const {
        const Tick addrGen = this->get_time_of_addr_gen();
        const Tick dataGen = this->get_time_of_data_gen();

        const Tick result = std::vector<Tick>({addrGen, dataGen})[addrGen < dataGen];
        return result;
    }

    Tick get_least_time_of_gen() const {
        const Tick addrGen = this->get_time_of_addr_gen();
        const Tick dataGen = this->get_time_of_data_gen();

        const Tick result = std::min(addrGen, dataGen);
        return result;
    }

    void set_counter_cache_hit(bool cacheHit) {
        if (cacheHit) {
            set_flag(flags, Flags::COUNTER_CACHE_HIT);
        } else {
            unset_flag(flags, Flags::COUNTER_CACHE_HIT);
        }
    }

    void set_verification_cache_hit(bool cacheHit) {
        if (cacheHit) {
            set_flag(flags, Flags::VERIFICATION_CACHE_HIT);
        } else {
            unset_flag(flags, Flags::VERIFICATION_CACHE_HIT);
        }
    }

    bool was_counter_cache_hit() const {
        return is_flag_set(flags, Flags::COUNTER_CACHE_HIT);
    }

    bool was_verification_cache_hit() const {
        return is_flag_set(flags, Flags::VERIFICATION_CACHE_HIT);
    }

    void set_orig_cacheline(CacheLine orignalCacheLine) {
        set_flag(flags, Flags::ORIGNAL_CACHELINE);
        this->orignalCacheLine = orignalCacheLine;
    }

    CacheLine get_orig_cacheline() const {
        panic_if_not(is_flag_set(flags, Flags::ORIGNAL_CACHELINE));
        return this->orignalCacheLine;
    }

    void set_ihb_pattern_match_index(size_t index) {
        set_flag(flags, Flags::IHB_PATTERN_MATCH_INDEX);
        this->ihbPatternMatchIndex = index;
    }

    size_t get_ihb_pattern_match_index() const {
        panic_if_not(is_flag_set(flags, Flags::IHB_PATTERN_MATCH_INDEX));
        return this->ihbPatternMatchIndex;
    }
    
};

#endif // SHIFTLAB_COMPLETED_WRITE_ENTRY_H__