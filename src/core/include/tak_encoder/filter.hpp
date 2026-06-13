#ifndef TAK_ENCODER_FILTER_HPP
#define TAK_ENCODER_FILTER_HPP

#include <vector>
#include <cstdint>
#include <array>

namespace takenc {
    constexpr std::array<uint16_t, 16> predictor_sizes = {
        4, 8, 12, 16, 24, 32, 48, 64, 80, 96, 128, 160, 192, 224, 256, 0,
    };

    struct FilterConfig {
        int order_index; // index into predictor_sizes
        int filter_order;
        int filter_quant;
        int dshift;

        int size;
        std::vector<int> predictors;
        std::vector<int32_t> warmup_residuals;
        int warmup_lpc_mode;
        std::vector<int32_t> filter_residuals;
        int total_bits;
    };

    // Evaluate and populate FilterConfig. Returns false if signal is degenerate or size is too small.
    bool try_filter_encode(const int32_t *samples, int subframe_size,
                           int order_idx, FilterConfig &cfg, bool max_compression = false);

    void inverse_lpc(int32_t *data, int mode, int length);

    int estimate_lpc_cost(const int32_t *samples, int length, int lpc_mode);
} // namespace takenc

#endif // TAK_ENCODER_FILTER_HPP
