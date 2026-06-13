#include "tak_encoder/wav_reader.hpp"
#include <cstring>
#include <stdexcept>

namespace takenc {

WavInfo read_wav_header(std::istream& is) {
    char chunk_id[4];
    uint32_t chunk_size;
    WavInfo info{};

    is.read(chunk_id, 4);
    if (std::memcmp(chunk_id, "RIFF", 4) != 0) throw std::runtime_error("Not a RIFF file");
    is.read(reinterpret_cast<char*>(&chunk_size), 4);
    is.read(chunk_id, 4);
    if (std::memcmp(chunk_id, "WAVE", 4) != 0) throw std::runtime_error("Not a WAVE file");

    while (is.read(chunk_id, 4) && is.read(reinterpret_cast<char*>(&chunk_size), 4)) {
        if (std::memcmp(chunk_id, "fmt ", 4) == 0) {
            auto fmt_start = is.tellg();
            info.fmt_chunk.resize(chunk_size);
            is.read(reinterpret_cast<char*>(info.fmt_chunk.data()), chunk_size);
            is.seekg(fmt_start);

            uint16_t audio_format;
            is.read(reinterpret_cast<char*>(&audio_format), 2);
            is.read(reinterpret_cast<char*>(&info.channels), 2);
            is.read(reinterpret_cast<char*>(&info.sample_rate), 4);
            is.seekg(4, std::ios::cur); is.seekg(2, std::ios::cur);
            is.read(reinterpret_cast<char*>(&info.bps), 2);
            is.seekg(fmt_start + static_cast<std::streamoff>(chunk_size));
        } else if (std::memcmp(chunk_id, "data", 4) == 0) {
            info.data_size = chunk_size;
            break;
        } else {
            // Foreign chunk
            WavInfo::ForeignChunk fc;
            std::memcpy(fc.id, chunk_id, 4);
            fc.data.resize(chunk_size);
            is.read(reinterpret_cast<char*>(fc.data.data()), chunk_size);
            info.foreign_chunks.push_back(std::move(fc));
            
            // Chunk size must be padded to 2 bytes
            if (chunk_size % 2 != 0) {
                is.seekg(1, std::ios::cur);
            }
        }
    }
    
    if (info.fmt_chunk.empty()) throw std::runtime_error("No fmt chunk");
    
    return info;
}

} // namespace takenc
