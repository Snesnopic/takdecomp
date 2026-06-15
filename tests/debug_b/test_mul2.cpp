#include <iostream>
#include <cstdint>

int main() {
    int16_t res = -10;
    int16_t filter = -10;
    
    int v_signed = (int)res * (int)filter;
    int v_unsigned = (int)( (unsigned)res * (unsigned)filter );
    
    std::cout << "v_signed=" << v_signed << " v_unsigned=" << v_unsigned << std::endl;
}
