#ifndef SHIFTLAB_DECLARATIONS_H__
#define SHIFTLAB_DECLARATIONS_H__

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>           
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>


#include "base/trace.hh"
#include "debug/ChunkInfo.hh"
#include "helper_suyash.h"

#include <type_traits>  // std::underlying_type
#include <utility>      // std::enable_if

/* PC Signature */
typedef uint64_t PC_t;

typedef uint64_t Addr_t;

/* Age can be negative */
typedef int64_t Age_t;

typedef uint64_t hash_t;

typedef std::vector<PC_t> PCSig;
const size_t PC_SIG_SIZE = 3;

#define print_ptr(width)                                                    \
    "0x" << std::hex << std::setw(width) << std::setfill('0')               \
    
template <typename T>
std::string vec2str(const std::vector<T> vec) {
    std::stringstream result;
    result << "<";
    for (int i = 0; i <  vec.size(); i++) {
        result << vec.at(i);
        if (i == vec.size() - 1) {
            result << ">";
        } else {
            result << ",";
        }
    }
    return result.str();
}

template <typename T>
std::string vec2hexStr(const std::vector<T> vec) {
    std::stringstream result;
    result << "<";
    for (int i = 0; i <  vec.size(); i++) {
        result << std::hex << "0x" << vec.at(i);
        if (i != vec.size() - 1){
            result << ",";
        }
    }
    result << std::dec << ">";
    return result.str();
}
    

/* For hashing std::pair */
struct hash_pair { 
    template <class T1, class T2> 
    size_t operator()(const std::pair<T1, T2>& p) const { 
        auto hash1 = std::hash<T1>{}(p.first); 
        auto hash2 = std::hash<T2>{}(p.second); 
        return hash1 ^ hash2; 
    }
};

/* For hashing std::pair */
struct hash_vector { 
    template <class T> 
    size_t operator()(const std::vector<T>& p) const { 
        auto hash1 = std::hash<T>{}(p[0]); 
        auto hash2 = std::hash<T>{}(p[1]); 
        auto hash3 = std::hash<T>{}(p[2]); 
        return hash1 ^ hash2 ^ hash3; 
    }
}; 

const size_t CACHELINE_BITS = 6UL; // 64 Bytes
const Addr_t CACHELINE_MASK = (1UL<<CACHELINE_BITS)-1UL; // 0b111111

inline Addr_t cacheline_align(Addr_t addr) {
    return (addr>>CACHELINE_BITS)<<CACHELINE_BITS;
}

inline Addr_t get_cacheline_off(Addr_t addr) {
    return addr & CACHELINE_MASK;
}

template<typename Key_t, typename Value_t>
void init_or_incr(std::unordered_map<Key_t, Value_t> &map, Key_t index, Value_t initValue, Value_t step) {
    if (map.find(index) == map.end()) {
        map[index] = initValue;
    } else {
        map[index]+=step;
    }
}

typedef uint32_t DataChunk;


const int CACHELINE_SIZE = 64; // bytes
const size_t DATA_CHUNK_COUNT = CACHELINE_SIZE/sizeof(DataChunk);

class Confidence {
private:
    int64_t init;
    int64_t conf;
    uint32_t max;
    uint32_t min;
    
    bool initialized = false;
    bool hasChanged = false;
public:
    Confidence() {}

    Confidence(uint64_t init, uint32_t max, uint32_t min) 
            : init(init), conf(init), max(max), min(min), initialized(true) {}

    uint32_t operator()() const;

    void add(int val) {
        this->hasChanged = true;
        panic_if_not(initialized);
        if (conf + val > max) {
            // std::cout << "Setting value to max" << std::endl;
            conf = max;
        } else if (conf + val < min) {
            // std::cout << "Setting value to min" << std::endl;
            conf = min;
        } else {
            // std::cout << "adding " << val << std::endl;
            // std::cout << "old val " << conf << std::endl;
            conf += val;
            // std::cout << "new val " << conf << std::endl;
        }
    }

    bool is_changed() const {
        return this->hasChanged;
    }

    void sub(int val) {
        panic_if_not(initialized);
        this->add(-val);
    }

};

#endif // SHIFTLAB_DECLARATIONS_H__