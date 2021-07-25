#ifndef SHIFTLAB_MEM_PREDICTOR_NEXT_ADD_PREDICTOR_H__
#define SHIFTLAB_MEM_PREDICTOR_NEXT_ADD_PREDICTOR_H__

#include "mem/predictor/Constants.hh"
#include "mem/predictor/Declarations.hh"
#include "mem/predictor/SimpleFixedSizeQueue.hh"

/** Δ-Address Predictor
 * @brief A simple address *delta predictor*, prediction is based on the
 * last 'TRACKING_SIZE' addresses.
*/
class AddrPredictor {
private:
    /**
     * Last seen address
    */
    Addr_t lastSeenAddr = 0;

    /**
     * Number of last address differences to keep track of
    */
    const int TRACKING_SIZE = 4;

    /**
     * Holds the address difference of the last addresses
    */
    SimpleFixedSizeQueue<int64_t> addrDiff = SimpleFixedSizeQueue<int64_t>(TRACKING_SIZE);;
public:
    void set_last_seen_addr(Addr_t addr) {
        // std::cout << "Adding to address difference queue" << std::endl;
        // std::cout << "Last seen address = " << (void*)this->lastSeenAddr << " current address  = " << (void*)addr << std::endl;
        this->addrDiff.push_back(addr - this->lastSeenAddr);
        this->lastSeenAddr = addr;
        // std::cout << "KDifference = " << this->addrDiff.back() << std::endl;
        // std::cout << "Size of the queue = " << this->addrDiff.get_size() << " max size = " << this->addrDiff.get_max_size() << std::endl;
    }

    /**
     * Checks if this predictor can predict an address
     * @return Boolean value indicating if the predictor can predictor
     *         the address
    */
    bool can_pred_addr() const {
        bool result = true;
        // std::cout << "Queue size = " << addrDiff.get_size() << std::endl;
        for (size_t i = 0; i < addrDiff.get_size() - 1; i++) {
            // std::cout << "size_t i = " << i << std::endl;
            if (addrDiff.get(i) != addrDiff.get(i+1)) {
                result = false;
                break;
            }
        }

        if (not this->addr_q_size() > 1) {
            result = false;
        }

        // std::cout << "Returning prediction bool = " << result << std::endl;
        return result;
    }

    /**
     * @return The size of the internal address queue
    */
    bool addr_q_size() const {
        return this->addrDiff.get_size();
    }

    /**
     * Predict the address of the next p-write if can_pred_addr()
     * returns true else returns 0
     * @return Predicted address value
    */
    Addr_t predict_addr() {
        Addr_t result = 0;
        if (can_pred_addr()) {
            // std::cout << "Last seen address = " 
            //           << print_ptr(16) 
            //           << this->lastSeenAddr 
            //           << " addrDiff = " 
            //           << print_ptr(16)
            //           << this->addrDiff.back()
            //           << std::endl;

            result = this->lastSeenAddr + this->addrDiff.back(); 
        }
        // std::cout << "Generating prediction value " << print_ptr(16) << result << std::endl;
        return result;
    }

    /**
     * Converts the addr predictor object to std::string
     * @param predict bool value to call the predict function
     * @return std::string representation of the object
    */
    std::string state_to_string(bool predict) {
        std::stringstream result;
        result << "Last address = " << print_ptr(16) << this->lastSeenAddr
               << " addrDiff = " << (void*)this->addrDiff.back() 
               << " TRACKING_SIZE = " << this->TRACKING_SIZE; 
        if (predict) {
            result << " Predicted value = " << print_ptr(16) << this->predict_addr();
        }
        return result.str();
    }
};

#endif // SHIFTLAB_MEM_PREDICTOR_NEXT_ADD_PREDICTOR_H__