#include <iostream>
#include <array>
#include <cstdint>

int main() {
    std::array<int16_t, 4> res16 = { -1, 2, -3, 4 };
    std::array<int32_t, 4> res32 = { -1, 2, -3, 4 };
    int16_t filter[4] = { -10, 20, -30, 40 };

    int v1 = 0;
    int v2 = 0;
    for(int i=0; i<4; i++) {
        v1 += res16[i] * static_cast<unsigned>(filter[i]);
        v2 += res32[i] * static_cast<unsigned>(filter[i]);
    }
    std::cout << "v1=" << v1 << " v2=" << v2 << std::endl;
}
