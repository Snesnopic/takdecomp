import sys

with open('src/core/tak_encoder.cpp', 'r') as f:
    text = f.read()

target = """            Decorrelator::DecorrelationResult dmode_res = {0, 0, 0, 0, {}};
            if (channels == 2) {
                dmode_res = decorr.apply_decorrelation(c[0].data(), c[1].data(), current_frame_samples);
            }

            if (channels == 2) {
                if (dmode_res.mode == 0) {
                    fw.write_bit(0);
                } else {
                    fw.write_bit(1);
                    fw.write_bits(dmode_res.mode - 1, 2);
                    if (dmode_res.mode >= 4 && dmode_res.mode <= 5) {
                        if (dmode_res.shift > 0) {
                            fw.write_bit(1);
                            fw.write_bits(dmode_res.shift - 1, 4);
                        } else { fw.write_bit(0); }
                        fw.write_bits(dmode_res.factor & 0x3FF, 10);
                    } else if (dmode_res.mode >= 6) {
                        if (dmode_res.shift > 0) {
                            fw.write_bit(1);
                            fw.write_bits(dmode_res.shift - 1, 4);
                        } else { fw.write_bit(0); }
                        fw.write_bit(dmode_res.filter_order == 16 ? 1 : 0);
                        fw.write_bit(0);
                        for (int i = 0; i < dmode_res.filter_order; i += 4) {
                            int max_val = 0;
                            for (int j = 0; j < 4; j++) max_val = std::max(max_val, std::abs(dmode_res.filter[i + j]));
                            int code_size = 0;
                            while ((1 << code_size) <= max_val && code_size < 14) code_size++;
                            if (code_size > 0) code_size++;
                            if (code_size < 7) code_size = 7;
                            fw.write_bits(14 - code_size, 3);
                            for (int j = 0; j < 4; j++) {
                                fw.write_bits(dmode_res.filter[i + j] & ((1 << code_size) - 1), code_size);
                            }
                        }
                    }
                }
            } else if (channels > 2) {
                fw.write_bit(0);
            }

            for (int ch = 0; ch < channels; ch++) {
                encode_channel(c[ch].data(), current_frame_samples, bps, lpc_mode[ch], sample_rate, cfg, fw);
            }"""

replacement = """            Decorrelator::DecorrelationResult dmode_res = {0, 0, 0, 0, {}};
            if (channels == 2) {
                dmode_res = decorr.apply_decorrelation(c[0].data(), c[1].data(), current_frame_samples);
            }

            if (channels > 2) fw.write_bit(0);

            for (int ch = 0; ch < channels; ch++) {
                encode_channel(c[ch].data(), current_frame_samples, bps, lpc_mode[ch], sample_rate, cfg, fw);
            }

            if (channels == 2) {
                fw.write_bit(0);
                fw.write_bits(dmode_res.mode, 3);
                if (dmode_res.mode >= 4 && dmode_res.mode <= 5) {
                    if (dmode_res.shift > 0) {
                        fw.write_bit(1);
                        fw.write_bits(dmode_res.shift - 1, 4);
                    } else { fw.write_bit(0); }
                    fw.write_bits(dmode_res.factor & 0x3FF, 10);
                } else if (dmode_res.mode >= 6) {
                    if (dmode_res.shift > 0) {
                        fw.write_bit(1);
                        fw.write_bits(dmode_res.shift - 1, 4);
                    } else { fw.write_bit(0); }
                    fw.write_bit(dmode_res.filter_order == 16 ? 1 : 0);
                    fw.write_bit(0);
                    for (int i = 0; i < dmode_res.filter_order; i += 4) {
                        int max_val = 0;
                        for (int j = 0; j < 4; j++) max_val = std::max(max_val, std::abs(dmode_res.filter[i + j]));
                        int code_size = 0;
                        while ((1 << code_size) <= max_val && code_size < 14) code_size++;
                        if (code_size > 0) code_size++;
                        if (code_size < 7) code_size = 7;
                        fw.write_bits(14 - code_size, 3);
                        for (int j = 0; j < 4; j++) {
                            fw.write_bits(dmode_res.filter[i + j] & ((1 << code_size) - 1), code_size);
                        }
                    }
                }
            }"""

if target in text:
    text = text.replace(target, replacement)
    with open('src/core/tak_encoder.cpp', 'w') as f:
        f.write(text)
    print("PATCH SUCCESSFUL")
else:
    print("TARGET NOT FOUND")
