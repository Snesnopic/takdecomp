import sys

with open('src/core/tak_encoder_subframe.cpp', 'r') as f:
    text = f.read()

target = """        if (best_splits > 1) {
            for (int v: best_vs) {
                fw.write_bits(v, 6);
            }
        }"""

replacement = """        if (best_splits > 1) {
            printf("best_splits=%d, vs=", best_splits);
            for (int v: best_vs) {
                printf("%d ", v);
                fw.write_bits(v, 6);
            }
            printf("\\n");
        }"""

if target in text:
    text = text.replace(target, replacement)
    with open('src/core/tak_encoder_subframe.cpp', 'w') as f:
        f.write(text)
    print("PATCH 2 SUCCESSFUL")
else:
    print("TARGET 2 NOT FOUND")
