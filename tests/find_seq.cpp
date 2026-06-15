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
    int total = (size - data_offset) / 4;
    for (int i = 0; i < total - 8; i++) {
        if (samples[2*i] == -4 && samples[2*(i+1)] == -4 && samples[2*(i+2)] == 10 && samples[2*(i+3)] == 12) {
            std::cout << "Found at sample " << i << "\n";
            return 0;
        }
    }
    std::cout << "Not found!\n";
    return 0;
}
