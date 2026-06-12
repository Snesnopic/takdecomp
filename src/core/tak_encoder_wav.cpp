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
        if (std::memcmp(chunk_id, "fmt ", 4) == 0) break;
        is.seekg(chunk_size, std::ios::cur);
    }
    if (!is) throw std::runtime_error("No fmt chunk");
    auto fmt_start = is.tellg();
    uint16_t audio_format;
    is.read(reinterpret_cast<char*>(&audio_format), 2);
    is.read(reinterpret_cast<char*>(&info.channels), 2);
    is.read(reinterpret_cast<char*>(&info.sample_rate), 4);
    is.seekg(4, std::ios::cur); is.seekg(2, std::ios::cur);
    is.read(reinterpret_cast<char*>(&info.bps), 2);
    is.seekg(fmt_start + static_cast<std::streamoff>(chunk_size));
    while (is.read(chunk_id, 4) && is.read(reinterpret_cast<char*>(&chunk_size), 4)) {
        if (std::memcmp(chunk_id, "data", 4) == 0) break;
        is.seekg(chunk_size, std::ios::cur);
    }
    if (!is) throw std::runtime_error("No data chunk");
    info.data_size = chunk_size;
    return info;
}

} // namespace takenc
