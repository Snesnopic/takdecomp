#include <iostream>
#include <fstream>
#include <vector>

int main() {
    std::ifstream is("tests/short2.wav", std::ios::binary);
    is.seekg(0, std::ios::end);
    size_t size = is.tellg();
    is.seekg(0, std::ios::beg);
    std::vector<char> raw_bytes(size);
    is.read(raw_bytes.data(), size);
    
    size_t data_offset = 0;
    for (size_t i = 12; i < size - 4; i++) {
        if (raw_bytes[i] == 'd' && raw_bytes[i+1] == 'a' && raw_bytes[i+2] == 't' && raw_bytes[i+3] == 'a') {
            data_offset = i + 8;
            break;
        }
    }
    
    int16_t* pcm_data = reinterpret_cast<int16_t*>(raw_bytes.data() + data_offset);
    int current_frame_samples = 11025;
    
    std::vector<int32_t> c0(current_frame_samples);
    std::vector<int32_t> c1(current_frame_samples);
    
    // simulate frame 1 read
    int16_t* f1_data = pcm_data + 11025 * 2;
    for (int i = 0; i < current_frame_samples; i++) {
        c0[i] = f1_data[i * 2];
        c1[i] = f1_data[i * 2 + 1];
    }
    
    for (int i = 0; i < 8; i++) {
        std::cout << "c1[" << i << "] = " << c1[i] << "\n";
    }
    return 0;
}
