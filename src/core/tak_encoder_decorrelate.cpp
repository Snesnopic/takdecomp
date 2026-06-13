#include "tak_encoder/decorrelate.hpp"
#include "tak_encoder/encoder.hpp"
#include <algorithm>
#include <stdexcept>

namespace takenc {

void Decorrelator::apply_mode(int mode, int shift, int factor, const std::vector<int>& filter, const int32_t* src1, const int32_t* src2, int32_t* dst1, int32_t* dst2, int len) {
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
    case 6: // Cross-FIR Right(pred) / Left(src)
    case 7: { // Cross-FIR Left(pred) / Right(src)
        const int32_t* p_src = (mode == 6) ? src1 : src2;
        const int32_t* p_tgt = (mode == 6) ? src2 : src1;
        int32_t* p_src_dst = (mode == 6) ? dst1 : dst2;
        int32_t* p_tgt_dst = (mode == 6) ? dst2 : dst1;
        
        int K = filter.size();
        int order_half = K / 2;
        
        // Sample 0 is always unpredicted/copied verbatim
        p_src_dst[0] = p_src[0];
        p_tgt_dst[0] = p_tgt[0];

        // Advance pointers by 1
        p_src++;
        p_tgt++;
        p_src_dst++;
        p_tgt_dst++;
        len--;

        for (int i = 0; i < len; i++) {
            p_src_dst[i] = p_src[i];
        }

        // Boundaries are unpredicted (linear sum like mode 1)
        for (int i = 0; i < order_half; i++) {
            p_tgt_dst[i] = p_tgt[i] - p_src[i];
        }
        for (int i = len - order_half; i < len; i++) {
            p_tgt_dst[i] = p_tgt[i];
        }
        
        // FIR Filter application
        for (int i = 0; i < len - (K - 1); i++) {
            int v = 1 << 9;
            for (int j = 0; j < K; j++) {
                v += (p_src[i + j] >> shift) * filter[j];
            }
            int32_t v_clip = ((v >> 10) + (1 << 13)) & ~((2U << 13) - 1) ? ((v >> 31) ^ ((1 << 13) - 1)) : (v >> 10);
            int32_t v_scale = v_clip * (1 << shift);
            
            p_tgt_dst[order_half + i] = v_scale - p_tgt[order_half + i];
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

static bool solve_linear_system(int n, std::vector<double>& A, std::vector<double>& b, std::vector<double>& x) {
    for (int i = 0; i < n; i++) {
        int pivot = i;
        for (int j = i + 1; j < n; j++) {
            if (std::abs(A[j * n + i]) > std::abs(A[pivot * n + i])) {
                pivot = j;
            }
        }
        if (pivot != i) {
            for (int j = i; j < n; j++) {
                std::swap(A[i * n + j], A[pivot * n + j]);
            }
            std::swap(b[i], b[pivot]);
        }
        if (std::abs(A[i * n + i]) < 1e-12) {
            return false;
        }
        for (int j = i + 1; j < n; j++) {
            double factor = A[j * n + i] / A[i * n + i];
            for (int k = i; k < n; k++) {
                A[j * n + k] -= factor * A[i * n + k];
            }
            b[j] -= factor * b[i];
        }
    }
    for (int i = n - 1; i >= 0; i--) {
        double sum = 0;
        for (int j = i + 1; j < n; j++) {
            sum += A[i * n + j] * x[j];
        }
        x[i] = (b[i] - sum) / A[i * n + i];
    }
    return true;
}

static bool compute_cross_fir_filter(const int32_t* target, const int32_t* source, int len, int K, int shift, std::vector<int>& filter_out) {
    std::vector<double> R(K * K, 0.0);
    std::vector<double> C(K, 0.0);
    
    int k_min = K / 2;
    int k_max = len - K / 2;
    if (k_max <= k_min) return false;
    
    for (int m = 0; m < K; m++) {
        double sum_c = 0;
        for (int k = k_min; k < k_max; k++) {
            sum_c += static_cast<double>(target[k]) * static_cast<double>(source[k - K/2 + m] >> shift);
        }
        C[m] = sum_c;
        
        for (int j = 0; j < K; j++) {
            double sum_r = 0;
            for (int k = k_min; k < k_max; k++) {
                sum_r += static_cast<double>(source[k - K/2 + j] >> shift) * static_cast<double>(source[k - K/2 + m] >> shift);
            }
            R[m * K + j] = sum_r;
        }
        R[m * K + m] += 1e-6; // Ridge regularization
    }
    
    std::vector<double> x(K, 0.0);
    if (!solve_linear_system(K, R, C, x)) return false;
    
    filter_out.resize(K);
    for (int i = 0; i < K; i++) {
        double val = x[i] * 1024.0;
        int quant = static_cast<int>(val + (val > 0 ? 0.5 : -0.5));
        if (quant > 8191) quant = 8191;
        if (quant < -8192) quant = -8192;
        filter_out[i] = quant;
    }
    return true;
}

Decorrelator::DecorrelationResult Decorrelator::apply_decorrelation(int32_t* data_c1, int32_t* data_c2, int len) {
    std::vector<int32_t> buf1(len);
    std::vector<int32_t> buf2(len);

    int best_mode = 0;
    int best_shift = 0;
    int best_factor = 0;
    std::vector<int> best_filter;
    int best_cost = 2147483647; // INT_MAX

    auto evaluate = [&](int mode, int shift, int factor, const std::vector<int>& filter) {
        apply_mode(mode, shift, factor, filter, data_c1, data_c2, buf1.data(), buf2.data(), len);
        int cost1 = estimate_entropy_fast(buf1.data(), len);
        int cost2 = estimate_entropy_fast(buf2.data(), len);
        int total_cost = cost1 + cost2;
        if (mode >= 4 && mode <= 5) {
            total_cost += 1 + (shift > 0 ? 4 : 0) + 10; // Overhead for dshift and dfactor
        } else if (mode >= 6) {
            int K = filter.size();
            int num_groups = K / 4;
            total_cost += 1 + (shift > 0 ? 4 : 0); // dshift
            total_cost += 1; // filter_order (8 or 16)
            total_cost += 2; // dval1, dval2
            total_cost += num_groups * 3; // code_size headers
            
            for (int i = 0; i < K; i += 4) {
                int max_val = 0;
                for (int j = 0; j < 4; j++) max_val = std::max(max_val, std::abs(filter[i + j]));
                int code_size = 0;
                while ((1 << code_size) <= max_val && code_size < 14) code_size++;
                if (code_size > 0) code_size++; // sign bit
                if (code_size < 7) code_size = 7;
                total_cost += 4 * code_size;
            }
        }
        
        if (total_cost < best_cost) {
            best_cost = total_cost;
            best_mode = mode;
            best_shift = shift;
            best_factor = factor;
            best_filter = filter;
        }
    };

    std::vector<int> dummy_filter;
    evaluate(0, 0, 0, dummy_filter);
    evaluate(1, 0, 0, dummy_filter);
    evaluate(2, 0, 0, dummy_filter);
    evaluate(3, 0, 0, dummy_filter);

    for (int shift = 0; shift <= 4; shift++) {
        int factor = compute_optimal_factor(data_c1, data_c2, len, shift);
        evaluate(4, shift, factor, dummy_filter);
    }

    for (int shift = 0; shift <= 4; shift++) {
        int factor = compute_optimal_factor(data_c2, data_c1, len, shift);
        evaluate(5, shift, factor, dummy_filter);
    }

    // Mode 6: p1=Right, p2=Left (filter predicts Right from Left)
    for (int K : {8, 16}) {
        for (int shift = 0; shift <= 4; shift++) {
            std::vector<int> filter_out;
            if (compute_cross_fir_filter(data_c2, data_c1, len, K, shift, filter_out)) {
                evaluate(6, shift, 0, filter_out);
            }
        }
    }
    
    // Mode 7: p1=Left, p2=Right (filter predicts Left from Right)
    for (int K : {8, 16}) {
        for (int shift = 0; shift <= 4; shift++) {
            std::vector<int> filter_out;
            if (compute_cross_fir_filter(data_c1, data_c2, len, K, shift, filter_out)) {
                evaluate(7, shift, 0, filter_out);
            }
        }
    }

    std::cerr << "ENCODER APPLYING BEST MODE: " << best_mode << std::endl;
    apply_mode(best_mode, best_shift, best_factor, best_filter, data_c1, data_c2, buf1.data(), buf2.data(), len);
    for (int i = 0; i < len; i++) {
        data_c1[i] = buf1[i];
        data_c2[i] = buf2[i];
    }

    return {best_mode, best_shift, best_factor, (int)best_filter.size(), best_filter};
}

} // namespace takenc
