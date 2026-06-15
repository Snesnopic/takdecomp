#include "../../src/core/include/tak_encoder/bitstream_writer.hpp"
#include <stdio.h>

int main() {
    takenc::BitStreamWriter fw;
    fw.write_bits(0xFFA0, 16);
    fw.align_write_bits();
    auto data = fw.get_data();
    for(auto b : data) printf("%02X ", b);
    printf("\n");
    return 0;
}
