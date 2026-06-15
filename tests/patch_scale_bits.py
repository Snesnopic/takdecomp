import re

with open("src/core/tak_encoder_entropy.cpp", "r") as f:
    text = f.read()

# Fix calc_bits_needed
new_text = text.replace("""                if (scale == 0) {
                    bits += 3;
                } else {
                    int scale_bits = 0;
                    uint32_t s_copy = scale;
                    while (s_copy > 0) {
                        scale_bits++;
                        s_copy >>= 1;
                    }

                    if (scale_bits <= 6) {
                        bits += 3;
                        bits += scale_bits - 1;
                    } else if (scale_bits <= 29) {
                        bits += 3 + 5;
                        bits += scale_bits - 1;
                    } else {
                        // Error, but whatever
                    }
                }""", """                if (scale == 0) {
                    bits += 3;
                } else {
                    int scale_bits = 1;
                    uint32_t s_copy = scale - 1;
                    while (s_copy > 0) {
                        scale_bits++;
                        s_copy >>= 1;
                    }
                    scale_bits--;
                    if (scale_bits == 0) scale_bits = 1;

                    if (scale_bits <= 6) {
                        bits += 3;
                        bits += scale_bits;
                    } else if (scale_bits <= 29) {
                        bits += 3 + 5;
                        bits += scale_bits;
                    } else {
                        // Error, but whatever
                    }
                }""")

# Let's just fix encode_segment directly again
new_text2 = re.sub(r'if \(scale == 0\) \{\s*bw\.write_bits\(0, 3\);\s*\} else \{\s*int scale_bits = 0;.*?bw\.write_bits\(scale - \(1U << \(scale_bits - 1\)\), scale_bits - 1\);\s*\}\s*\}\s*\}', r'''if (scale == 0) {
                    bw.write_bits(0, 3);
                } else {
                    int scale_bits = 1;
                    uint32_t s_copy = scale - 1;
                    while (s_copy > 0) {
                        scale_bits++;
                        s_copy >>= 1;
                    }
                    scale_bits--;
                    if (scale_bits == 0) scale_bits = 1;

                    if (scale_bits <= 6) {
                        bw.write_bits(scale_bits, 3);
                        bw.write_bits(scale - 1, scale_bits);
                    } else if (scale_bits <= 29) {
                        bw.write_bits(7, 3);
                        bw.write_bits(scale_bits, 5);
                        bw.write_bits(scale - 1, scale_bits);
                    } else {
                        // Error, but whatever
                    }
                }
            }''', new_text, flags=re.DOTALL)

with open("src/core/tak_encoder_entropy.cpp", "w") as f:
    f.write(new_text2)

