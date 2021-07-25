#ifndef SHIFTLAB_PREDICTOR_TABLE_H__
#define SHIFTLAB_PREDICTOR_TABLE_H__

#include "debug/PredictorFrontendLogic.hh"
#include "mem/packet.hh"
#include "mem/predictor/Common.hh"
#include "mem/predictor/DataStore.hh"
#include "mem/predictor/Declarations.hh"
#include "mem/predictor/FixedSizeQueue.hh"
#include "mem/predictor/LastFoundKeyEntry.hh"
#include "mem/predictor/SharedArea.hh"
#include "mem/predictor/SimpleFixedSizeQueue.hh"

#define ALL_SUYASH__
#include "helper_suyash.h"
#undef ALL_SUYASH__


#include <cstdint>
#include <string>
#include <cstring>
#include <deque>
#include <unordered_map>
#include <vector>


#include "mem/predictor/CacheLine.hh"
#include "mem/predictor/ChunkInfo.hh"
#include "mem/predictor/Constants.hh"
#include "mem/predictor/Declarations.hh"
#include "mem/predictor/PCQueue.hh"

class PredictorTableEntry {
public:
    static const int DATA_T_SIZE = 64/sizeof(ChunkInfo::Content::data);

    /* Diagnostics information */
    Addr_t destAddr_diag;
private:
    ChunkInfo addrChunk;
    ChunkInfo dataChunks[DATA_T_SIZE];
    PCSig pcSig = PCSig({0, 0, 0});
    bool used = false;

    /* Stores the order of this entry relative to all 
       other parents entries to determine the entry's age */
    Age_t insertionOrder = 0;
    bool hasAge = false;

    const std::string CORRECT_ADDR_PREDICTION_AGE_REWARD_STR = "CORRECT_ADDR_PREDICTION_AGE_REWARD";
    const std::string CORRECT_DATA_PREDICTION_AGE_REWARD_STR = "CORRECT_DATA_PREDICTION_AGE_REWARD";

    Age_t CORRECT_ADDR_PREDICTION_AGE_REWARD;
    Age_t CORRECT_DATA_PREDICTION_AGE_REWARD;

    uint32_t CONF_MAX = 7;
    uint32_t CONF_MIN = 0;
    uint32_t CONF_INIT = 6;

    CacheLine originalCacheLine;
    bool hasOrigCL = false;

    bool addrOnlyPrediction = false;
    
    hash_t hash;
    bool hasHash;


    //! If you add anything here updated the copy for that new field down in the function for
    //! for operator=
    //! NOTE: Fix this!
public:
    Confidence addrConf = Confidence(CONF_INIT, CONF_MAX, CONF_MIN);
    Confidence dataConf = Confidence(CONF_INIT, CONF_MAX, CONF_MIN);

    PCSig get_pc() const  { return this->pcSig; }
    void set_pc(PCSig pcSig) { this->pcSig = pcSig; }

    ChunkInfo* get_datachunks() { return this->dataChunks; } 
    void set_datachunks(ChunkInfo *dataChunks, size_t count) { 
        assert(count <= DATA_CHUNK_COUNT);
        /* Copy the data and clear rest of the dataChunks*/
        std::memcpy(this->dataChunks, dataChunks, count*sizeof(ChunkInfo));
        std::memset(this->dataChunks+count, 0, (DATA_T_SIZE-count)*sizeof(ChunkInfo));
    } 

    ChunkInfo &get_addr_chunk() {
        return this->addrChunk;
    }

    void set_addr_chunk(ChunkInfo addrChunk) {
        assert(addrChunk.get_chunk_type() == ChunkInfo::ChunkType::ADDR);
        this->addrChunk = addrChunk;
    }

    PredictorTableEntry(ChunkInfo *dataChunks) : PredictorTableEntry() {
        for (int i = 0; i < DATA_T_SIZE; i++) {
            this->dataChunks[i] = dataChunks[i];
        }
    }
    PredictorTableEntry() {
        CORRECT_ADDR_PREDICTION_AGE_REWARD = std::stoul(get_env_str(CORRECT_ADDR_PREDICTION_AGE_REWARD_STR, "20"));
        CORRECT_DATA_PREDICTION_AGE_REWARD = std::stoul(get_env_str(CORRECT_DATA_PREDICTION_AGE_REWARD_STR, "80"));
    }

