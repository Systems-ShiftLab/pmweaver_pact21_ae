#ifndef SHIFTLAB_SIMPLE_FIXED_SIZE_QUEUE_H__
#define SHIFTLAB_SIMPLE_FIXED_SIZE_QUEUE_H__

template <class T>
class SimpleFixedSizeQueue {
private:
    size_t sz;
    std::deque<T> queue;

public:

    /**
     * Parent constructor is called automatically.
    */
    SimpleFixedSizeQueue(size_t size) : sz(size) {
        // this->queue.resize(size);
    }

    SimpleFixedSizeQueue(size_t size, T initValue) : sz(size) {
        this->queue.resize(size);
        std::fill(this->queue.begin(), this->queue.end(), initValue);
    }

    bool push_back(T elem) {
        assert(this->queue.size() <= this->sz);
        if (this->queue.size() == this->sz) {
            this->queue.pop_front();
        }

        this->queue.push_back(elem);
        return true;
    }

    T &back() {
        return this->queue.back();
    }

    T &get() {
        return this->queue.front();
    }

    T get(size_t index) const {
        return this->queue.at(index);
    }

    T get_const(size_t index) const {
        return this->queue.at(index);
    }

    size_t get_size() const {
        assert(this->queue.size() <= this->sz);
        return this->queue.size();
    }

    size_t get_max_size() const {
        return this->sz;
    }

    bool remove() {
        assert(this->queue.size() > 0);
        this->queue.pop_front();
        return true;
    }

    void clear() {
        while (this->get_size()) {
            // std::cout << "size: " << this->queue.size() << " " << this->get_size() << "\n";
            this->remove();
        }
    }

    /* For C++ range based loops */
    typename std::deque<T>::iterator begin() { return this->queue.begin(); }
    typename std::deque<T>::iterator end() { return this->queue.end(); }

    
    friend std::ostream& operator<<(std::ostream& os, const SimpleFixedSizeQueue& dt);
};

#endif // SHIFTLAB_SIMPLE_FIXED_SIZE_QUEUE_H__