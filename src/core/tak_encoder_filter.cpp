#include "tak_encoder/filter.hpp"
#include <cmath>
#include "tak_encoder/encoder.hpp"
#include <cmath>
#include <algorithm>
#include <vector>
#include <cstring>
#include <array>

namespace takenc {
    static int32_t clip_intp2(int32_t a, int p) {
        if (((static_cast<unsigned>(a) + (1 << p)) & ~((2U << p) - 1)) != 0u)
            return (a < 0 ? -1 : 0) ^ ((1 << p) - 1);
        return a;
    }

    // Compute autocorrelation r[0..order]
    static void compute_autocorrelation(const int32_t *data, int len, double *r, int order) {
        for (int k = 0; k <= order; k++) {
            double sum = 0;
            for (int i = k; i < len; i++)
                sum += static_cast<double>(data[i]) * data[i - k];
            r[k] = sum;
        }
    }

    // Levinson-Durbin: compute PARCOR (reflection) coefficients from autocorrelation
    static bool levinson_durbin(const double *r, int order, double *parcor) {
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
            if (E <= 0.0 || std::isnan(E)) return false;
            std::copy(a.begin(), a.end(), a_prev.begin());
        }
        return true;
    }

    // Build the filter from quantized predictors (exactly matches decoder)
    static void build_filter(const int *predictors, int filter_order, int filter_quant,
                             int16_t *filter_out) {
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
        if (filter_order % 2 != 0) {
            int m = filter_order / 2;
            filter_out[m] = static_cast<int16_t>(x - ((tfilter[m] + y) >> (15 - filter_quant)));
        }
    }

    // Compute residuals and update history, perfectly matching the decoder loop
    static void compute_filter_residuals(const int32_t *decoded, int subframe_size,
                                         const int16_t *filter, int filter_order,
                                         int filter_quant, int dshift,
                                         int32_t *residuals_out) {
        std::array<int16_t, 544> residues{};
        for (int i = 0; i < filter_order; i++)
            residues[i] = static_cast<int16_t>(decoded[i] >> dshift);

        int y = 544 - filter_order;
        int x = subframe_size - filter_order;
        int out_idx = 0;

        while (x > 0) {
            int tmp = std::min(y, x);
            for (int i = 0; i < tmp; i++) {
                int32_t v = 1 << (filter_quant - 1);
                for (int j = 0; j < filter_order; j++) {
                    v += residues[i + j] * static_cast<unsigned>(filter[j]);
                }
                int32_t pred = clip_intp2(v >> filter_quant, 13) * (1 << dshift);
                int32_t actual = decoded[filter_order + out_idx];
                residuals_out[out_idx] = pred - actual;
                residues[filter_order + i] = static_cast<int16_t>(actual >> dshift);
                out_idx++;
            }
            x -= tmp;
            if (x > 0) {
                std::memmove(residues.data(), residues.data() + y, 2 * filter_order);
            }
        }
    }

    void inverse_lpc(int32_t *data, int mode, int length) {
        if (mode == 0 || length < 2) return;
        if (mode == 1) {
            for (int i = length - 1; i >= 1; i--)
                data[i] = (int32_t) ((uint32_t) data[i] - (uint32_t) data[i - 1]);
        } else if (mode == 2) {
            for (int i = length - 1; i >= 2; i--)
                data[i] = (int32_t) ((uint32_t) data[i] - 2u * (uint32_t) data[i - 1] + (uint32_t) data[i - 2]);
            data[1] = (int32_t) ((uint32_t) data[1] - (uint32_t) data[0]);
        } else if (mode == 3) {
            for (int i = length - 1; i >= 3; i--)
                data[i] = (int32_t) ((uint32_t) data[i] - 3u * (uint32_t) data[i - 1] + 3u * (uint32_t) data[i - 2] - (
                                         uint32_t) data[i - 3]);
            if (length > 2) data[2] = (int32_t) ((uint32_t) data[2] - 2u * (uint32_t) data[1] + (uint32_t) data[0]);
            data[1] = (int32_t) ((uint32_t) data[1] - (uint32_t) data[0]);
        }
    }

    int estimate_lpc_cost(const int32_t *samples, int length, int lpc_mode) {
        std::vector<int32_t> tmp(samples, samples + length);
        inverse_lpc(tmp.data(), lpc_mode, length);
        int best = Encoder::calc_bits_needed(1, tmp.data() + 1, length - 1);
        for (int m = 2; m <= 34; m++) {
            int c = Encoder::calc_bits_needed(m, tmp.data() + 1, length - 1);
            if (c < best) best = c;
        }
        return best;
    }


    static int evaluate_filter_cost(FilterConfig &cfg, const int32_t *samples, int subframe_size, int filter_order) {
        int overhead = 1 + 4 + 1 + 2 + 1 + 1 + 1 + 20 + 2 * cfg.size;
        if (filter_order > 4) {
            overhead += 1;
            for (int i = 4; i < filter_order; i++) {
                if ((i & 3) == 0) overhead += 2;
                overhead += cfg.size;
            }
        }

        cfg.warmup_residuals.assign(samples, samples + filter_order);
        cfg.warmup_lpc_mode = 0;
        int warmup_cost = 0; {
            int best = Encoder::calc_bits_needed(1, cfg.warmup_residuals.data(), filter_order);
            for (int m = 2; m <= 34; m++) {
                int c = Encoder::calc_bits_needed(m, cfg.warmup_residuals.data(), filter_order);
                if (c < best) best = c;
            }
            warmup_cost = 1 + 6 + best;
        }

        int resid_cost = 0; {
            int best = Encoder::calc_bits_needed(1, cfg.filter_residuals.data(), subframe_size - filter_order);
            for (int m = 2; m <= 34; m++) {
                int c = Encoder::calc_bits_needed(m, cfg.filter_residuals.data(), subframe_size - filter_order);
                if (c < best) best = c;
            }
            resid_cost = 1 + 6 + best;
        }

        return overhead + warmup_cost + resid_cost;
    }

    bool try_filter_encode(const int32_t *samples, int subframe_size,
                           int order_idx, FilterConfig &cfg, bool max_compression) {
        int filter_order = predictor_sizes[order_idx];
        if (subframe_size <= filter_order) return false;

        cfg.order_index = order_idx;
        cfg.filter_order = filter_order;
        cfg.filter_quant = 10;

        uint32_t max_abs = 0;
        for (int i = 0; i < subframe_size; i++) {
            uint32_t a = std::abs(samples[i]);
            if (a > max_abs) max_abs = a;
        }
        cfg.dshift = 0;
        while (max_abs > 32767) {
            max_abs >>= 1;
            cfg.dshift++;
        }
        cfg.size = 6;
        cfg.warmup_lpc_mode = 0;

        // scale down 24-bit samples before autocorrelation to avoid double precision loss of significance.
        // the levinson-durbin algorithm is scale-invariant, so the parcor coefficients remain mathematically identical.
        // without this dynamic downshift, intermediate sums would exceed 53 bits of precision,
        // causing quantized coefficients to drift by ±1 and corrupting the decoded pcm.
        std::vector<int32_t> shifted(subframe_size);
        for (int i = 0; i < subframe_size; i++)
            shifted[i] = samples[i] >> cfg.dshift;

        std::vector<double> r(filter_order + 1);
        compute_autocorrelation(shifted.data(), subframe_size, r.data(), filter_order);


        std::vector<double> parcor(filter_order);
        if (!levinson_durbin(r.data(), filter_order, parcor.data())) {
            return false;
        }

        cfg.predictors.resize(std::max(4, filter_order));

        auto init_predictors = [&](int size_val) {
            for (int i = 0; i < filter_order; i++) {
                double k = parcor[i];
                int q;
                if (i < 2) {
                    q = static_cast<int>(std::round(k * 512.0));
                    q = std::clamp(q, -512, 511);
                } else {
                    int shift = 10 - size_val;
                    int max_val = (1 << (size_val - 1)) - 1;
                    int min_val = -(1 << (size_val - 1));
                    int raw = static_cast<int>(std::round(k * 512.0 / (1 << shift)));
                    raw = std::clamp(raw, min_val, max_val);
                    q = raw * (1 << shift);
                }
                cfg.predictors[i] = q;
            }
            for (int i = filter_order; i < 4; i++) cfg.predictors[i] = 0;
        };

        auto evaluate_configuration = [&]() {
            int16_t filter[256];
            build_filter(cfg.predictors.data(), filter_order, cfg.filter_quant, filter);
            cfg.filter_residuals.resize(subframe_size - filter_order);
            compute_filter_residuals(samples, subframe_size, filter, filter_order,
                                     cfg.filter_quant, cfg.dshift, cfg.filter_residuals.data());
            return evaluate_filter_cost(cfg, samples, subframe_size, filter_order);
        };

        init_predictors(cfg.size);
        cfg.total_bits = evaluate_configuration();

        if (max_compression) {
            // Try size 7
            FilterConfig best_cfg = cfg;

            cfg.size = 7;
            init_predictors(7);
            int cost7 = evaluate_configuration();
            if (cost7 < best_cfg.total_bits) {
                cfg.total_bits = cost7;
                best_cfg = cfg;
            } else {
                cfg = best_cfg;
            }

            // Coordinate descent on first 16 coefficients
            int N = std::min(filter_order, 16);
            bool improved = true;
            for (int pass = 0; pass < 2 && improved; pass++) {
                improved = false;
                for (int i = 0; i < N; i++) {
                    int orig_q = cfg.predictors[i];
                    int step = (i < 2) ? 1 : (1 << (10 - cfg.size));
                    int min_val, max_val;
                    if (i < 2) {
                        min_val = -512;
                        max_val = 511;
                    } else {
                        min_val = -(1 << (cfg.size - 1)) * (1 << (10 - cfg.size));
                        max_val = ((1 << (cfg.size - 1)) - 1) * (1 << (10 - cfg.size));
                    }

                    int best_q = orig_q;
                    int best_cost = cfg.total_bits;

                    for (int dir: {-1, 1}) {
                        int test_q = orig_q + dir * step;
                        if (test_q < min_val || test_q > max_val) continue;

                        cfg.predictors[i] = test_q;
                        int test_cost = evaluate_configuration();

                        if (test_cost < best_cost) {
                            best_cost = test_cost;
                            best_q = test_q;
                        }
                    }

                    if (best_q != orig_q) {
                        cfg.predictors[i] = best_q;
                        cfg.total_bits = best_cost;
                        improved = true;
                    } else {
                        cfg.predictors[i] = orig_q;
                    }
                }
            }

            // Ensure final state is correct
            evaluate_configuration();
        }

        return true;
    }
} // namespace takenc