    // PredictorTableEntry(const PredictorTableEntry &pte) {
    //     this->destAddr_diag = pte.destAddr_diag;
    //     this->addrChunk = pte.addrChunk;
    //     for (int i = 0; i < DATA_CHUNK_COUNT; i++) {
    //         this->dataChunks[i] = pte.dataChunks[i];
    //     }
    //     this->pcSig = pte.pcSig;
    //     this->used = pte.used;
    //     this->insertionOrder = pte.insertionOrder;
    //     this->hasAge = pte.hasAge;
    // }

    PredictorTableEntry &operator=(const PredictorTableEntry &pte) {
        this->destAddr_diag = pte.destAddr_diag;
        this->addrChunk = pte.addrChunk;
        for (int i = 0; i < DATA_CHUNK_COUNT; i++) {
            this->dataChunks[i] = pte.dataChunks[i];
        }
        this->pcSig = pte.pcSig;
        this->used = pte.used;
        this->insertionOrder = pte.insertionOrder;
        this->hasAge = pte.hasAge;
        this->originalCacheLine = pte.originalCacheLine;
        this->hasOrigCL = pte.hasOrigCL;
        this->hash = pte.hash;
        this->hasHash = pte.hasHash;
        return *this;
    }

    bool is_used() const {
        return this->used;
    }

    void use() {
        panic_if(is_used(), "Entry already used");
        this->used = false;
    }

    
    void set_order(Age_t order) {
        this->hasAge = true;
        this->insertionOrder = order;
    }

    Age_t get_age(Age_t currentOrder) const {
        panic_if_not(this->hasAge);
        panic_if(currentOrder < this->insertionOrder, 
                    "Current order cannot be smaller than the insertion "
                    "order of an element.");
        Age_t age = currentOrder - this->insertionOrder;
        return age;
    }

    /**
     * Decrease the age of this entry since it was correctly predicted
    */
    void notify_confidence(bool addrPrediction, bool dataPrediction) {
        // std::cout << "Got notification for " << addrPrediction << " " << dataPrediction << std::endl;
        // std::cout << "Original conf = " << addrConf() << " " << dataConf() << std::endl;
        if (addrPrediction) {
            this->addrConf.add(1);
        } else {
            this->addrConf.sub(1);
        }

        if (dataPrediction) {
            this->dataConf.add(1);
        } else {
            this->dataConf.sub(1);
        }
        // std::cout << "New conf = " << addrConf() << " " << dataConf() << std::endl;
    }

    /** 
     * Decrease the age of this entry since it was correctly predicted 
    */ 
    void notify_correct_prediction(bool wasAddrPredicted, bool wasDataPredicted) { 
        Age_t save = this->insertionOrder;
        if (wasAddrPredicted) {
            this->insertionOrder -= CORRECT_ADDR_PREDICTION_AGE_REWARD; 
        }
        if (wasDataPredicted) {
            this->insertionOrder -= CORRECT_DATA_PREDICTION_AGE_REWARD; 
        }
        // std::cout << CYN << "Changed Age from " << save << " to " << this->insertionOrder << RST << std::endl;
    }

    void set_original_cacheline(CacheLine originalCacheLine) {
        this->hasOrigCL = true;
        this->originalCacheLine = originalCacheLine;
    }

    CacheLine get_original_cacheline() const {
        panic_if_not(this->hasOrigCL);
        return this->originalCacheLine;
    }

    bool has_orig_cacheline() const {
        return this->hasOrigCL;
    }

    void set_addr_only_pred() {
        this->addrOnlyPrediction = true;
    }

    bool is_addr_only_pred() const {
        return this->addrOnlyPrediction;
    }

    /**
     * Returns a cacheline representing the generating PCs of this entry
     * @return A cacheline with all the data chunks' value as the generating PC
    */
    CacheLine gen_pc_as_cl() const {
        CacheLine result;
        for (int i = 0; i < DATA_CHUNK_COUNT; i++) {
            ChunkInfo srcDataChunk = this->dataChunks[i];
            ChunkInfo &tgtDataChunk = result.get_datachunks()[i];
            if (srcDataChunk.is_valid()) {
                tgtDataChunk.set_chunk_type(ChunkInfo::ChunkType::DATA);
                tgtDataChunk.set_data(srcDataChunk.get_generating_pc());
            }
        }
        result.set_addr(this->addrChunk.get_generating_pc());
        return result;
    }

