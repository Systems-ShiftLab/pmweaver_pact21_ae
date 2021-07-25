#ifndef SHIFTLAB_MEM_PREDICTOR_SHARED_AREA_H__
#define SHIFTLAB_MEM_PREDICTOR_SHARED_AREA_H__

#include "mem/predictor/Common.hh"
#include "mem/predictor/Constants.hh"
#include "mem/predictor/Declarations.hh"

#include <unordered_map>

typedef size_t AddrCount;
typedef size_t DataCount;
typedef std::pair<AddrCount, DataCount> cp_entry;


/**
 * Variables shared across everything for easy access
*/
class SharedArea {
public:
    /**
     * Holds the information on correct predictions. This information is
     * added by the backend and read by each of the frontend.
    */
    static std::unordered_map<hash_t, cp_entry> correctPredictions;
    /**
     * Holds the confidence for individual PCs to avoid those that are
     * repeatedly predicting wrong values.
    */
    static std::unordered_map<PC_t, Confidence> genPCConf;
    /**
     * Unordered Map used for collecting statistics on unique PCs.
     * Potential use: Finding useful PCs for the frontend
    */
    static std::unordered_map<hash_t, bool> uniquePCSig;

    typedef struct constChunkLocator {
        size_t constOffset =- 1;    // Cacheline offset for locating the constant chunk
        size_t timesFound = 0;      // Number of times the chunk was found to hold constant 
                                    // value
        DataChunk lastData = -1;    // Last data that the backend saw
    };

    /**
     * Table that keeps track of the PC signature that have chunks which can be 
     * potentially be constant value fields. 
     * */                             
    static std::unordered_map<hash_t,
                              std::unordered_map<size_t, constChunkLocator>    // = <offset, constChunkLocator>
                              > constPredTracker;

    static Addr mmap_persistent_start;
    static Addr mmap_persistent_end;

    /**
     * Size multiplier for all the tables in the predictor, used for sensitivtiy analysis
    */
    static float sizeMultiplier;
    static void init_size_multiplier();

    /* Holds the match count for different statistics from the backend */
    static std::vector<size_t> backendIhbPatternMatchIndex;

}; // class SharedArea
#endif // SHIFTLAB_MEM_PREDICTOR_SHARED_AREA_H__