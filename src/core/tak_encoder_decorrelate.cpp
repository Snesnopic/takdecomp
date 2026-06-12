#include "tak_encoder/decorrelate.hpp"
#include "tak_encoder/encoder.hpp"
#include <algorithm>
#include <stdexcept>

namespace takenc {

void Decorrelator::apply_mode(int mode, const int32_t* src1, const int32_t* src2, int32_t* dst1, int32_t* dst2, int len) {
    switch (mode) {
    case 0:
        for (int i = 0; i < len; i++) {
            dst1[i] = src1[i];
            dst2[i] = src2[i];
        }
        break;
    case 1: // Left / Side (p1=Left, p2=Side)
        for (int i = 0; i < len; i++) {
            dst1[i] = src1[i];
            dst2[i] = src2[i] - src1[i];
        }
        break;
    case 2: // Side / Right (p1=Side, p2=Right)
        for (int i = 0; i < len; i++) {
            dst1[i] = src2[i] - src1[i];
            dst2[i] = src2[i];
        }
        break;
    case 3: // Mid / Side (p1=Mid, p2=Side)
        for (int i = 0; i < len; i++) {
            dst1[i] = (src1[i] + src2[i]) >> 1;
            dst2[i] = src2[i] - src1[i];
        }
        break;
    default:
        throw std::runtime_error("Advanced decorrelation modes not implemented yet");
    }
}

int Decorrelator::apply_decorrelation(int32_t* data_c1, int32_t* data_c2, int len) {
    std::vector<int32_t> buf1(len);
    std::vector<int32_t> buf2(len);

    int best_mode = 0;
    int best_cost = 2147483647; // INT_MAX

    for (int mode = 0; mode <= 3; mode++) {
        apply_mode(mode, data_c1, data_c2, buf1.data(), buf2.data(), len);
        
        int cost1 = Encoder::calc_bits_needed(2, buf1.data(), len); // rough estimation using mode 2?
        int cost2 = Encoder::calc_bits_needed(2, buf2.data(), len); 
        int total_cost = cost1 + cost2;

        if (total_cost < best_cost) {
            best_cost = total_cost;
            best_mode = mode;
        }
    }

    // Apply the best mode to the original buffers
    apply_mode(best_mode, data_c1, data_c2, buf1.data(), buf2.data(), len);
    for (int i = 0; i < len; i++) {
        data_c1[i] = buf1[i];
        data_c2[i] = buf2[i];
    }

    return best_mode;
}

} // namespace takenc
