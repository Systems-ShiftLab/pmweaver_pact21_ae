#ifndef SHIFTLAB_PC_QUEUE_H__
#define SHIFTLAB_PC_QUEUE_H__

#include "mem/predictor/Constants.hh"
#include "mem/predictor/DataStore.hh"
#include "mem/predictor/Declarations.hh"

#include <iomanip>
#include <queue>

#include "helper_suyash.h"

class PCQueue {
public:
    struct pcQueueEntry_t {
        PC_t pc;
        Tick tick;

        pcQueueEntry_t(PC_t pc, Tick tick) 
            : pc(pc), tick(tick) {}

        bool operator<(const pcQueueEntry_t &rhs) const {
            /* std::priority_queue::top() will return the smallest element */
            return tick > rhs.tick;
        }
        bool operator==(const pcQueueEntry_t &rhs) const {
            return (tick == rhs.tick);
        }
        struct hash_function { 
            size_t operator()(const pcQueueEntry_t& p) const { 
                /**
                 * Hash both the pc and the tick to avoid inserting duplicate 
                 * entries from the write history buffer
                */
                auto pcHash = std::hash<PC_t>{}(p.pc); 
                auto tickHash = std::hash<Tick>{}(p.tick); 
                return pcHash ^ tickHash;
            }
        };
    };
private:
    std::priority_queue<pcQueueEntry_t> queue;
    std::unordered_map<pcQueueEntry_t, bool, pcQueueEntry_t::hash_function> uniqPCs;
    pcQueueEntry_t lastPoppedEntry = pcQueueEntry_t(0,0);
public:
    std::deque<pcQueueEntry_t> queue_n;
    Tick get_oldest_entry() const {
        std::priority_queue<pcQueueEntry_t> q(this->queue);
        // dprintf(3, "Content of pcQueue:\n");
        
        Tick oldest_tick = UINT64_MAX;
        while (! q.empty() ) {
            pcQueueEntry_t top = q.top();
            if (top.tick < oldest_tick) {
                oldest_tick = top.tick;
            }
            q.pop();
        }
        return oldest_tick;
    }

    bool exists(const pcQueueEntry_t entry) const {
        bool result = false;
        for (pcQueueEntry_t potEntry : this->queue_n) {
            if (potEntry.pc == entry.pc) {
                result = true;
                break;
            }
        }
        return result;
    }

    size_t get_size() const {
        // return 0;
        return this->queue_n.size();
    }

    void push_back(pcQueueEntry_t entry) {
        this->queue_n.push_back(entry);
        return;
        // std::cout << "entering " << __FUNCTION__ << "()" << "\n";
        // std::cout << dbg_var(entry.tick) << "\n";
        // std::cout << dbg_var(this->queue.top().tick) << "\n";
        if (not this->queue.empty()) {
            panic_if(entry.tick < this->queue.top().tick, "Write history buffer iteration "
                        "order shoudn't allow a new insertion to have tick value smaller "
                        "thant he smallest entry had %lu, got %lu", this->queue.top().tick, entry.tick);
            // std::cout << "Checked for panic condition" << std::endl;
        }
        // dprintf(3, UMAG "Got a push request for entry with pc = %p and tick = %lu" RST "\n" , entry.pc, entry.tick);
        bool val = this->uniqPCs[entry];
        if (val == false) {
            this->queue.push(entry);
            this->uniqPCs[entry] = true;
        } 
        this->queue_n.push_back(entry);
        // std::cout << "exiting " << __FUNCTION__ << "()" << "\n";
    }

    void pop() {
        this->queue_n.pop_front();
        return;
        // dprintf(3, UGRN "Got a pop request" RST "\n");
        // std::cout << "entering " << __FUNCTION__ << "()" << "\n";
        // auto front = this->queue_n.front();
        // int entriesToRemove = 0;
        // auto iter = this->queue_n.begin();
        // if (this->queue_n.size() > 1) {
        //     while(*iter++ == front) {   
        //         entriesToRemove++;
        //     }
        // }

        // while (entriesToRemove --> 0) {
            // this->queue_n.pop_front();
        // }
        
        // if (this->lastPoppedEntry.pc != 0) {
        //     panic_if(this->lastPoppedEntry.tick > this->queue_n.front().tick, "another panic");
        // }
        // std::cout << "exiting " << __FUNCTION__ << "()" << "\n";
        // return;
        lastPoppedEntry = this->queue_n.front();
        // panic_if(this->get_size() == 0, "%s() called on zero sized structure", __FUNCTION__);

        auto topElem = this->queue.top();
        this->queue_n.pop_front();
        this->queue.pop();
        this->uniqPCs[topElem] = false;
    }

    void print() const {
        // std::priority_queue<pcQueueEntry_t> q(this->queue);
        // dprintf(3, "Content of pcQueue:\n");
        // while (! q.empty() ) {
        //     dprintf(3, "%p -> %lu ", q.top().pc, q.top().tick);
        //     q.pop_front();
        // }
    }

    pcQueueEntry_t top() const {
        return this->queue_n.front();   
        // return this->queue_n.front();
        // std::cout << "entering " << __FUNCTION__ << "()" << "\n";
        // auto q_iter = this->queue_n.begin();
        // while (q_iter != this->queue_n.end()) {
        //     if (q_iter+1 == this->queue_n.end()) {
        //             std::cout << "exit " << __FUNCTION__ << "()" << "\n";
        //         return *q_iter;
        //     } else {
        //         if (*q_iter == *(q_iter+1)) {
        //             q_iter++;
        //         } else {
        //             std::cout << "exit " << __FUNCTION__ << "()" << "\n";
        //             return *q_iter;
        //         }
        //     }
        // }
        // panic("akldnfkasdnflkandflk");

        // panic_if(this->queue_n.front().pc != this->queue.top().pc and this->queue_n.front().tick != this->queue.top().tick, "panic panic");
        // panic_if(this->get_oldest_entry() != this->queue.top().tick, "Queue logic error");
        // panic_if(this->get_size() == 0, "%s() called on zero sized structure", __FUNCTION__);

        auto retVal = this->queue.top();

        // dprintf(3, UCYN  "Got a top request, reutrning element with pc = %p and tick = %lu" RST "\n", retVal.pc, retVal.tick);

        print();

        return retVal;
    }   

    std::string to_string() const {
        std::stringstream result;

        result << "<";
        for (int i = 0; i < this->queue_n.size(); i++) {
            result << print_ptr(16) << this->queue_n.at(i).pc;
            if (i == this->queue_n.size() - 1) {
                result << ">";
            } else {
                result << ", ";
            }
        }
        return result.str();
    }

    std::vector<PC_t> top_n_pc(size_t n) {
        std::vector<PC_t> result(n);
        for (int i = 0; i < n; i++) {
            if (i < this->queue_n.size()) {
                std::cerr << "Trying to insert " << this->queue_n.at(i).pc << std::endl;
                result[i] = this->queue_n.at(i).pc;
            } else {
                result[i] = 0;
            }
        }
        return result;
    }

};

#endif // SHIFTLAB_PC_QUEUE_H__