#include <iostream>
#include <cstdint>

int main() {
    int dfactor = -10;
    int b = 1000;
    
    int v_signed = (unsigned)((dfactor * b + 128) >> 8) << 2;
    int v_unsigned = static_cast<unsigned>(static_cast<int>((dfactor * static_cast<unsigned>(b)) + 128) >> 8) << 2;
    
    std::cout << "v_signed=" << v_signed << " v_unsigned=" << v_unsigned << std::endl;
}
