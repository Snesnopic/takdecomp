#include "tak_encoder/subframe.hpp"
#include "tak_encoder/encoder.hpp"
#include "tak_decoder/constants.hpp"
#include <iostream>
#include <fstream>
#include <vector>

namespace takenc {



SubframeChoice evaluate_subframe(const int32_t* subframe_data, int subframe_size, const EncoderConfig& cfg) {
    SubframeChoice choice = {};
    choice.use_filter = false;

    // Estimate cost of no-filter path
    int best = Encoder::calc_bits_needed(1, subframe_data, subframe_size);
    int limit = std::min(cfg.max_lpc_mode, 34);
    for (int m = 2; m <= limit; m++) {
        int c = Encoder::calc_bits_needed(m, subframe_data, subframe_size);
        if (c < best) best = c;
    }
    choice.nofilter_cost = 1 + 1 + 6 + best; // filter_flag(1) + segment_flag(1) + mode(6) + data
    choice.total_bits = choice.nofilter_cost;

    // Try predictor filter with different orders
    bool have_filter = false;
    FilterConfig best_filter;
    
    if (cfg.test_filters) {
        int max_idx = std::min(cfg.max_filter_order_idx, 14); // 14 is max predictor_sizes index
        for (int idx = 0; idx <= max_idx; idx++) {
            FilterConfig fcfg;
            if (try_filter_encode(subframe_data, subframe_size, idx, fcfg, cfg.max_compression)) {
                if (!have_filter || fcfg.total_bits < best_filter.total_bits) {
                    best_filter = fcfg;
                    have_filter = true;
                }
            }
        }
    }

    if (have_filter && best_filter.total_bits < choice.nofilter_cost) {
        choice.use_filter = true;
        choice.filter = best_filter;
        choice.total_bits = best_filter.total_bits;
    }

    return choice;
}



static void encode_residues(const int32_t* data, int length, BitStreamWriter& fw) {
    int best_m = 1;
    int best_c = Encoder::calc_bits_needed(1, data, length);
    for (int m = 2; m <= 34; m++) {
        int c = Encoder::calc_bits_needed(m, data, length);
        if (c < best_c) { best_c = c; best_m = m; }
    }
    fw.write_bit(0);
    fw.write_bits(best_m, 6);
    Encoder::encode_segment(best_m, data, length, fw);
}

void write_subframe(const SubframeChoice& choice, const int32_t* subframe_data,
                    int subframe_size, int prev_subframe_size, BitStreamWriter& fw) {
    if (choice.use_filter) {
        // filter flag
        fw.write_bit(1);
        fw.write_bits(choice.filter.order_index, 4);
        if (prev_subframe_size > 0) {
            fw.write_bit(0); // new filter (not reusing)
        }
        fw.write_bits(choice.filter.warmup_lpc_mode, 2);

        // Warmup samples
        encode_residues(choice.filter.warmup_residuals.data(), choice.filter.filter_order, fw);

        // dshift
        if (choice.filter.dshift > 0) {
            fw.write_bit(1);
            fw.write_bits(choice.filter.dshift - 1, 4);
        } else {
            fw.write_bit(0);
        }
        
        // size
        fw.write_bit(choice.filter.size == 7 ? 1 : 0);
        // filter_quant = 10 (default, write 0)
        fw.write_bit(0);

        // Predictors
        fw.write_bits(choice.filter.predictors[0] & 0x3FF, 10);
        fw.write_bits(choice.filter.predictors[1] & 0x3FF, 10);
        {
            int shift = 10 - choice.filter.size;
            int raw2 = choice.filter.predictors[2] >> shift;
            int raw3 = choice.filter.predictors[3] >> shift;
            fw.write_bits(raw2 & ((1 << choice.filter.size) - 1), choice.filter.size);
            fw.write_bits(raw3 & ((1 << choice.filter.size) - 1), choice.filter.size);
        }
        if (choice.filter.filter_order > 4) {
            fw.write_bit(0); // 1st escape (diff=0) -> size doesn't change
            for (int i = 4; i < choice.filter.filter_order; i++) {
                if ((i & 3) == 0) {
                    fw.write_bits(0, 2); // diff=0
                }
                int shift = 10 - choice.filter.size;
                int raw = choice.filter.predictors[i] >> shift;
                fw.write_bits(raw & ((1 << choice.filter.size) - 1), choice.filter.size);
            }
        }

        // Filter residuals
        encode_residues(choice.filter.filter_residuals.data(),
                        static_cast<int>(choice.filter.filter_residuals.size()), fw);
    } else {
        fw.write_bit(0);
        encode_residues(subframe_data, subframe_size, fw);
    }
}

// ============================================================================
// Encoder: write one channel
// ============================================================================

void encode_channel(const int32_t* samples, int nb_samples, int bps,
                    int lpc_mode, int sample_rate, const EncoderConfig& cfg, BitStreamWriter& fw) {
    int subframe_size = nb_samples - 1;
    const int32_t* subframe_data = samples + 1;

    // Evaluate 1 subframe
    SubframeChoice c1 = evaluate_subframe(subframe_data, subframe_size, cfg);
    int best_cost = c1.total_bits;
    int best_splits = 1;
    std::vector<SubframeChoice> best_choices = { c1 };
    std::vector<int> best_lens = { subframe_size };
    std::vector<int> best_vs;

    // Evaluate 2 subframes
    int base_align = (((sample_rate + 511) >> 9) + 3) & ~3;
    int subframe_scale = base_align << 1;
    
    if (cfg.test_subframe_splits && subframe_size > subframe_scale * 4) {
        int v_mid = (subframe_size / 2) / subframe_scale;
        int len1 = v_mid * subframe_scale;
        int len2 = subframe_size - len1;
        if (len1 > 0 && len2 > 0) {
            SubframeChoice c2_1 = evaluate_subframe(subframe_data, len1, cfg);
            SubframeChoice c2_2 = evaluate_subframe(subframe_data + len1, len2, cfg);
            int cost2 = c2_1.total_bits + c2_2.total_bits + 6; // +6 bits for v
            if (cost2 < best_cost) {
                best_cost = cost2;
                best_splits = 2;
                best_choices = { c2_1, c2_2 };
                best_lens = { len1, len2 };
                best_vs = { v_mid };
            }
        }
        
        // Evaluate 4 subframes
        int v_1 = v_mid / 2;
        int v_3 = v_mid + (v_mid / 2);
        int l1 = v_1 * subframe_scale;
        int l2 = (v_mid - v_1) * subframe_scale;
        int l3 = (v_3 - v_mid) * subframe_scale;
        int l4 = subframe_size - (l1 + l2 + l3);
        if (l1 > 0 && l2 > 0 && l3 > 0 && l4 > 0) {
            SubframeChoice c4_1 = evaluate_subframe(subframe_data, l1, cfg);
            SubframeChoice c4_2 = evaluate_subframe(subframe_data + l1, l2, cfg);
            SubframeChoice c4_3 = evaluate_subframe(subframe_data + l1 + l2, l3, cfg);
            SubframeChoice c4_4 = evaluate_subframe(subframe_data + l1 + l2 + l3, l4, cfg);
            int cost4 = c4_1.total_bits + c4_2.total_bits + c4_3.total_bits + c4_4.total_bits + 18; // +18 bits for v
            if (cost4 < best_cost) {
                
                best_splits = 4;
                best_choices = { c4_1, c4_2, c4_3, c4_4 };
                best_lens = { l1, l2, l3, l4 };
                best_vs = { v_1, v_mid, v_3 };
            }
        }
    }

    // Write header
    fw.write_bit(0);                 // sample_shift = 0
    fw.write_bits(samples[0] & ((1u << bps) - 1), bps);
    fw.write_bits(lpc_mode, 2);
    fw.write_bits(best_splits - 1, 3); // nb_subframes - 1

    // Write split points
    if (best_splits > 1) {
        for (int v : best_vs) {
            fw.write_bits(v, 6);
        }
    }


    
    // Write subframes
    int prev_len = 0;
    int offset = 0;
    for (int i = 0; i < best_splits; i++) {
        write_subframe(best_choices[i], subframe_data + offset, best_lens[i], prev_len, fw);
        offset += best_lens[i];
        prev_len = best_lens[i];
    }
}



} // namespace takenc
