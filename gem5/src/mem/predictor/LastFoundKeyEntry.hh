#ifndef SHIFTLAB_MEM_PREDICTOR_LAST_FOUND_KEY_ENTRY__
#define SHIFTLAB_MEM_PREDICTOR_LAST_FOUND_KEY_ENTRY__

class IHB_Entry;

class LastFoundKeyEntry {
public:
    /* Orignal index in the key constructor */
    size_t index;
    std::vector<IHB_Entry> lastFoundKey;
    friend std::ostream& operator<<(std::ostream& os, const LastFoundKeyEntry& dt);
};

#endif // SHIFTLAB_MEM_PREDICTOR_LAST_FOUND_KEY_ENTRY__