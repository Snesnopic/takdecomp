#include <iostream>
#include <fstream>
#include <vector>

int main() {
    std::ifstream f("tests/short2.wav", std::ios::binary);
    // Find "data" chunk
    char buf[4];
    f.seekg(12);
    while (f.read(buf, 4)) {
        uint32_t size;
        f.read((char*)&size, 4);
        if (std::string(buf, 4) == "data") {
            std::cout << "Data chunk at " << f.tellg() << ", size " << size << std::endl;
            int16_t samples[2];
            f.read((char*)samples, 4);
            std::cout << "First samples: " << samples[0] << ", " << samples[1] << std::endl;
            break;
        }
        f.seekg(size, std::ios::cur);
    }
}
