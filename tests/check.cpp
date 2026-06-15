#include <iostream>
#include <fstream>
#include <vector>

int main() {
    std::ifstream f("tests/short2.wav", std::ios::binary);
    f.seekg(0x4a0); // skip 1184 bytes? No wait, let's just use 44.
    int16_t samples[20];
    f.read((char*)samples, 40);
    for(int i=0; i<10; i++) {
        std::cout << samples[i*2] << ", " << samples[i*2+1] << std::endl;
    }
}