    /**
     * Mark the Chunks for address and data for searching the whb when 
     * this prediction would be triggered
    */
    void mark_pred_for_whb_search(PCQueue pcQueue) {
        // std::cout << "Marking chunks for whb search" << std::endl;
        Tick earliestTick = UINT64_MAX;

        /* Find the latest tick in first 3 found PCs from the pcQueue */
        for (int i = 0; i < 3 and i < pcQueue.get_size(); i++) {
            Tick tick = pcQueue.queue_n.at(i).tick;
            // std::cout << "Got tick = " << tick << " for PC " << (void*)pcQueue.queue_n.at(i).pc << std::endl;
            if (earliestTick > tick) {
                earliestTick = tick;
            }
        }
        // std::cout << "Earliest tick = " << earliestTick << std::endl;

        /* Set the whb search order for the data chunks */
        for (int i = 0; i < DATA_CHUNK_COUNT; i++) {
            if (this->dataChunks[i].is_valid()) {
                using pcQueueEntry_t = PCQueue::pcQueueEntry_t;
                auto MatchEntry 
                    = [&](pcQueueEntry_t entry) { 
                        return entry.pc 
                            == this->dataChunks[i].get_generating_pc();
                };
                bool inPCQueue = (std::find_if(pcQueue.queue_n.begin(), 
                                    pcQueue.queue_n.begin()+3,    
                                    MatchEntry)) 
                                != pcQueue.queue_n.begin() + 3;

                bool wasFoundBefore = this->dataChunks[i].get_gen_pc_in_tick() < earliestTick;
                bool notInPCQueue = not inPCQueue;

                DPRINTF(PredictorFrontendLogic, 
                        "Checking data chunk at index = %2d, inPCQueue = %d, tick = %08d"
                        " wasFoundBefore = %d, notInPCQueue = %d, PC = %08p\n",
                        i, inPCQueue, this->dataChunks[i].get_gen_pc_in_tick(),
                        wasFoundBefore, notInPCQueue, (void*)this->dataChunks[i].get_generating_pc());

                if (wasFoundBefore and notInPCQueue) {
                    this->dataChunks[i].set_whb_search();
                }
            }
        }

        /* Set the whb search order for address chunks */
        if (this->addrChunk.is_valid()
                and this->addrChunk.get_gen_pc_in_tick() < earliestTick) {
            this->addrChunk.set_whb_search();
        }
    }
    
    bool has_hash() const {
        return this->hasHash;
    }

    hash_t get_hash() const {
        panic_if_not(has_hash());
        return this->hash;
    }

    void set_hash(hash_t hash) {
        this->hash = hash;
        this->hasHash = true;
    }

    friend std::ostream& operator<<(std::ostream& os, PredictorTableEntry& pte);
};

class IHB_Entry {
protected:
    bool valid = false;
    PC_t pc;
    DataChunk *dataChunks;
    size_t count;
public:
    IHB_Entry()
            : valid(false), pc(-1) {
        this->dataChunks = nullptr;
    }

    IHB_Entry(PC_t pc, const DataChunk *dataChunks, size_t count)
            : valid(true), pc(pc), count(count) {
        this->dataChunks = (DataChunk*)malloc(sizeof(DataChunk)*count);

        /* Copy the data */
        for (int i = 0; i < count; i++) {
            this->dataChunks[i] = dataChunks[i];
        }
    }

    ~IHB_Entry() {
        if (this->dataChunks != nullptr) {
            // free(dataChunks);
        }
    }

    PC_t get_pc(const char *str = __builtin_FUNCTION(), const int line = __builtin_LINE()) const {
        panic_if(this->dataChunks == nullptr, "No datachunk found, called from %s:%d", str, line);
        return this->pc;
    }

    DataChunk *get_data_chunks() {
        panic_if(this->dataChunks == nullptr, "No datachunk found");
        return this->dataChunks;
    }

    bool is_valid() const {
        return valid;
    }

    size_t get_count() {
        panic_if(this->dataChunks == nullptr, "No datachunk found");
        return this->count;
    }

    friend std::ostream& operator<<(std::ostream& os, const IHB_Entry& ihb_i);

};


class PredictorTable : DataStore<PredictorTableEntry> {
private:
    /* Table size */
    size_t size = 0;
    size_t MAX_SIZE = 32;
    
    std::unordered_map<hash_t, PredictorTableEntry> predictorTable;
    std::unordered_map<PC_t, size_t> pcFilter;  
    std::unordered_map<PC_t, std::deque<PCSig>> pcFilterMap;

    size_t dumpId = 0;

    /* Holds the keys that was last found using IHB */
    std::vector<LastFoundKeyEntry> lastFoundKeys;
    std::vector<hash_t> lastFoundHashes;
    bool isLastKeyValid = false;

