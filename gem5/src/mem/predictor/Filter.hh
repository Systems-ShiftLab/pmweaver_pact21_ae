#ifndef SHIFTLAB_FILTER_H__
#define SHIFTLAB_FILTER_H__

#include "DataStore.hh"
#include "libcuckoofilter/include/cuckoo_filter.h"

#include <cassert>

/**
 * Filter implementation using cuckoo filter, works only for elements with
 * compile time size information  
 */
template <typename T>
class Filter : DataStore<T> {
private:
    cuckoo_filter_t *cFilter;
    size_t maxKeyCount;
    static const int MAX_KICK_ATTEMPTS = 100;
    size_t keyCount = 0;
public:
    Filter(size_t maxKeyCount) : maxKeyCount(maxKeyCount) {
	uint32_t seed = (uint32_t) (time(NULL) & 0xffffffff);
	bool result =
	    cuckoo_filter_new(&cFilter, maxKeyCount, MAX_KICK_ATTEMPTS, seed);
	assert(result == CUCKOO_FILTER_OK && "Unable to create filter");
    }
  
    ~Filter() {
	cuckoo_filter_free(cFilter);
    }

    const size_t get_elem_size() override;

    bool add(const T elem) override;
    bool remove_elem(const T elem) override;
    
    size_t get_size() const override;
    
protected:
  
};

#endif // SHIFTLAB_FILTER_H__
