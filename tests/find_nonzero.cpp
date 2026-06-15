#include <iostream>
#include <fstream>
#include <vector>

int main() {
    std::ifstream f("tests/short2.wav", std::ios::binary);
    f.seekg(44); // skip wav header
    int16_t sample;
    int idx = 0;
    while (f.read((char*)&sample, 2)) {
        if (sample != 0) {
            std::cout << "First non-zero at: " << idx / 2 << " ch " << (idx % 2) << " val: " << sample << std::endl;
            break;
        }
        idx++;
    }
}
