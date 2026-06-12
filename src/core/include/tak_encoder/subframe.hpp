#ifndef TAK_ENCODER_SUBFRAME_HPP
#define TAK_ENCODER_SUBFRAME_HPP

#include "tak_encoder/filter.hpp"
#include "tak_encoder/bitstream_writer.hpp"
#include <cstdint>

namespace takenc {

struct SubframeChoice {
    bool use_filter;
    FilterConfig filter;
    int nofilter_cost;
    int total_bits;
};

SubframeChoice evaluate_subframe(const int32_t* subframe_data, int subframe_size);
void write_subframe(const SubframeChoice& choice, const int32_t* subframe_data,
                    int subframe_size, int prev_subframe_size, BitStreamWriter& fw);
void encode_channel(const int32_t* samples, int nb_samples, int bps,
                    int lpc_mode, int sample_rate, BitStreamWriter& fw);

} // namespace takenc

#endif // TAK_ENCODER_SUBFRAME_HPP