    /** 
     * Holds the count of the elemnts in the pendingTable to calculate
     * the insertionOrder and age of the pendingTableEntires
     */
    Age_t currentOrder = 0;

    /**
     * Represents the number of insertions after which an entry would 
     * be considered to be an stale entry. Entry marked as stale can be 
     * removed at any point thereafter.
     */
    std::string STALE_ENTRY_AGE_THRESHOLD_STR = "STALE_ENTRY_AGE_THRESHOLD";
    std::string IHB_PATTERN_MATCH_THRESH_STR = "IHB_PATTERN_MATCH_THRESH";
    std::string IHB_PATTERN_MIN_COUNT_STR = "IHB_PATTERN_MIN_COUNT";
    
    Age_t STALE_ENTRY_AGE_THRESHOLD = 200;

    /**
     * Number of insertions the predictor table will wait before triggering
     * the next scheduled events.
     */
    static const Tick CLOCK_PERIOD = 2;

    static const size_t LOW_CONF_TRESH = 1;
    static const size_t HIGH_CONF_TRESH = 2;

    /** Specifies the accuracy percentage for IHB pattern below which the 
     * pattern will be shutdown*/
    size_t IHB_PATTERN_MATCH_THRESH;

    /** Minimum number of predictions before which the threshold is used */
    size_t IHB_PATTERN_MIN_COUNT;

    /**
     * Clock used by the predictor table for scheduling tasks like cleaning
     * old entries.
     */
    Tick clock;

protected:
    Stats::Scalar lowConfidenceEvictionCounter;
    Stats::Scalar entryReplacementCounter;
    Stats::Scalar ihbMatchCounter;
    Stats::Scalar pcFilterHits;
    Stats::Scalar staleEntryDeletionCounter;
    Stats::Scalar predictorTableTicks;
    Stats::Scalar correctPredictionCounter;
    Stats::Scalar skippedLowConfEntries;
    Stats::Scalar notifications;
    Stats::Scalar droppedAdditions;
    Stats::Scalar capacityEvictions;
    Stats::Scalar const0PredUpdates;
    Stats::Scalar const0PredDeletions;
    Stats::Distribution pcFilterSize;
    Stats::Scalar sizeStat;
    Stats::Distribution ihbPatternMatchId;
    std::vector<size_t> ihbPatternMatchIdVec;

    void cleanup_low_conf_entries();

    /**
     * Removes entries from the predictor table that are very old, decided
     * using the insertion order of the entry.
     */
    void cleanup_stale_entries();
    SimpleFixedSizeQueue<IHB_Entry> *indexHistoryBuffer;
    bool disableConfidence = true;

    /**
     * Calls different function for cleaning up stale and low confidence entries
     */
    void tick();

    void check_shared_area();

    bool const0PredEnabled = false;

    const std::string ENABLE_CONST_0_PREDICTION_STR = "ENABLE_CONST_0_PREDICTION";
    const std::string PATH_HISTORY_SIZE_STR = "PATH_HISTORY_SIZE";
    size_t PATH_HISTORY_SIZE = 4;
public:
    SimpleFixedSizeQueue<PC_t> *pathHistory;

