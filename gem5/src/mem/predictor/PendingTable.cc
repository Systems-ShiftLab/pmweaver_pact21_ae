#include "PendingTable.hh"
#include "debug/PendingTable.hh"
#include "debug/PredictorFrontendLogic.hh"
#include "mem/predictor/CacheLine.hh"
#include "mem/predictor/ChunkInfo.hh"
#include "mem/predictor/Declarations.hh"
#include "helper_suyash.h"  

PendingTable::PendingTable(std::string name, FixedSizeQueue<WriteHistoryBufferEntry> *whb) : 
    DataStore<U>(name), whb(whb) {
        pendingVolatilePCsSize
            .name(name + ".pendingVolatilePCsSize")
            .desc("Size of pending volatile PCs")
            .init(0,1000, 1000/10);
        SharedArea::init_size_multiplier();
        this->MAX_SIZE *= SharedArea::sizeMultiplier;
        this->DISABLE_WHB_SEARCH = get_env_val("DISABLE_WHB_SEARCH");
    }

bool 
PendingTable::add(U elem) {
    if (not elem.is_whb_search() or DISABLE_WHB_SEARCH) {
        DataStore::add(this->dummy);
        
        this->pendingVolatilePCsSize.sample(this->pendingVolatilePCs.size());

        this->pendingTable[elem.get_generating_pc()].push_back(elem);
        this->pendingVolatilePCs[elem.get_generating_pc()]++;

        if (std::find(this->insertionOrder.begin(),     
                    this->insertionOrder.end(),   
                    elem.get_generating_pc()) == this->insertionOrder.end()) {    
            this->insertionOrder.push_back(elem.get_generating_pc()); 
        }


        panic_if_not(this->pendingVolatilePCs[elem.get_generating_pc()] > 0);
        panic_if_not(elem.has_parent_index());

        DPRINTFR(PendingTable, RED 
                "Inserting elem with parent = %p, "
                "parentIndex = %d, "
                "chunkType = %d, "
                "dataFieldOffset = %d, "
                "pc = %p, "
                "new_value = %d, "
                "q.sz = %d\n" RST,
                (void*)elem.get_parent(), elem.get_parent_index(), 
                (int)elem.get_chunk_type(), elem.get_data_field_offset(), 
                elem.get_generating_pc(),
                this->pendingVolatilePCs[elem.get_generating_pc()], 
                this->pendingTable[elem.get_generating_pc()].size());
                
        /* If the table is at capacity, free the first entry */
        if (this->insertionOrder.size() == MAX_SIZE) {
            panic_if(this->insertionOrder.size() > MAX_SIZE, "Inconsistent size");
            auto entryToEvict = insertionOrder.front();
            insertionOrder.pop_front();
            this->pendingVolatilePCs.erase(entryToEvict);
            this->pendingTable.erase(entryToEvict);
        }
    } else {
        // std::cout << "Insert WHB search element for PC "    
        //           << (void*)elem.get_generating_pc() 
        //           << " index " << elem.get_parent_index()
        //           << std::endl;
        bool updated = false;
        for (auto whb_riter = this->whb->rbegin();   
                whb_riter != this->whb->rend(); 
                whb_riter++) {

            WriteHistoryBufferEntry *whb_riter_obj = whb_riter->get();
            size_t df_offset = whb_riter_obj->get_cacheline().find_first_valid_index()  
                                + elem.get_data_field_offset();
            if (whb_riter_obj->get_cacheline().get_datachunks()[df_offset].is_valid()) {
                CacheLine cl = whb_riter->get()->get_cacheline();

                if (cl.get_datachunks()[df_offset].get_chunk_type() == ChunkInfo::ChunkType::DATA) {
                    DataChunk data = cl.get_datachunks()[df_offset].get_data();

                    if (whb_riter_obj->get_pc() == elem.get_generating_pc()) {
                        if (elem.get_chunk_type() == ChunkInfo::ChunkType::DATA) {
                            ChunkInfo *dataChunks   
                                = elem.get_parent()->cacheline.get_datachunks();
                            size_t parentIndex = elem.get_parent_index();

                            dataChunks[parentIndex].set_data(data);
                            elem.get_parent()->dataComplete[parentIndex] = true;
                        } else if (elem.get_chunk_type() == ChunkInfo::ChunkType::ADDR) {
                            ChunkInfo &addrChunk = elem.get_parent()->addr;

                            addrChunk.set_target_addr(data);
                            elem.get_parent()->addrComplete = true;
                        }
                        DPRINTF(PredictorFrontendLogic, "[REV] PC %p\n", (void*)whb_riter_obj->get_pc());
                        updated = true;
                        break;
                    } 
                }
            }
        }

        if (not updated) {
            DPRINTF(PredictorFrontendLogic,     
                    "No entry found for updating reverse search for pc = %p\n", 
                    elem.get_generating_pc());
        }
    }
    return true;
}

