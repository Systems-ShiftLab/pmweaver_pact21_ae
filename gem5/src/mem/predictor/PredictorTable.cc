#include "base/trace.hh"
#include "debug/IHB.hh"
#include "debug/PCFilter.hh"
#include "debug/PredictorTable.hh"
#include "mem/predictor/Constants.hh"
#include "mem/predictor/Declarations.hh"
#include "mem/predictor/LastFoundKeyEntry.hh"
#include "mem/predictor/PredictorTable.hh"
#include "mem/predictor/SharedArea.hh"
#include "mem/predictor_backend.hh"

#define ALL_SUYASH__
#include "helper_suyash.h"
#undef ALL_SUYASH__

hash_t 
PredictorTable::getEvictionIndex() const {
    hash_t result = 0;
    if (not disableConfidence) {
        for (auto entry : this->predictorTable) {
            if (entry.second.addrConf() <= LOW_CONF_TRESH) {
                result = entry.first;
            }
        }
    } else {
        for (auto entry : this->predictorTable) {
            if (entry.second.get_age(this->currentOrder) 
                    > STALE_ENTRY_AGE_THRESHOLD) {
                result = entry.first;
            }
        }
    }
    return result;
}

bool 
PredictorTable::add(PredictorTableEntry elem) {
    panic_if_not(elem.has_orig_cacheline());
    DataStore::add(elem);

    this->tick();

    /* Set the insertion order of the element */
    elem.set_order(this->currentOrder);

    /* Increment the current order */
    this->currentOrder++;

    if (this->predictorTable.find(elem.get_hash()) != this->predictorTable.end()) {
        PredictorTableEntry &potentialReplacement 
                = this->predictorTable.at(elem.get_hash());
        bool shouldReplace = false;

        Confidence addrConf = potentialReplacement.addrConf;
        Confidence dataConf = potentialReplacement.dataConf;
        if (addrConf() <= LOW_CONF_TRESH or dataConf() <= LOW_CONF_TRESH) {
            shouldReplace = true;
        }

        /** 
         * Replace only if the existing entry has low confidence else decrease
         * the confidence of the existing entry 
        */
        if (shouldReplace) {
            this->entryReplacementCounter++;
            this->predictorTable.insert(std::make_pair(elem.get_hash(), elem));
        } else {
            this->droppedAdditions++;
            potentialReplacement.dataConf.sub(1);

            /* Update the entry to handle constant 0 prediction */
            this->update_entry_for_0_pred(elem);
        }
    } else {
        /* The table is at capacity */
        if (this->size + 1 > MAX_SIZE) {
            hash_t indexToEvict = this->getEvictionIndex();
            if (indexToEvict != 0) {
                this->capacityEvictions++;
                this->predictorTable.erase(indexToEvict);
            }
        }

        this->predictorTable.insert(std::make_pair(elem.get_hash(), elem));

    }
    panic_if_not(this->size <= MAX_SIZE);
    return true;
}

bool 
PredictorTable::contains(hash_t pc) {
    DataStore<PredictorTableEntry>::contains();
    return this->predictorTable.find(pc) != this->predictorTable.end();
}

size_t 
PredictorTable::get_size() {
    DataStore::get_size();
    return this->predictorTable.size();
}

PredictorTableEntry& 
PredictorTable::get(hash_t pc) {
    DataStore<PredictorTableEntry>::get();
    return predictorTable.at(pc);
}

bool 
PredictorTable::remove_elem(hash_t pc) {
    DataStore<PredictorTableEntry>::remove();
    this->predictorTable.erase(pc);
    return true;
} 

std::ostream& 
operator<<(std::ostream& os, const SimpleFixedSizeQueue<IHB_Entry>& dt) {
    os << "<";
    for (int i = 0; i < dt.get_size(); i++) {
        os << (void*)dt.get_const(i).get_pc();
        if (i != dt.get_size() - 1) {
            os << ", ";
        }
    }
    os << ">";
}

