#ifndef TAK_ENCODER_MD5_HPP
#define TAK_ENCODER_MD5_HPP

#include <cstdint>
#include <string>
#include <array>

namespace takenc {

class MD5 {
public:
    MD5();
    void update(const uint8_t* data, size_t length);
    void finalize();
    std::string to_string() const;
    std::array<uint8_t, 16> digest() const;

private:
    void transform(const uint8_t block[64]);
    
    uint32_t state[4];
    uint32_t count[2];
    uint8_t buffer[64];
    std::array<uint8_t, 16> digest_;
    bool finalized;
};

} // namespace takenc

#endif // TAK_ENCODER_MD5_HPP