std::deque<PendingTableEntryParent*>
PendingTable::update_entry_state(PC_t pc, const DataChunk *dataChunks, 
                                 size_t size) {
    /* Cannot call this function if an entry for this pc doesn't exists 
       in the pending table */
    panic_if(not this->has_pc_waiting(pc), 
             "%s called on a non-existent pc (=%p)", 
             __FUNCTION__, (void*)pc);

    DataStore::get();

    DPRINTF(PendingTable, 
            CYN "Updating pending table entry with pc = %p, current size = %lu"
            RST "\n", (void*)pc, this->get_size());

    auto &waitingEntries = this->pendingTable.at(pc);
    std::deque<PendingTableEntryParent*> completedParents;
    std::deque<size_t> completedPendingTableEntries;
    
    size_t iter = 0;
    
    for (auto waitingEntry : waitingEntries) {
        panic_if_not(waitingEntry.get_generating_pc() != 0);
        auto parent = waitingEntry.get_parent();
        auto dataFieldOffset = waitingEntry.get_data_field_offset();

        /* Size of the incoming write should always be greater than the data 
           field offset, but some instructions like FXSAVE have same PC value
           for different sized stores. */
        if (dataFieldOffset < size) {
            panic_if (parent->cacheline.all_invalid(), 
                    "All cachelines of the parent for this entry are invalid, "
                    "check insertion.");

            panic_if_not(waitingEntry.is_valid());

            auto dataFromWrite = dataChunks[dataFieldOffset];
            Tick timeOfWrite   = curTick();
            
            if (waitingEntry.get_chunk_type() == ChunkInfo::ChunkType::DATA) {
            
                auto parentIndex = waitingEntry.get_parent_index();
                parent->dataComplete[parentIndex] = true;

                parent->cacheline.get_datachunks()[parentIndex].set_chunk_type(ChunkInfo::ChunkType::DATA);
                parent->cacheline.get_datachunks()[parentIndex].set_data(dataFromWrite);
                parent->cacheline.get_datachunks()[parentIndex].set_time_of_gen(timeOfWrite);

                /* Erase the matched entry from the map and the filter */
                // waitingEntries.erase(waitingEntry++);
                this->pendingVolatilePCs[pc]--;

                if (this->pendingVolatilePCs[pc] == 0) {
                    this->pendingVolatilePCs.erase(pc);
                }

                completedPendingTableEntries.push_back(iter);

                panic_if(this->pendingVolatilePCs[pc] == -1, "Volatile write count violation with pc = %p", pc);
            } else if (waitingEntry.get_chunk_type() == ChunkInfo::ChunkType::ADDR) {
                parent->addrComplete = true;
                parent->addr.set_chunk_type(ChunkInfo::ChunkType::ADDR);
                
                panic_if_not(dataFieldOffset+1<size);
                Addr_t addrFromWriteHigh = (Addr_t)dataChunks[dataFieldOffset];
                Addr_t addrFromWriteLow = (Addr_t)dataChunks[dataFieldOffset+1];

                /* NOTE: Intentional reversal of address */
                Addr_t addrFromWrite = cacheline_align((addrFromWriteLow<<(sizeof(Addr_t)*8/2)) + addrFromWriteHigh);

                parent->addr.set_target_addr(addrFromWrite);
                parent->addr.set_time_of_gen(timeOfWrite);

                /* Erase the matched entry from the map and the filter */
                // waitingEntries.erase(waitingEntry++);
                this->pendingVolatilePCs.at(pc)--;
                completedPendingTableEntries.push_back(iter);
            } else {
                panic_if_not(0);
            }
            
            // std::cout << "Completeness = " << parent->allComplete() << std::endl;
            
            bool allComplete = parent->allComplete() == 17;
            
            if (allComplete) {
                completedParents.push_back(parent);
            }       
            iter++;
        }   
    }

    /* Remove completed entries from the pending table */
    while (not completedPendingTableEntries.empty()) {
        DataStore::remove();
        
        auto waitingEntriesIn = waitingEntries.begin() 
                              + completedPendingTableEntries.back();
        waitingEntries.erase(waitingEntriesIn);
        completedPendingTableEntries.pop_back();
    }
    return completedParents;
}

bool
PendingTable::has_pc_waiting(PC_t pc) const {
    return (this->pendingVolatilePCs.find(pc) != this->pendingVolatilePCs.end())
            && this->pendingVolatilePCs.find(pc)->second > 0;
}

bool 
PendingTable::contains(T key) {
    DataStore<U>::contains();
    return this->pendingTable.find(key) != this->pendingTable.end();
}

size_t 
PendingTable::get_size() {
    DataStore::get_size();
    return this->pendingTable.size();
}

U& 
PendingTable::get(T key) {
    fatal("Unimplemented");
    DataStore<U>::get();
    return pendingTable.at(key).front();
}

bool 
PendingTable::remove_elem(T key) {
    DataStore<U>::remove();
    this->pendingTable.erase(key);
    return true;
}

PendingTableEntryParent*
PendingTable::get_completed_parent(PC_t pc) {
    // DPRINTF(PendingTable, "Trying to access completed parent with pc = %p, current size = %lu\n", (void*)pc, this->get_size());
    
    bool pendingTableHasPC = this->pendingTable.find(pc) != this->pendingTable.end();
    PendingTableEntryParent *result = nullptr;
    if (pendingTableHasPC) {
        for (auto entry : pendingTable[pc]) {
            uint64_t completeness = entry.get_parent()->allComplete();
            if (completeness == 17) {
                result = entry.get_parent(); //this->pendingTable[pc].front().get_parent();
                    
                break;    
            }
        }
    }
    
    return result;
}