template <typename T>
T
rotl (T v, unsigned int b)
{
  static_assert(std::is_integral<T>::value, "rotate of non-integral type");
  static_assert(! std::is_signed<T>::value, "rotate of signed type");
  constexpr unsigned int num_bits {std::numeric_limits<T>::digits};
  static_assert(0 == (num_bits & (num_bits - 1)), "rotate value bit length not power of two");
  constexpr unsigned int count_mask {num_bits - 1};
  const unsigned int mb {b & count_mask};
  using promoted_type = typename std::common_type<int, T>::type;
  using unsigned_promoted_type = typename std::make_unsigned<promoted_type>::type;
  return ((unsigned_promoted_type{v} << mb)
          | (unsigned_promoted_type{v} >> (-mb & count_mask)));
}

hash_t 
PredictorTable::get_path_hash() const {
    hash_t result = 0ul;

    // auto mask_upper_32_bits = [](hash_t hash) { return (hash << 32) >> 32; };

    for (int i = 0; i < this->pathHistory->get_size(); i++) {
        result = result ^ (this->pathHistory->get(i) << i);
    }
    return result;
}

PredictorTableEntry
PredictorTable::get_with_hash(hash_t hash) {
    // std::cout << " Trying to get hash " << std::endl;
    PredictorTableEntry result;
    bool found = false;
    for (auto entry :  this->predictorTable) {
        if (entry.second.get_hash() == hash) {
            result = entry.second;
            found = true;
        }
    }
    panic_if(not found, "Unable to find any match for hash %p", hash);
    return result;
}


bool 
PredictorTable::update_ihb(PacketPtr pkt) {
    /* Check the shared area for updates from the backend */
    this->check_shared_area();
    // std::cout << "Updating ihb" << std::endl;

    panic_if(not pkt->isWrite(), "");
    bool result = false;

    PC_t pc = pkt->req->getPC();
    

    /* If the incoming packet is smaller than 4 bytes, pad it */
    DataChunk *paddedData = new DataChunk;
    bool useCopiedPtr = false;
    if (pkt->getSize() < 4) {
        useCopiedPtr = true;
        switch (pkt->getSize()) {
        case 1:
            *paddedData = (DataChunk)(*pkt->getPtr<uint8_t>());
            break;
        case 2:
            *paddedData = (DataChunk)(*pkt->getPtr<uint16_t>());
            break;
        }
    }

    auto ptrToConstr = useCopiedPtr ? paddedData : pkt->getPtr<DataChunk>();

    /* Padded data is always of 4 bytes (== 1 chunk in current implementation) */
    auto chunkCount = useCopiedPtr 
                        ? 4/sizeof(DataChunk) 
                        : pkt->getSize()/sizeof(DataChunk);

    this->pathHistory->push_back(pc);
    // std::cout << "PC added, new hash = " << this->get_path_hash() << std::endl;
    this->lastFoundHashes.clear();

    /* Overwrite everything for the path based */
    if (this->has_hash(this->get_path_hash())) {
        // std::cout << GRN << "Found hash " << this->get_path_hash() << RST << std::endl;
        result = true;
        this->lastFoundHashes.push_back(this->get_path_hash());
    } else {
        // std::cout << RED << "Did not find hash " << this->get_path_hash() << RST << std::endl;
    }

    return result;
}

void 
PredictorTable::check_shared_area() {
    for (auto correctPrediction : SharedArea::correctPredictions) {
        int correctAddrPred = correctPrediction.second.first;
        int correctDataPred = correctPrediction.second.second;

        for (int i = 0; i < correctAddrPred; i++) {
            bool wasDataPredicted = (correctDataPred - i) >= 0;
            this->notify_correct_prediction(
                correctPrediction.first, true, wasDataPredicted
            );
            
            if (wasDataPredicted) {
                this->correctPredictionCounter++;
            }
        }
    }
    SharedArea::correctPredictions.clear();
}

void 
PredictorTable::tick() {
    /* cleanup if the clock overflowed */
    if ((this->clock % CLOCK_PERIOD) == 0) {
        // this->cleanup_low_conf_entries();
        this->cleanup_stale_entries();
        predictorTableTicks++;
    }
    this->clock++;
}

