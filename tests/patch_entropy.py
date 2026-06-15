import sys

with open('src/core/tak_encoder_entropy.cpp', 'r') as f:
    text = f.read()

target = """                while (x_init >= code.escape) {
                    int base_x = next_bit ? (x_init | limit) : x_init;
                    if (base_x >= code.aescape) {
                        int num = x + code.escape - base_x;"""

replacement = """                while (x_init >= code.escape) {
                    if (next_bit == 0) {
                        if (x_init == x) {
                            int l = code.init + 1;
                            if (l < best_len) {
                                best_len = l; best_x_init = x_init; best_next = 0; best_scale = -1; best_s2 = -1;
                            }
                        }
                    } else {
                        int base_x = x_init | limit;
                        if (base_x >= code.aescape) {
                            int num = x + code.escape - base_x;"""

if target in text:
    text = text.replace(target, replacement)
    
    target2 = """                            if (l < best_len) {
                                best_len = l; best_x_init = x_init; best_next = next_bit; best_scale = scale; best_s2 = s2;
                            }
                            break;
                        }
                    }
                    x_init -= code.scale;
                }"""
    
    replacement2 = """                            if (l < best_len) {
                                best_len = l; best_x_init = x_init; best_next = next_bit; best_scale = scale; best_s2 = s2;
                            }
                            break;
                        }
                        } else {
                            if (base_x - code.escape == x) {
                                int l = code.init + 2;
                                if (l < best_len) {
                                    best_len = l; best_x_init = x_init; best_next = 1; best_scale = -1; best_s2 = -1;
                                }
                            }
                        }
                    }
                    x_init -= code.scale;
                }"""
    if target2 in text:
        text = text.replace(target2, replacement2)
        with open('src/core/tak_encoder_entropy.cpp', 'w') as f:
            f.write(text)
        print("PATCH SUCCESSFUL")
    else:
        print("TARGET2 NOT FOUND")
else:
    print("TARGET NOT FOUND")
