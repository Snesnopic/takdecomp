mono:
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

filt:
bool try_filter_encode(const int32_t* samples, int subframe_size,
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

    std::vector<double> r(filter_order + 1);
    compute_autocorrelation(samples, subframe_size, r.data(), filter_order);

    std::vector<double> parcor(filter_order);
    if (!levinson_durbin(r.data(), filter_order, parcor.data())) {
        return false;
    }

    cfg.predictors.resize(filter_order);
    for (int i = 0; i < filter_order; i++) {
        double k = parcor[i];
        int q;
        if (i < 2) {
            // 10-bit: range [-512, 511], representing k * ~512
            q = static_cast<int>(std::round(k * 512.0));
            q = std::clamp(q, -512, 511);
        } else {
            // 'size'-bit: the decoder reads get_sbits(size) * (1 << (10-size))
            // For size=6: range [-32, 31] * 16 = [-512, 496], step 16
            int shift = 10 - cfg.size;
            int max_val = (1 << (cfg.size - 1)) - 1;  // 31 for size=6
            int min_val = -(1 << (cfg.size - 1));      // -32 for size=6
            int raw = static_cast<int>(std::round(k * 512.0 / (1 << shift)));
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

    // Cost of overhead (predictors + sizes)
    int overhead = 1 + 4 + 2 + 1 + 1 + 1 + 20 + (2 * cfg.size);
    if (filter_order > 4) {
        overhead += 1;
        for (int i = 4; i < filter_order; i++) {
            if ((i & 3) == 0) overhead += 2;
            overhead += cfg.size; // approximate
        }
    }

    // Try applying LPC on warmup samples to save bits
    cfg.warmup_residuals.assign(samples, samples + filter_order);
    cfg.warmup_lpc_mode = 0;

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