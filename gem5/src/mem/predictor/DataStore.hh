#ifndef SHIFTLAB_DATA_STORE_H__
#define SHIFTLAB_DATA_STORE_H__

#include <cstdlib>

#include "base/statistics.hh"

/* Template for how a cache implementation could be */
template <typename T>
class DataStore {
public:
    Stats::Scalar *totalInsertions;
    Stats::Scalar *totalDeletions;
    Stats::Scalar *totalAccess;
private:
protected:
    T dummy;
    bool statsEnabled = false;
    std::string name_ds;
public:
    DataStore(std::string parentName) {
        if (parentName != "") {
            this->totalInsertions = new Stats::Scalar;
            this->totalDeletions = new Stats::Scalar;
            this->totalAccess = new Stats::Scalar;

            (*totalInsertions)
                .name(parentName + ".totalInsertions")
                .desc(parentName + ".totalInsertions");
            (*totalDeletions)
                .name(parentName + ".totalDeletions")
                .desc(parentName + ".totalDeletions");
            (*totalAccess)
                .name(parentName + ".totalAccess")
                .desc(parentName + ".totalAccess");
            statsEnabled = true;
            this->name_ds = parentName;
        }

        
    }
    virtual ~DataStore() {
        // delete this->totalInsertions;
        // delete this->totalDeletions;
        // delete this->totalAccess;
    }

    virtual bool remove_elem(const T elem) {
        if (statsEnabled) {
            totalDeletions->operator++();
        }
        return true;
    }

    virtual T& get() {
        if (statsEnabled) {
            totalAccess->operator++();
        }
        return dummy;
    }

    virtual bool add_back(const T elem) {
        if (statsEnabled) {
            totalInsertions->operator++();
        }
        return true;
    }

    virtual bool add_front(const T elem) {
        if (statsEnabled) {
            totalInsertions->operator++();
        }
        return true;
    }

    virtual bool add(const T elem) {
        if (statsEnabled) {
            totalInsertions->operator++();
        }
        return true;
    }

    virtual bool remove_back() {
        if (statsEnabled) {
            totalDeletions->operator++();
        }
        return true;
    }

    virtual bool remove_front() {
        if (statsEnabled) {
            totalDeletions->operator++();
        }
        return true;
    }

    virtual bool remove() {
        if (statsEnabled) {
            totalDeletions->operator++();
        }
        return true;
    };
    
    virtual T& get_back() {
        if (statsEnabled) {
            totalAccess->operator++();
        }
        return dummy;
    }
    virtual T& get_front() {
        if (statsEnabled) {
            totalAccess->operator++();
        }
        return dummy;
    }

    virtual size_t get_size() {
        return true;
    }

    virtual bool contains() {
        return true;
    }

};

#endif // SHIFTLAB_DATA_STORE_H__
