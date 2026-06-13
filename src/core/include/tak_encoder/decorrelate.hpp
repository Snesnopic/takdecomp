#ifndef TAK_ENCODER_DECORRELATE_HPP
#define TAK_ENCODER_DECORRELATE_HPP

#include <cstdint>
#include <vector>

namespace takenc {
    class Decorrelator {
    public:
        Decorrelator() = default;

        struct DecorrelationResult {
            int mode;
            int shift;
            int factor;
            int filter_order;
            std::vector<int> filter;
        };

        // Evaluates and applies the best stereo decorrelation mode.
        // data_c1 and data_c2 are the original left/right channels (size len).
        // They will be overwritten with the encoded channels (e.g. Mid/Side).
        // Returns the selected mode (0 to 7).
        DecorrelationResult apply_decorrelation(int32_t *data_c1, int32_t *data_c2, int len);

    private:
        void apply_mode(int mode, int shift, int factor, const std::vector<int> &filter, const int32_t *src1,
                        const int32_t *src2, int32_t *dst1, int32_t *dst2, int len);
    };
} // namespace takenc

#endif // TAK_ENCODER_DECORRELATE_HPP
