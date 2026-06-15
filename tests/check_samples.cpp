#include <iostream>
#include <vector>
#include "tak_encoder/wav_reader.hpp"

int main() {
    takenc::WavReader reader("tests/short2.wav");
    if (!reader.is_open()) {
        std::cerr << "Could not open" << std::endl;
        return 1;
    }
    auto info = reader.get_info();
    std::cout << "Channels: " << info.channels << ", Samples: " << info.sample_count << std::endl;
    std::vector<int32_t> buffer(info.channels * 10);
    reader.read(buffer.data(), 10);
    for (int i=0; i<10; i++) {
        std::cout << buffer[i*2] << ", " << buffer[i*2+1] << std::endl;
    }
    return 0;
}