void 
PredictorTable::cleanup_stale_entries() {
    std::deque<hash_t> deletionQ;
    std::deque<Age_t> ages;
    /* Find items that are stale */
    // std::cout << __FUNCTION__ << ": currentorder " << std::dec << currentOrder << std::endl;
    for (auto pt_iter : *this) {
        PredictorBackend::initConf(pt_iter.first);
        // std::cout << "Current order " << currentOrder 
        //           << " age = " << pt_iter.second.get_age(currentOrder) 
        //           << " conf = " << PredictorBackend::confidenceTable[pt_iter.first]
        //           << " hash = " << (void*)pt_iter.first
        //           << " PRED_CONFIDENCE_MAX = " << PRED_CONFIDENCE_MAX
        //           << " pt_iter.second.get_age(currentOrder) = " << (pt_iter.second.get_age(currentOrder))
        //           << " STALE_ENTRY_AGE_THRESHOLD = " << (STALE_ENTRY_AGE_THRESHOLD)
        //           << " pt_iter.second.get_age(currentOrder) > STALE_ENTRY_AGE_THRESHOLD = " << (pt_iter.second.get_age(currentOrder) > STALE_ENTRY_AGE_THRESHOLD)
        //           << " PredictorBackend::confidenceTable[pt_iter.first] < PRED_CONFIDENCE_MAX = " << (PredictorBackend::confidenceTable[pt_iter.first] < PRED_CONFIDENCE_MAX)
        //           << std::endl;
        if ((pt_iter.second.get_age(currentOrder) > STALE_ENTRY_AGE_THRESHOLD
                and PredictorBackend::confidenceTable[pt_iter.first] < PRED_CONFIDENCE_MAX)) {
            // std::cout << "deleting entry " << (void*)pt_iter.first << std::endl;
            deletionQ.push_back(pt_iter.first);
            ages.push_back(pt_iter.second.get_age(currentOrder));
        }
    }

    for (auto deletePCSig : deletionQ) {
        // std::cout << "Deleting entry " << deletePCSig << std::endl;
        this->remove_elem(deletePCSig);
        ages.pop_front();
        staleEntryDeletionCounter++;
    }

}

// void 
// PredictorTable::cleanup_low_conf_entries() {
//     std::deque<PCSig> entriesToRemove;

//     for (auto entry : this->predictorTable) {
//         uint8_t conf = PredictorBackend::getConfidenceForLoc(entry.first);
//         if (conf <= PRED_CONF_THRESHOLD and conf != PRED_CONF_INVALID) {
//             entriesToRemove.push_back(entry.first);
//         }
//     }

//     for (auto entryToRemove : entriesToRemove) {
//         ++lowConfidenceEvictionCounter;
//         auto index = this->predictorTable.find(entryToRemove);
//         this->predictorTable.erase(index);
//     }
// }

void 
PredictorTable::dump() {
    return;
    // std::string dump_path = "/ramdisk/predictor_table_dump" 
    //                              + std::to_string(dumpId);
    // std::ofstream dumpFile;
    // dumpFile.open(dump_path);
    // for (auto predictorEntry : *this) {
    //     dumpFile 
    //         << "<" << print_ptr(16) << predictorEntry.first.first
    //         << ", "  << print_ptr(16) << predictorEntry.first.second
    //         << "> [" << print_ptr(16) 
    //         << predictorEntry.second.get_addr_chunk().get_generating_pc()
    //         << "]";
    //     dumpFile << predictorEntry.second << std::endl;
    // }
    // dumpId++;
}

bool 
PredictorTable::has_hash(hash_t hash) const {
    bool result = false;
    // std::cout << "Predictor table size = " << this->predictorTable.size() << std::endl;
    for (auto entry : this->predictorTable) {
        // std::cout << BLU << "hash: " << entry.second.get_hash() << std::endl;
        if (entry.second.get_hash() == hash) {
            result =  true;
            break;
        }
    }
    return result;
}
