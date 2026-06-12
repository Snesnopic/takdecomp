mono:
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

sub:
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