#include "WriteHistoryBuffer.hh"

#include "mem/predictor/CacheLine.hh"
#include "mem/predictor/ChunkInfo.hh"

#include <cassert>

/*--- Class: WriteHistoryBufferEntry ---*/

std::ostream& operator<<(std::ostream& out, const WriteHistoryBufferEntry& data) {
    out << "[0x" << std::hex << std::setw(16) << std::setfill('0') << data.get_pc() << std::dec << "]: Cacheline: " << data.get_cacheline();
    return out;
}

void WriteHistoryBufferEntry::set_cacheline(CacheLine &cacheLine) {
    this->cacheLine = cacheLine;
}

CacheLine WriteHistoryBufferEntry::get_cacheline() const {
    return this->cacheLine;
}

void WriteHistoryBufferEntry::set_pc(PC_t pc) {
    this->pc = pc;
}

PC_t WriteHistoryBufferEntry::get_pc() const {
    return pc;
}

Tick WriteHistoryBufferEntry::get_gen_tick() const {
    assert(this->hasGenTick);
    return this->genTick;
}

void WriteHistoryBufferEntry::set_gen_tick(Tick genTick) {
    this->hasGenTick = true;
    this->genTick = genTick;
}

void WriteHistoryBufferEntry::set_id(uint64_t id) {
    this->id = id;
}

uint64_t WriteHistoryBufferEntry::get_id() const {
    return this->id;
}

void WriteHistoryBufferEntry::use() {
    this->isUsed = true;
}

bool WriteHistoryBufferEntry::is_used() const {
    return this->isUsed;
}

hash_t WriteHistoryBufferEntry::get_path_hash() const {
    panic_if_not(hasPathHash);
    return this->pathHash;
}

void WriteHistoryBufferEntry::set_path_hash(hash_t hash) {
    this->pathHash = hash;
    this->hasPathHash = true;
}

size_t WriteHistoryBufferEntry::get_size() const {
    panic_if_not(hasSize);
    return this->size;
}

void WriteHistoryBufferEntry::set_size(size_t size) {
    this->size = size;
    this->hasSize = true;
}