#include "mem/predictor/SharedArea.hh"


std::unordered_map<hash_t, cp_entry>                    SharedArea::correctPredictions;
std::unordered_map<PC_t, Confidence>                    SharedArea::genPCConf;
std::unordered_map<hash_t, bool>                        SharedArea::uniquePCSig;
std::vector<size_t>                                     SharedArea::backendIhbPatternMatchIndex = std::vector<size_t>(10);
std::unordered_map<hash_t, 
                   std::unordered_map<size_t, 
                   SharedArea::constChunkLocator>>      SharedArea::constPredTracker;

Addr SharedArea::mmap_persistent_start = 0;
Addr SharedArea::mmap_persistent_end = 0x20000000000ULL;
float SharedArea::sizeMultiplier = 1.0;

void SharedArea::init_size_multiplier() {
    SharedArea::sizeMultiplier = get_env_float("SIZE_MULTIPLIER", 1);
}