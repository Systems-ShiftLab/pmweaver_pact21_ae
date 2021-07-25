#include "Declarations.hh"
#include "mem/predictor/ChunkInfo.hh"
#include "mem/predictor/CacheLine.hh"
#include "mem/predictor/LastFoundKeyEntry.hh"
#include "mem/predictor/PredictorTable.hh"

std::ostream &operator<<(std::ostream &out, const ChunkInfo &data) {
    auto genPC = data.has_generating_pc() ? data.get_generating_pc() : -1;
    auto ownerKey = data.has_owner_key() ? data.get_owner_key() : -1;
    auto offset = data.has_data_field_offset() ? data.get_data_field_offset() : -1;

    out << "Generating PC: "
        << "0x" << std::hex << std::setw(16) << std::setfill('0') << genPC << std::endl
        << "owner_key : 0x" << ownerKey << std::endl
        << "chunkType" << static_cast<uint64_t>(data.get_chunk_type()) << std::endl 
        << "dataFieldOffset: " << offset << std::endl
        << "isComplete: " << data.get_completion() << std::endl << std::dec;
    return out;
}

std::ostream& operator<<(std::ostream& out, const CacheLine& data) {
    const size_t fieldWidth = sizeof(DataChunk)*2;

    if (data.hasAddr) {
        out << "[" << RED << print_ptr(16) << data.get_addr() << "] " << RST;
    } else {
        out << "[" << RED << print_ptr(16) << 0 << "] " << RST;
    }
    for (int i = 0; i < DATA_CHUNK_COUNT; i++) {
        panic_if(data.get_datachunks()[i].is_valid() 
                    and data.get_datachunks()[i].get_chunk_type() 
                        != ChunkInfo::ChunkType::DATA, 
                    "Cacheline has a non-data type chunk at offset %d for address %p",
                    i, (void*)data.get_addr());

        out << std::hex << i << ":0x";
        if (data.get_datachunks()[i].get_chunk_type() == ChunkInfo::ChunkType::DATA) {
            out << std::setw(fieldWidth) << std::setfill('0') << data.get_datachunks()[i].get_data() << " " << std::dec;
            std::string isConst = data.get_datachunks()[i].is_constant_pred() ? BHRED "C" RST : " ";
            out << isConst;
        } else {
            out << "INVALID#" << " ";
            out << "X";
        }
    }
    out << std::dec;
    return out;
}

std::ostream& operator<<(std::ostream& os, PredictorTableEntry& pte) {
    // out << "[0x" << std::hex << data.get_addr() << "] :" << std::endl;
    const size_t fieldWidth = sizeof(DataChunk)*2;

    for (int i = 0; i < DATA_CHUNK_COUNT; i++) {
        os << " ";
        if (pte.get_datachunks()[i].is_valid()) {
            os << print_ptr(16) << pte.get_datachunks()[i].get_generating_pc();
        } else {
            os << std::setw(16) << std::setfill(' ') << "NOT VALID";
        }
    }
    os << std::endl;
    os << std::dec;
    return os;
}

std::ostream& operator<<(std::ostream& os, const IHB_Entry& ihb_e) {
    os << ihb_e.get_pc();
    return os;
}


uint32_t Confidence::operator()() const {
    return this->conf;
}