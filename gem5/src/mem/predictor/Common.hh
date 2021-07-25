#ifndef SHIFTLAB_MEM_COMMON_H__
#define SHIFTLAB_MEM_COMMON_H__

#include "base/types.hh"
#include "base/logging.hh"
#include "mem/predictor/Constants.hh"
#include "mem/packet.hh"

inline bool is_paddr_pm(Addr paddr) {
    bool result = false;
    if ( paddr > 8UL*(1024UL)*(1024UL)*(1024UL) /*8 GiB*/) {
        result = true;
    }
    return result;
}

inline bool is_paddr_volatile(Addr paddr) {
    return not is_paddr_pm(paddr);
}

inline bool is_vaddr_pm(Addr vaddr) {
    bool result;
    if ((vaddr >= PMEM_MMAP_HINT) && (vaddr < (2*PMEM_MMAP_HINT))) {
        result = true;
    } else {
        result = false;
    }
    
    return result;
}

inline bool is_vaddr_clwb(const PacketPtr pkt) {
    bool result = false;
    result = pkt->req->isToPOC() and pkt->hasData() == false and pkt->getSize() == 1 and is_vaddr_pm(pkt->req->getVaddr());
    return result;
}

inline bool is_vaddr_volatile(Addr vaddr) {
    return not is_vaddr_pm(vaddr);
}

inline void set_flag(uint64_t &flags, uint64_t flag) {
    flags |= flag;
}

inline void unset_flag(uint64_t &flags, uint64_t flag) {
    flags &= (~flag);
}

inline bool is_flag_set(const uint64_t &flags, uint64_t flag) {
    bool result;
    result = flags & flag;
    return result;
}

inline bool get_env_val(std::string str) {
    char* val = std::getenv(str.c_str());
    bool result = false;
    if (val != nullptr) {
        std::string val_str = std::string(val);
        if (val_str != "0") {
            result = true;
        } else {
            result = false;
        }
    }
    return result;
}

inline std::string get_env_str(std::string str, std::string defaultVal) {
    bool exists = get_env_val(str);

    std::string result;
    if (exists) {
        char *val = std::getenv(str.c_str());
        panic_if(val == nullptr, "Nullptr foundn while checking for %s", str.c_str());

        result = std::string(val);
    } else {
        result = defaultVal;
    }
    
    return result;
}

inline float get_env_float(std::string str, float defaultVal) {
    bool exists = get_env_val(str);

    float result;
    if (exists) {
        char *val = std::getenv(str.c_str());
        panic_if(val == nullptr, "Nullptr foundn while checking for %s", str.c_str());

        result = std::stof(std::string(val));
    } else {
        result = defaultVal;
    }
    
    return result;
}

#define LPRINT                              \
    std::cerr << __FILE__ << ":"            \
              << __LINE__ << " @ "          \
            << __FUNCTION__                 \
            << "(...)" << std::endl;

#endif // SHIFTLAB_MEM_COMMON_H__