import sys

with open('src/core/tak_encoder_subframe.cpp', 'r') as f:
    text = f.read()

target = """        if (cfg.test_filters) {
            int max_idx = std::min(cfg.max_filter_order_idx, 14); // 14 is max predictor_sizes index
            for (int idx = 0; idx <= max_idx; idx++) {
                FilterConfig fcfg;
                if (try_filter_encode(subframe_data, subframe_size, idx, fcfg, cfg.max_compression)) {"""

replacement = """        if (cfg.test_filters) {
            int max_order = std::min(cfg.max_filter_order_idx + 1, 16);
            for (int order = 1; order <= max_order; order++) {
                FilterConfig fcfg;
                if (try_filter_encode(subframe_data, subframe_size, order, fcfg, cfg.max_compression)) {"""

if target in text:
    text = text.replace(target, replacement)
    with open('src/core/tak_encoder_subframe.cpp', 'w') as f:
        f.write(text)
    print("PATCH 1 SUCCESSFUL")
else:
    print("TARGET 1 NOT FOUND")

with open('src/core/tak_encoder_filter.cpp', 'r') as f:
    text = f.read()

target = """    bool try_filter_encode(const int32_t *samples, int subframe_size,
                           int order_idx, FilterConfig &cfg, bool max_compression) {
        int filter_order = predictor_sizes[order_idx];
        if (subframe_size <= filter_order) return false;

        cfg.order_index = order_idx;
        cfg.filter_order = filter_order;"""

replacement = """    bool try_filter_encode(const int32_t *samples, int subframe_size,
                           int filter_order, FilterConfig &cfg, bool max_compression) {
        if (subframe_size <= filter_order) return false;

        cfg.order_index = filter_order - 1;
        cfg.filter_order = filter_order;"""

if target in text:
    text = text.replace(target, replacement)
    with open('src/core/tak_encoder_filter.cpp', 'w') as f:
        f.write(text)
    print("PATCH 2 SUCCESSFUL")
else:
    print("TARGET 2 NOT FOUND")
