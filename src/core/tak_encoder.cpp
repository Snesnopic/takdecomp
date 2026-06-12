#include "tak_encoder/encoder.hpp"
#include "tak_encoder/decorrelate.hpp"
#include "tak_decoder/constants.hpp"
#include "tak_decoder/tak_crc.hpp"
#include "tak_encoder/bitstream_writer.hpp"
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <vector>
#include <cstring>
#include <cmath>
#include <array>
#include <algorithm>

namespace takenc {

// ============================================================================
// WAV reader
// ============================================================================

struct WavInfo {
    uint16_t channels;
    uint32_t sample_rate;
    uint16_t bps;
    uint32_t data_size;
};

static WavInfo read_wav_header(std::ifstream& is) {
    WavInfo info{};
    char riff[4];
    is.read(riff, 4);
    if (std::memcmp(riff, "RIFF", 4) != 0) throw std::runtime_error("Not a RIFF file");
    is.seekg(4, std::ios::cur);
    char wave[4];
    is.read(wave, 4);
    if (std::memcmp(wave, "WAVE", 4) != 0) throw std::runtime_error("Not a WAVE file");

    char chunk_id[4]; uint32_t chunk_size;
    while (is.read(chunk_id, 4) && is.read(reinterpret_cast<char*>(&chunk_size), 4)) {
        if (std::memcmp(chunk_id, "fmt ", 4) == 0) break;
        is.seekg(chunk_size, std::ios::cur);
    }
    if (!is) throw std::runtime_error("No fmt chunk");
    auto fmt_start = is.tellg();
    uint16_t audio_format;
    is.read(reinterpret_cast<char*>(&audio_format), 2);
    is.read(reinterpret_cast<char*>(&info.channels), 2);
    is.read(reinterpret_cast<char*>(&info.sample_rate), 4);
    is.seekg(4, std::ios::cur); is.seekg(2, std::ios::cur);
    is.read(reinterpret_cast<char*>(&info.bps), 2);
    is.seekg(fmt_start + static_cast<std::streamoff>(chunk_size));
    while (is.read(chunk_id, 4) && is.read(reinterpret_cast<char*>(&chunk_size), 4)) {
        if (std::memcmp(chunk_id, "data", 4) == 0) break;
        is.seekg(chunk_size, std::ios::cur);
    }
    if (!is) throw std::runtime_error("No data chunk");
    info.data_size = chunk_size;
    return info;
}

// ============================================================================
// Inverse LPC (finite differencing)
// ============================================================================

static void inverse_lpc(int32_t* data, int mode, int length) {
    if (mode == 0 || length < 2) return;
    if (mode == 1) {
        for (int i = length - 1; i >= 1; i--)
            data[i] = (int32_t)((uint32_t)data[i] - (uint32_t)data[i-1]);
    } else if (mode == 2) {
        for (int i = length - 1; i >= 2; i--)
            data[i] = (int32_t)((uint32_t)data[i] - 2u*(uint32_t)data[i-1] + (uint32_t)data[i-2]);
        data[1] = (int32_t)((uint32_t)data[1] - (uint32_t)data[0]);
    } else if (mode == 3) {
        for (int i = length - 1; i >= 3; i--)
            data[i] = (int32_t)((uint32_t)data[i] - 3u*(uint32_t)data[i-1] + 3u*(uint32_t)data[i-2] - (uint32_t)data[i-3]);
        if (length > 2) data[2] = (int32_t)((uint32_t)data[2] - 2u*(uint32_t)data[1] + (uint32_t)data[0]);
        data[1] = (int32_t)((uint32_t)data[1] - (uint32_t)data[0]);
    }
}

static int estimate_lpc_cost(const int32_t* samples, int length, int lpc_mode) {
    std::vector<int32_t> tmp(samples, samples + length);
    inverse_lpc(tmp.data(), lpc_mode, length);
    int best = Encoder::calc_bits_needed(1, tmp.data() + 1, length - 1);
    for (int m = 2; m <= 50; m++) {
        int c = Encoder::calc_bits_needed(m, tmp.data() + 1, length - 1);
        if (c < best) best = c;
    }
    return best;
}

// ============================================================================
// Predictor filter helpers
// ============================================================================

static constexpr std::array<uint16_t, 16> predictor_sizes = {
    4, 8, 12, 16, 24, 32, 48, 64, 80, 96, 128, 160, 192, 224, 256, 0,
};

static int32_t clip_intp2(int32_t a, int p) {
    if (((static_cast<unsigned>(a) + (1 << p)) & ~((2U << p) - 1)) != 0u)
        return (a >> 31) ^ ((1 << p) - 1);
    return a;
}

// Compute autocorrelation r[0..order]
static void compute_autocorrelation(const int32_t* data, int len, double* r, int order) {
    for (int k = 0; k <= order; k++) {
        double sum = 0;
        for (int i = k; i < len; i++)
            sum += (double)data[i] * data[i - k];
        r[k] = sum;
    }
}

// Levinson-Durbin: compute PARCOR (reflection) coefficients from autocorrelation
// Returns true if stable, false if the signal is degenerate
static bool levinson_durbin(const double* r, int order, double* parcor) {
    if (r[0] <= 0.0) return false;

    std::vector<double> a(order), a_prev(order);
    double E = r[0];

    for (int i = 0; i < order; i++) {
        double sum = r[i + 1];
        for (int j = 0; j < i; j++)
            sum += a_prev[j] * r[i - j];

        double k = -sum / E;
        if (std::abs(k) >= 1.0) k = (k > 0) ? 0.999 : -0.999;
        parcor[i] = k;

        for (int j = 0; j < i; j++)
            a[j] = a_prev[j] + k * a_prev[i - 1 - j];
        a[i] = k;

        E *= (1.0 - k * k);
        if (E <= 0.0) return false;
        std::copy(a.begin(), a.end(), a_prev.begin());
    }
    return true;
}

// Build the filter from quantized predictors (exactly matches decoder)
static void build_filter(const int* predictors, int filter_order, int filter_quant,
                         int16_t* filter_out) {
    std::array<int, 256> tfilter{};
    tfilter[0] = predictors[0] * 64;
    for (int i = 1; i < filter_order; i++) {
        for (int j = 0; j < (i + 1) / 2; j++) {
            int32_t v1 = tfilter[j];
            int32_t v2 = tfilter[i - 1 - j];
            int32_t x = v1 + ((predictors[i] * v2 + 256) >> 9);
            tfilter[i - 1 - j] = v2 + ((predictors[i] * v1 + 256) >> 9);
            tfilter[j] = x;
        }
        tfilter[i] = predictors[i] * 64;
    }

    int x = 1 << (32 - (15 - filter_quant));
    int y = 1 << ((15 - filter_quant) - 1);
    for (int i = 0, j = filter_order - 1; i < filter_order / 2; i++, j--) {
        filter_out[j] = static_cast<int16_t>(x - ((tfilter[i] + y) >> (15 - filter_quant)));
        filter_out[i] = static_cast<int16_t>(x - ((tfilter[j] + y) >> (15 - filter_quant)));
    }
}

// Compute prediction residuals using the exact same filter as the decoder.
// decoded[0..filter_order-1] = warmup, decoded[filter_order..n-1] = samples to predict.
// Returns the residuals for decoded[filter_order..n-1].
static void compute_filter_residuals(const int32_t* decoded, int subframe_size,
                                      const int16_t* filter, int filter_order,
                                      int filter_quant, int dshift,
                                      int32_t* residuals_out) {
    // Match the decoder's residues_ buffer: int16_t[544]
    std::array<int16_t, 544> residues{};
    for (int i = 0; i < filter_order; i++)
        residues[i] = static_cast<int16_t>(decoded[i] >> dshift);

    int y = static_cast<int>(residues.size()) - filter_order;
    int x = subframe_size - filter_order;
    int out_idx = 0;

    while (x > 0) {
        int tmp = std::min(y, x);
        for (int i = 0; i < tmp; i++) {
            int v = 1 << (filter_quant - 1);
            for (int j = 0; j < filter_order; j++)
                v += residues[i + j] * static_cast<unsigned>(filter[j]);

            int prediction = clip_intp2(v >> filter_quant, 13) * (1 << dshift);
            // decoder: output = prediction - residual_from_bitstream
            // encoder: residual_to_write = prediction - actual_output
            int actual = decoded[filter_order + out_idx];
            residuals_out[out_idx] = static_cast<int32_t>(
                static_cast<unsigned>(prediction) - static_cast<unsigned>(actual));

            // The decoder stores the DECODED output, not the residual
            int decoded_val = static_cast<int32_t>(
                static_cast<unsigned>(prediction) - static_cast<unsigned>(residuals_out[out_idx]));
            residues[filter_order + i] = static_cast<int16_t>(decoded_val >> dshift);
            out_idx++;
        }
        x -= tmp;
        if (x > 0)
            std::memmove(residues.data(), residues.data() + y, 2 * filter_order);
    }
}

// ============================================================================
// Encoder: write residues
// ============================================================================

static void encode_residues(const int32_t* data, int length, BitStreamWriter& fw) {
    int best_mode = 1;
    int best_cost = Encoder::calc_bits_needed(1, data, length);
    for (int m = 2; m <= 50; m++) {
        int c = Encoder::calc_bits_needed(m, data, length);
        if (c < best_cost) { best_cost = c; best_mode = m; }
    }
    fw.write_bit(0);
    fw.write_bits(best_mode, 6);
    Encoder::encode_segment(best_mode, data, length, fw);
}

// ============================================================================
// Try encoding a subframe with predictor filter, return estimated cost
// ============================================================================

struct FilterConfig {
    int order_index;  // index into predictor_sizes
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

static bool try_filter_encode(const int32_t* samples, int subframe_size,
                              int order_idx, FilterConfig& cfg) {
    int filter_order = predictor_sizes[order_idx];
    if (subframe_size <= filter_order) return false;

    cfg.order_index = order_idx;
    cfg.filter_order = filter_order;
    cfg.filter_quant = 10;

    int32_t max_abs = 0;
    for (int i = 0; i < subframe_size; i++) {
        int32_t a = std::abs(samples[i]);
        if (a > max_abs) max_abs = a;
    }
    cfg.dshift = 0;
    while (max_abs > 32767) {
        max_abs >>= 1;
        cfg.dshift++;
    }
    cfg.size = 6;
    cfg.warmup_lpc_mode = 0;

    // Compute autocorrelation
    double r[257];
    compute_autocorrelation(samples, subframe_size, r, filter_order);

    // Levinson-Durbin for PARCOR coefficients
    std::vector<double> parcor(filter_order);
    if (!levinson_durbin(r, filter_order, parcor.data())) return false;

    // Quantize PARCOR to predictors format
    // predictors[0..1]: 10-bit signed (range [-512, 511])
    // predictors[2..3]: 'size'-bit signed, stored * (1 << (10-size))
    //   For size=6: 6-bit signed [-32,31], stored * 16
    // predictors[4+]: variable-bit signed, stored * (1 << (10-size))
    cfg.predictors.resize(filter_order);
    for (int i = 0; i < filter_order; i++) {
        double k = parcor[i];
        int q;
        if (i < 2) {
            // 10-bit: range [-512, 511], representing k * ~512
            q = static_cast<int>(round(k * 512.0));
            q = std::clamp(q, -512, 511);
        } else {
            // 'size'-bit: the decoder reads get_sbits(size) * (1 << (10-size))
            // For size=6: range [-32, 31] * 16 = [-512, 496], step 16
            int shift = 10 - cfg.size;
            int max_val = (1 << (cfg.size - 1)) - 1;  // 31 for size=6
            int min_val = -(1 << (cfg.size - 1));      // -32 for size=6
            int raw = static_cast<int>(round(k * 512.0 / (1 << shift)));
            raw = std::clamp(raw, min_val, max_val);
            q = raw * (1 << shift);
        }
        cfg.predictors[i] = q;
    }

    // Build filter from quantized predictors
    int16_t filter[256];
    build_filter(cfg.predictors.data(), filter_order, cfg.filter_quant, filter);

    // Compute prediction residuals
    cfg.filter_residuals.resize(subframe_size - filter_order);
    compute_filter_residuals(samples, subframe_size, filter, filter_order,
                              cfg.filter_quant, cfg.dshift, cfg.filter_residuals.data());

    // Debug: print PARCOR and first predictions
    {
        static int pcor_dbg = 0;
        if (pcor_dbg < 1) {
            std::cerr << "  PARCOR: ";
            for (int i = 0; i < std::min(filter_order, 4); i++)
                std::cerr << parcor[i] << " ";
            std::cerr << "\n  Quantized: ";
            for (int i = 0; i < std::min(filter_order, 4); i++)
                std::cerr << cfg.predictors[i] << " ";
            std::cerr << "\n  Warmup: ";
            for (int i = 0; i < std::min(filter_order, 4); i++)
                std::cerr << samples[i] << " ";
            std::cerr << "\n  First 8 actual vs residual: ";
            for (int i = 0; i < std::min(8, subframe_size - filter_order); i++)
                std::cerr << samples[filter_order+i] << "/" << cfg.filter_residuals[i] << " ";
            std::cerr << "\n";
            pcor_dbg++;
        }
    }

    // Estimate cost of warmup (raw samples)
    cfg.warmup_residuals.assign(samples, samples + filter_order);

    // Total bit cost estimate:
    // 1 (filter flag) + 4 (order) + 1 (new filter) + 2 (warmup lpc) +
    // warmup_bits + 1 (dshift=0) + 1 (size) + 1 (quant=default) +
    // 2*10 (predictors 0-1) + 2*size (predictors 2-3) + filter_residual_bits
    int overhead = 1 + 4 + 1 + 2 + 1 + 1 + 1 + 20 + 2 * cfg.size;
    if (filter_order > 4) {
        // Additional predictor bits for orders > 4
        overhead += 1; // tmp flag
        for (int i = 4; i < filter_order; i++) {
            if ((i & 3) == 0) overhead += 2;
            overhead += cfg.size; // approximate
        }
    }

    // Cost of warmup
    int warmup_cost = 0;
    {
        int best = Encoder::calc_bits_needed(1, cfg.warmup_residuals.data(), filter_order);
        for (int m = 2; m <= 50; m++) {
            int c = Encoder::calc_bits_needed(m, cfg.warmup_residuals.data(), filter_order);
            if (c < best) best = c;
        }
        warmup_cost = 1 + 6 + best; // flag + mode bits + data
    }

    // Cost of filter residuals
    int resid_cost = 0;
    {
        int best = Encoder::calc_bits_needed(1, cfg.filter_residuals.data(),
                                              subframe_size - filter_order);
        for (int m = 2; m <= 50; m++) {
            int c = Encoder::calc_bits_needed(m, cfg.filter_residuals.data(),
                                               subframe_size - filter_order);
            if (c < best) best = c;
        }
        resid_cost = 1 + 6 + best;
    }

    cfg.total_bits = overhead + warmup_cost + resid_cost;
    return true;
}

struct SubframeChoice {
    bool use_filter;
    FilterConfig filter;
    int nofilter_cost;
    int total_bits;
};

static SubframeChoice evaluate_subframe(const int32_t* subframe_data, int subframe_size) {
    SubframeChoice choice = {};
    choice.use_filter = false;

    // Estimate cost of no-filter path
    int best = Encoder::calc_bits_needed(1, subframe_data, subframe_size);
    for (int m = 2; m <= 50; m++) {
        int c = Encoder::calc_bits_needed(m, subframe_data, subframe_size);
        if (c < best) best = c;
    }
    choice.nofilter_cost = 1 + 1 + 6 + best; // filter_flag(1) + segment_flag(1) + mode(6) + data
    choice.total_bits = choice.nofilter_cost;

    // Try predictor filter with different orders
    bool have_filter = false;
    FilterConfig best_filter;
    // Try orders: 4, 8, 12, 16 (indices 0-3)
    for (int idx = 0; idx < 4; idx++) {
        FilterConfig cfg;
        if (try_filter_encode(subframe_data, subframe_size, idx, cfg)) {
            if (!have_filter || cfg.total_bits < best_filter.total_bits) {
                best_filter = cfg;
                have_filter = true;
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

static void write_subframe(const SubframeChoice& choice, const int32_t* subframe_data,
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
        
        // size = 6 (write 0)
        fw.write_bit(0);
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

static void encode_channel(const int32_t* samples, int nb_samples, int bps,
                           int lpc_mode, int sample_rate, BitStreamWriter& fw) {
    int subframe_size = nb_samples - 1;
    const int32_t* subframe_data = samples + 1;

    // Evaluate 1 subframe
    SubframeChoice c1 = evaluate_subframe(subframe_data, subframe_size);
    int best_cost = c1.total_bits;
    int best_splits = 1;
    std::vector<SubframeChoice> best_choices = { c1 };
    std::vector<int> best_lens = { subframe_size };
    std::vector<int> best_vs;

    // Evaluate 2 subframes
    int base_align = (((sample_rate + 511) >> 9) + 3) & ~3;
    int subframe_scale = base_align << 1;
    
    if (subframe_size > subframe_scale * 4) {
        int v_mid = (subframe_size / 2) / subframe_scale;
        int len1 = v_mid * subframe_scale;
        int len2 = subframe_size - len1;
        if (len1 > 0 && len2 > 0) {
            SubframeChoice c2_1 = evaluate_subframe(subframe_data, len1);
            SubframeChoice c2_2 = evaluate_subframe(subframe_data + len1, len2);
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
            SubframeChoice c4_1 = evaluate_subframe(subframe_data, l1);
            SubframeChoice c4_2 = evaluate_subframe(subframe_data + l1, l2);
            SubframeChoice c4_3 = evaluate_subframe(subframe_data + l1 + l2, l3);
            SubframeChoice c4_4 = evaluate_subframe(subframe_data + l1 + l2 + l3, l4);
            int cost4 = c4_1.total_bits + c4_2.total_bits + c4_3.total_bits + c4_4.total_bits + 18; // +18 bits for v
            if (cost4 < best_cost) {
                best_cost = cost4;
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
    
    static int dbg = 0;
    if (dbg < 4) {
        std::cerr << "  splits=" << best_splits << " vs=";
        for (int v : best_vs) std::cerr << v << ",";
        std::cerr << " lengths=";
        for (int l : best_lens) std::cerr << l << ",";
        std::cerr << "\n";
        dbg++;
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

// ============================================================================
// Main encode_file
// ============================================================================

void Encoder::encode_file(const char* wav_path, const char* tak_path) {
    std::ifstream is(wav_path, std::ios::binary);
    if (!is) throw std::runtime_error("Could not open WAV file");

    WavInfo wav = read_wav_header(is);
    int channels = wav.channels;
    int bps = wav.bps;
    int sample_rate = wav.sample_rate;
    int block_align = channels * (bps / 8);
    int total_samples = wav.data_size / block_align;

    std::ofstream os(tak_path, std::ios::binary);
    if (!os) throw std::runtime_error("Could not open TAK file for writing");

    os.write("tBaK", 4);

    // StreamInfo metadata
    BitStreamWriter si_gb;
    si_gb.write_bits(static_cast<int>(takdecomp::CodecType::MonoStereo),
                     takdecomp::constants::ENCODER_CODEC_BITS);
    si_gb.write_bits(0, takdecomp::constants::ENCODER_PROFILE_BITS);
    si_gb.write_bits(static_cast<int>(takdecomp::FrameSizeType::Fs4096),
                     takdecomp::constants::SIZE_FRAME_DURATION_BITS);
    si_gb.write_bits64(total_samples, takdecomp::constants::SIZE_SAMPLES_NUM_BITS);
    si_gb.write_bits(0, takdecomp::constants::FORMAT_DATA_TYPE_BITS);
    si_gb.write_bits(sample_rate - takdecomp::constants::SAMPLE_RATE_MIN,
                     takdecomp::constants::FORMAT_SAMPLE_RATE_BITS);
    si_gb.write_bits(bps - takdecomp::constants::BPS_MIN,
                     takdecomp::constants::FORMAT_BPS_BITS);
    si_gb.write_bits(channels - takdecomp::constants::CHANNELS_MIN,
                     takdecomp::constants::FORMAT_CHANNEL_BITS);
    si_gb.write_bit(0);
    si_gb.align_write_bits();

    os.write("\x01", 1);
    int si_len = si_gb.get_position_bytes();
    int md_block_size = si_len + 3;
    uint8_t size_le[3] = {
        static_cast<uint8_t>(md_block_size & 0xff),
        static_cast<uint8_t>((md_block_size >> 8) & 0xff),
        static_cast<uint8_t>((md_block_size >> 16) & 0xff)
    };
    os.write(reinterpret_cast<char*>(size_le), 3);
    os.write(reinterpret_cast<const char*>(si_gb.get_data().data()), si_len);
    uint32_t si_crc = takdecomp::compute_crc24(si_gb.get_data().data(), si_len);
    uint8_t crc_le[3] = {
        static_cast<uint8_t>(si_crc & 0xff),
        static_cast<uint8_t>((si_crc >> 8) & 0xff),
        static_cast<uint8_t>((si_crc >> 16) & 0xff)
    };
    os.write(reinterpret_cast<char*>(crc_le), 3);
    os.write("\x00\x00\x00\x00", 4);

    // Encode frames
    int frame_samples = 4096;
    int remaining_samples = total_samples;
    int frame_num = 0;
    Decorrelator decorr;

    while (remaining_samples > 0) {
        int current_frame_samples = std::min(frame_samples, remaining_samples);

        std::vector<int32_t> c1(current_frame_samples);
        std::vector<int32_t> c2(current_frame_samples);
        for (int i = 0; i < current_frame_samples; i++) {
            if (bps == 16) {
                if (channels == 2) {
                    int16_t l, r;
                    is.read(reinterpret_cast<char*>(&l), 2);
                    is.read(reinterpret_cast<char*>(&r), 2);
                    c1[i] = l; c2[i] = r;
                } else {
                    int16_t l;
                    is.read(reinterpret_cast<char*>(&l), 2);
                    c1[i] = l;
                }
            } else if (bps == 24) {
                if (channels == 2) {
                    uint8_t buf[6];
                    is.read(reinterpret_cast<char*>(buf), 6);
                    int32_t l = buf[0] | (buf[1] << 8) | (buf[2] << 16);
                    if (l & 0x800000) l |= 0xFF000000;
                    int32_t r = buf[3] | (buf[4] << 8) | (buf[5] << 16);
                    if (r & 0x800000) r |= 0xFF000000;
                    c1[i] = l; c2[i] = r;
                } else {
                    uint8_t buf[3];
                    is.read(reinterpret_cast<char*>(buf), 3);
                    int32_t l = buf[0] | (buf[1] << 8) | (buf[2] << 16);
                    if (l & 0x800000) l |= 0xFF000000;
                    c1[i] = l;
                }
            }
        }

        // Frame header
        BitStreamWriter fw;
        fw.write_bits(takdecomp::constants::FRAME_HEADER_SYNC_ID,
                      takdecomp::constants::FRAME_HEADER_SYNC_ID_BITS);
        int flags = 0;
        if (remaining_samples <= frame_samples)
            flags |= takdecomp::constants::FRAME_FLAG_IS_LAST;
        fw.write_bits(flags, takdecomp::constants::FRAME_HEADER_FLAGS_BITS);
        fw.write_bits(frame_num, takdecomp::constants::FRAME_HEADER_NO_BITS);
        if (flags & takdecomp::constants::FRAME_FLAG_IS_LAST) {
            fw.write_bits(current_frame_samples - 1,
                          takdecomp::constants::FRAME_HEADER_SAMPLE_COUNT_BITS);
            fw.write_bits(0, 2);
        }
        fw.align_write_bits();
        int header_bytes = fw.get_position_bytes();
        uint32_t header_crc = takdecomp::compute_crc24(fw.get_data().data(), header_bytes);
        fw.write_bits((header_crc >> 16) & 0xff, 8);
        fw.write_bits((header_crc >> 8) & 0xff, 8);
        fw.write_bits(header_crc & 0xff, 8);

        // Choose LPC mode
        int lpc_mode_c1 = 0, lpc_mode_c2 = 0;
        if (current_frame_samples >= 16) {
            int costs[4];
            for (int m = 0; m < 4; m++) costs[m] = estimate_lpc_cost(c1.data(), current_frame_samples, m);
            lpc_mode_c1 = 0;
            for (int m = 1; m < 4; m++) if (costs[m] < costs[lpc_mode_c1]) lpc_mode_c1 = m;

            if (channels == 2) {
                for (int m = 0; m < 4; m++) costs[m] = estimate_lpc_cost(c2.data(), current_frame_samples, m);
                lpc_mode_c2 = 0;
                for (int m = 1; m < 4; m++) if (costs[m] < costs[lpc_mode_c2]) lpc_mode_c2 = m;
            }
        }

        // Apply inverse LPC
        inverse_lpc(c1.data(), lpc_mode_c1, current_frame_samples);
        if (channels == 2) inverse_lpc(c2.data(), lpc_mode_c2, current_frame_samples);

        // Apply inverse decorrelation
        int best_dmode = 0;
        int32_t pre_c1 = c1[0], pre_c2 = (channels == 2) ? c2[0] : 0;
        if (channels == 2) {
            best_dmode = decorr.apply_decorrelation(c1.data(), c2.data(), current_frame_samples);
            c1[0] = pre_c1; c2[0] = pre_c2;
        }

        // Encode channels
        for (int ch = 0; ch < channels; ch++) {
            const int32_t* d = (ch == 0) ? c1.data() : c2.data();
            int lpc = (ch == 0) ? lpc_mode_c1 : lpc_mode_c2;
            encode_channel(d, current_frame_samples, bps, lpc, sample_rate, fw);
        }

        // Stereo decorrelation info
        if (channels == 2) {
            fw.write_bit(0);
            fw.write_bits(best_dmode, 3);
        }

        // Final sample_shift
        for (int ch = 0; ch < channels; ch++) fw.write_bit(0);

        // Frame tail
        fw.align_write_bits();
        fw.write_bits(0, 24);

        os.write(reinterpret_cast<const char*>(fw.get_data().data()),
                 fw.get_position_bytes());
        remaining_samples -= current_frame_samples;
        frame_num++;
    }
}

} // namespace takenc
