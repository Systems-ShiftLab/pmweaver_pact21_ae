#include "Filter.hh"

template <typename T>
const size_t get_elem_size() {
    return sizeof(T);
}

template <typename T>
bool Filter::add(const T elem) {
    // TODO: Find a better way than sizeof
    if (CUCKOO_FILTER_OK == cuckoo_filter_add(cFilter, &elem, sizeof(elem))) {
	keyCount++;
	return true;
    } else {
	return false;
    }
}

template <typename T>
bool Filter::remove() {
    unimplemented__("Uimplemented")
}

template <typename T>
bool Filter::remove_elem(const T elem) {
    if (CUCKOO_FILTER_OK == cuckoo_filter_remove(cFilter, &elem, sizeof(elem))) {
	keyCount--;
	return true;
    } else {
	return false;
    }
}

template <typename T>
const T& Filter::get() const {
    unimplemented__("Uimplemented")
}

template <typename T>
const T& Filter::get_back() {
    return get(elem);
}

template <typename T>
const T& Filter::get_front() {
    return get(elem);
}

template <typename T>
bool add_back(const T elem) {
    return add(elem);
}

template <typename T>
bool add_front(const T elem) {
    return add(elem);
}

template <typename T>
bool remove_back() {
    return remove();
}

template <typename T>
bool remove_front() {
    return remove();
}

template <typename T>
size_t get_size() const {
    return keyCount;
}
