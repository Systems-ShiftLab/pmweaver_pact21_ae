#ifndef SHIFTLAB_FIXED_SIZE_QUEUE_H__
#define SHIFTLAB_FIXED_SIZE_QUEUE_H__

#include "DataStore.hh"
#include "helper_suyash.h"

#include "mem/predictor/Declarations.hh"

#include <cassert>
#include <deque>
#include <unordered_map>

#include <fstream>

/**
 * ....   ....   ....   elem   elem   elem   elem   ....   ....   ....
 *                /\                          /\
 *             back_ptr                   front_ptr
 */
template <class T>
class FixedSizeQueue : private DataStore<T> {
public:
    using T_ptr = std::unique_ptr<T>;
private:
    size_t sz;
    std::deque<T_ptr> queue;
    
    /**
     *  By default the queue would destroy objects that have to be evicted for inserting new ones 
     */
    bool manageMem = true; 

    void manageObj(T *obj) {
        if (manageMem) {
            delete obj;
        }
    }

    /* Diagnostics info */
    size_t dump_id;
public: 
    /**
     * Parent constructor is called automatically.
    */
    FixedSizeQueue(std::string parentName, size_t size) : DataStore<T>(parentName), sz(size) {
        // this->queue.resize(size);
    }
    FixedSizeQueue(std::string parentName, size_t size, T initValue) : DataStore<T>(parentName), sz(size) {
        // this->queue.resize(size);
    }
    FixedSizeQueue(std::string parentName) : DataStore<T>(parentName) { unimplemented__(""); }
    ~FixedSizeQueue() {
        while (this->queue.size()) {
            // T *rawPtr = this->queue.front().release();
            // manageObj(rawPtr);
        }
    }
    bool push_back(T *elem) {
        DataStore<T>::add_back(*elem);
        
        if (this->queue.size() == this->sz) {
            // T *rawPtr = this->queue.front().release();
            // manageObj(rawPtr);
            this->queue.pop_front();
            DataStore<T>::remove();
        }

        T_ptr ptr(elem);
        this->queue.push_back(std::move(ptr));
        return true;
    }

    T &get_front() override {
        return *this->queue.front();
    }

    T &get() {
        DataStore<T>::get();
        return *this->queue.front();
    }

    T &get(size_t index) {
        panic_if(index > this->get_size(), 
                "index %d for %s exceeds size %d", 
                index, this->name_ds, this->get_size());
        return *this->queue.at(index);
    }

    size_t get_size() override {
        assert(this->queue.size() <= this->sz);
        return this->queue.size();
    }

    size_t get_max_size() const {
        return this->sz;
    }

    bool remove() override {
        DataStore<T>::remove();
        // T *rawPtr = this->queue.front().release();
        // manageObj(rawPtr);
        this->queue.pop_front();
        return true;
    }

    bool is_mem_managed() {
        return this->manageMem;
    }

    void set_manage_mem(bool manageMem) {
        this->manageMem = manageMem;
    }

    /* For C++ range based loops */
    typename std::deque<T_ptr>::iterator  
    begin()  { return this->queue.begin();    }
    
    typename std::deque<T_ptr>::iterator  
    end()    { return this->queue.end();      }
    
    typename std::deque<T_ptr>::reverse_iterator  
    rbegin() { return this->queue.rbegin();   }
    
    typename std::deque<T_ptr>::reverse_iterator  
    rend()   { return this->queue.rend();     }

    void dump() {
        // std::cout << "Dump id = " << dump_id << std::endl;
        // return;
        std::string dump_path = "/ramdisk/dump_" + std::to_string(dump_id);
        std::ofstream dumpFile;
        dumpFile.open(dump_path);

        int id = 0;
        for (auto &whb_iter : *this) {
            dumpFile << print_ptr(5) << whb_iter->get_pc()
                     << " : " << whb_iter->get_cacheline()
                     << " : " << (void*)whb_iter->get_gen_tick() << std::endl;
        }
        dumpFile.close();
        this->dump_id++;
    }
};
// #error

#endif // SHIFTLAB_FIXED_SIZE_QUEUE_H__