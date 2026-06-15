#include <iostream>
#include <fstream>
#include <vector>

int main() {
    std::ifstream file("tests/short2.wav", std::ios::binary);
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<char> buffer(size);
    file.read(buffer.data(), size);
    
    size_t data_offset = 0;
    for (size_t i = 12; i < size - 4; i++) {
        if (buffer[i] == 'd' && buffer[i+1] == 'a' && buffer[i+2] == 't' && buffer[i+3] == 'a') {
            data_offset = i + 8;
            break;
        }
    }
    
    int16_t* samples = reinterpret_cast<int16_t*>(buffer.data() + data_offset);
    for (int i = 11025; i < 11025 + 8; i++) {
        std::cout << "Sample " << i << ": ch0=" << samples[2*i] << ", ch1=" << samples[2*i+1] << "\n";
    }
    return 0;
}
