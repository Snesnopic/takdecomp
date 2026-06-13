#include <iostream>
#include <vector>
#include <cstdint>

namespace takdecomp {
    uint32_t compute_crc24(const uint8_t* data, size_t len);
}

int main() {
    const uint8_t payload[] = {0xff, 0xa0, 0x02, 0x00, 0x00, 0x02, 0x10, 0x55, 0xd7, 0x00, 0x00, 0x40, 0x4d, 0x09, 0x04, 0x00};
    uint32_t crc = takdecomp::compute_crc24(payload, 16);
    printf("CRC: %06x\n", crc);
    return 0;
}