    PCSig lastCompleteEntry;
    PredictorTable(std::string name) 
            : DataStore<PredictorTableEntry>(name) {
                indexHistoryBuffer = new SimpleFixedSizeQueue<IHB_Entry>(IHB_SIZE);
        lowConfidenceEvictionCounter
            .name(name + ".lowConfidenceEvictionCounter")
            .desc("Total number of entries with low confidence that were "
                  "evicted from the predictor table. Threshold is " 
                  + std::to_string(PRED_CONF_THRESHOLD));
        entryReplacementCounter
            .name(name + ".entryReplacementCounter")
            .desc("Number of entries replaced in the predictor table." 
                  + std::to_string(PRED_CONF_THRESHOLD));
        ihbMatchCounter
            .name(name + ".ihbMatchCounter")
            .desc("Counts the number of times the index history buffer "
                  "had a match");
        pcFilterHits
            .name(name + ".pcFilterHits")
            .desc("Counts the number of times the pc filter was hit.");
        staleEntryDeletionCounter
            .name(name + ".staleEntryDeletionCounter")
            .desc("Counts the number of stal entries removed from the predictor"
                  " table.");
        predictorTableTicks
            .name(name + ".predictorTableTicks")
            .desc("Counts the number of times the scheduled event was called.");
        correctPredictionCounter
            .name(name + ".correctPredictionCounter")
            .desc("Number of times a correct prediction notification from the"
                  " backend decreased the age.");
        skippedLowConfEntries
            .name(name + ".skippedLowConfEntries")
            .desc("Predictions skipped due to low prediction confidence.");
        notifications
            .name(name + ".notifications")
            .desc("Total notifications received from the backend through the shared area");
        droppedAdditions
            .name(name + ".droppedAdditions")
            .desc("Number of insertions that were dropped from the predictor table due to "
                  "high conf of existing entry");
        capacityEvictions
            .name(name + ".capacityEvictions")
            .desc("Entries replaced due to limited capacity of predictor table.");
        const0PredUpdates
            .name(name  + ".const0PredUpdates")
            .desc("Counts the number of times the predictor table updated the constant 0"
                  " prediction field for it's field.");
        const0PredDeletions
            .name(name  + ".const0PredDeletions")
            .desc("Number of datachunks that had their const 0 prediction unset");
        pcFilterSize
            .name(name + ".pcFilterSize")
            .desc("Size of pc filter")
            .init(0, 2000, 2000/10);
        sizeStat
            .name(name + ".sizeStat")
            .desc("Size of the table");
        ihbPatternMatchId
            .name(name + ".ihbPatternMatchId")
            .init(0, 10, 1)
            .desc("ihbPatternMatchId");

        PATH_HISTORY_SIZE = std::stoul(get_env_str(PATH_HISTORY_SIZE_STR, "32"));
        pathHistory = new SimpleFixedSizeQueue<PC_t>(PATH_HISTORY_SIZE);

        STALE_ENTRY_AGE_THRESHOLD = std::stoul(get_env_str(STALE_ENTRY_AGE_THRESHOLD_STR, "200"));
        IHB_PATTERN_MATCH_THRESH = std::stoul(get_env_str(IHB_PATTERN_MATCH_THRESH_STR, "10"));
        IHB_PATTERN_MIN_COUNT = std::stoul(get_env_str(IHB_PATTERN_MIN_COUNT_STR, "50"));

        const0PredEnabled = get_env_val(ENABLE_CONST_0_PREDICTION_STR);
        disableConfidence = get_env_val("DISABLE_CONFIDENCE");

        std::cout << "Disable confidence = " << disableConfidence << std::endl;

        SharedArea::init_size_multiplier();
        std::cout <<  "Using size mult = " << SharedArea::sizeMultiplier << std::endl;
        this->MAX_SIZE *= SharedArea::sizeMultiplier;
        this->sizeStat = this->MAX_SIZE;
        std::cout << RED << "========\n\n\n"    
                  << "Predictor table size = " 
                  << this->MAX_SIZE 
                  << RST << "\n\n\n========\n";
        ihbPatternMatchIdVec = std::vector<size_t>(10);
    }
    ~PredictorTable() {
        // delete indexHistoryBuffer;
    }

    /* Adds an entry to the PredictorTable, no checks are performed. */
    bool add(PredictorTableEntry elem) override;
    
    /* Returns true if the given pc signature exists in the predictor table. */
    bool contains(hash_t pc);

    bool remove() override { unimplemented__("") };
    bool remove_elem(const PredictorTableEntry entry) override { unimplemented__("") };
    bool remove_elem(hash_t pc);

    /* Returns the XOR'd value of all the PCs in the path history */
    hash_t get_path_hash() const;

    PredictorTableEntry& get() override { unimplemented__(""); }
    PredictorTableEntry get_with_hash(hash_t hash);

    PredictorTableEntry& get(hash_t pc);
    
    bool add_back(const PredictorTableEntry elem)  override { return this->add(elem); };
    bool add_front(const PredictorTableEntry elem) override { return this->add(elem); };
    
    bool remove_back()  override { return this-> remove(); };
    bool remove_front() override { return this-> remove(); };
    
    PredictorTableEntry& get_back()  override { unimplemented__(""); };
    PredictorTableEntry& get_front() override { unimplemented__(""); };

    size_t get_size() override;

    /** 
     * Returns true if this request completes the search for one of the entry 
     * in the index history buffer (IHB).
     */
    bool update_ihb(PacketPtr pkt);

    void addEntryToPendingTable(PacketPtr pkt);

    bool is_last_key_valid() const {
        return this->isLastKeyValid;
    }

    std::vector<LastFoundKeyEntry> get_last_found_keys() {
        return this->lastFoundKeys;
    }

