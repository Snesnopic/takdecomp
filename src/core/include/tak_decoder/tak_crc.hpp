#ifndef TAK_DECOMP_CRC_HPP
#define TAK_DECOMP_CRC_HPP

#include <cstdint>
#include <cstddef>

namespace takdecomp {

uint32_t compute_crc24(const uint8_t* data, size_t len);

} // namespace takdecomp

#endif // TAK_DECOMP_CRC_HPP
