#include "tak_encoder/decorrelate.hpp"
#include "tak_encoder/encoder.hpp"
#include <algorithm>
#include <stdexcept>

namespace takenc {

void Decorrelator::apply_mode(int mode, int shift, int factor, const int32_t* src1, const int32_t* src2, int32_t* dst1, int32_t* dst2, int len) {
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
    case 4: { // Left / Side with scale (decoder Side / Left)
        for (int i = 0; i < len; i++) {
            int32_t left = src1[i];
            int32_t right = src2[i];
            int32_t left_scaled = static_cast<unsigned>(static_cast<int>((factor * static_cast<unsigned>(left >> shift)) + 128) >> 8) << shift;
            dst1[i] = left;
            dst2[i] = left_scaled - right; // Side = Left_scaled - Right
        }
        break;
    }
    case 5: { // Side / Right with scale (decoder Side / Right)
        for (int i = 0; i < len; i++) {
            int32_t left = src1[i];
            int32_t right = src2[i];
            int32_t right_scaled = static_cast<unsigned>(static_cast<int>((factor * static_cast<unsigned>(right >> shift)) + 128) >> 8) << shift;
            dst1[i] = right_scaled - left; // Side = Right_scaled - Left
            dst2[i] = right;
        }
        break;
    }
    default:
        throw std::runtime_error("Advanced decorrelation modes not implemented yet");
    }

    if (mode > 0 && mode < 6) {
        dst1[0] = src1[0];
        dst2[0] = src2[0];
    }
}

static int estimate_entropy_fast(const int32_t* data, int len) {
    int best = Encoder::calc_bits_needed(1, data, len);
    for (int m = 2; m <= 34; m++) {
        int c = Encoder::calc_bits_needed(m, data, len);
        if (c < best) best = c;
    }
    return best;
}

static int compute_optimal_factor(const int32_t* pred_source, const int32_t* target, int len, int shift) {
    double sum_p2 = 0;
    double sum_pt = 0;
    for (int i = 0; i < len; i++) {
        double p = static_cast<double>(pred_source[i] >> shift);
        double t = static_cast<double>(target[i]);
        sum_p2 += p * p;
        sum_pt += p * t;
    }
    if (sum_p2 < 1.0) return 0;
    double factor_d = (sum_pt / sum_p2) * 256.0;
    int factor = static_cast<int>(factor_d + (factor_d > 0 ? 0.5 : -0.5));
    if (factor > 511) factor = 511;
    if (factor < -512) factor = -512;
    return factor;
}

Decorrelator::DecorrelationResult Decorrelator::apply_decorrelation(int32_t* data_c1, int32_t* data_c2, int len) {
    std::vector<int32_t> buf1(len);
    std::vector<int32_t> buf2(len);

    int best_mode = 0;
    int best_shift = 0;
    int best_factor = 0;
    int best_cost = 2147483647; // INT_MAX

    auto evaluate = [&](int mode, int shift, int factor) {
        apply_mode(mode, shift, factor, data_c1, data_c2, buf1.data(), buf2.data(), len);
        int cost1 = estimate_entropy_fast(buf1.data(), len);
        int cost2 = estimate_entropy_fast(buf2.data(), len);
        int total_cost = cost1 + cost2;
        if (mode >= 4) {
            total_cost += 1 + (shift > 0 ? 4 : 0) + 10; // Overhead for dshift and dfactor
        }
        if (total_cost < best_cost) {
            best_cost = total_cost;
            best_mode = mode;
            best_shift = shift;
            best_factor = factor;
        }
    };

    // Force mode 2
    evaluate(2, 0, 0);
    apply_mode(best_mode, best_shift, best_factor, data_c1, data_c2, buf1.data(), buf2.data(), len);
    for (int i = 0; i < len; i++) {
        data_c1[i] = buf1[i];
        data_c2[i] = buf2[i];
    }

    return {best_mode, best_shift, best_factor};
}

} // namespace takenc