    std::vector<hash_t> get_last_found_hashes() {
        return this->lastFoundHashes;
    }

    std::unordered_map<hash_t, PredictorTableEntry>::iterator begin() {
        return this->predictorTable.begin();
    }

    std::unordered_map<hash_t, PredictorTableEntry>::iterator end() {
        return this->predictorTable.end();
    }

    void dump();

    hash_t getEvictionIndex() const;
    
    /**
     * 
     */
    void notify_correct_prediction(hash_t hash, bool addrPrediction, bool dataPrediction) {
        notifications++;
        if (this->predictorTable.find(hash) != this->predictorTable.end()) {
            this->predictorTable[hash].notify_confidence(addrPrediction, dataPrediction);
        }

        if (this->predictorTable.find(hash) != this->predictorTable.end()) { 
            this->predictorTable[hash].notify_correct_prediction(addrPrediction, dataPrediction); 
        }
    }

    void update_entry_for_0_pred(PredictorTableEntry elem) {
        if (const0PredEnabled) {
            update_entry_for_0_pred_handler(elem);
        }
    }

    /**
     * Updates the entry at the pcsig of the element for constant 0 predictions
    */
    void update_entry_for_0_pred_handler(PredictorTableEntry elem) {
        hash_t hash = elem.get_hash();

        panic_if(this->predictorTable.find(hash) == this->predictorTable.end(), 
                "%s called with a non existent pc signature", __FUNCTION__);

        auto &targetEntry = this->predictorTable.at(hash);
        
        auto targetDataChunks = targetEntry.get_datachunks();
        auto sourceDataChunks = elem.get_datachunks();

        /* set any chunk that repeats the zero value as the constant zero prediction */
        for (int i = 0; i < DATA_CHUNK_COUNT; i++) {
            bool targetValid = targetDataChunks[i].is_valid();
            bool sourceValid = sourceDataChunks[i].is_valid();

            if (targetValid and sourceValid) {
                panic_if(targetDataChunks[i].get_chunk_type() != ChunkInfo::ChunkType::DATA, 
                    "target's data chunk at location %d is not of data type but is valid (type = %d)", 
                    i, (int)targetDataChunks[i].get_chunk_type());

                panic_if(sourceDataChunks[i].get_chunk_type() != ChunkInfo::ChunkType::DATA, 
                    "source's data chunk at location %d is not of data type but is valid (type = %d)", 
                    i, (int)sourceDataChunks[i].get_chunk_type());

                bool chunksEqual = targetDataChunks[i].get_data() 
                                        == sourceDataChunks[i].get_data();
                bool targetChunkZero = targetDataChunks[i].get_data() == 0;
                
                if (chunksEqual and targetChunkZero) {
                    this->const0PredUpdates++;
                    targetDataChunks[i].set_const_0_pred();
                }
            }
        }

        /* unset any chunk that repeats the zero value as the constant zero prediction */
        for (int i = 0; i < DATA_CHUNK_COUNT; i++) {
            bool targetValid = targetDataChunks[i].is_valid();
            bool sourceValid = sourceDataChunks[i].is_valid();

            if (targetValid and sourceValid) {
                panic_if(targetDataChunks[i].get_chunk_type() != ChunkInfo::ChunkType::DATA, 
                    "target's data chunk at location %d is not of data type but is valid (type = %d)", 
                    i, (int)targetDataChunks[i].get_chunk_type());

                panic_if(sourceDataChunks[i].get_chunk_type() != ChunkInfo::ChunkType::DATA, 
                    "source's data chunk at location %d is not of data type but is valid (type = %d)", 
                    i, (int)sourceDataChunks[i].get_chunk_type());

                bool chunksEqual = targetDataChunks[i].get_data() 
                                        == sourceDataChunks[i].get_data();
                bool targetChunkZero = targetDataChunks[i].get_data() == 0;
                
                if (not chunksEqual and targetDataChunks[i].is_const_0_pred()) {
                    this->const0PredDeletions++;
                    targetDataChunks[i].unset_const_0_pred();
                }
            }
        }
    }

    bool isPCInPCFilter(PC_t pc) const {
        return this->pcFilter.find(pc) != this->pcFilter.end();
    }

    bool has_hash(hash_t hash) const;
};

/* NOTE: Check FixedSizeQueue.cc for instantiation of Class specific versions of FizedSizeQueue<class> */

#endif // SHIFTLAB_PREDICTOR_TABLE_H__
