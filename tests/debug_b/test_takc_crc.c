#include <stdio.h>
#include <stdint.h>
#include <libavutil/crc.h>

int main() {
    FILE *f = fopen("../../my.tak", "rb"); // wait, my.tak or short_takc.tak?
    fclose(f);
    return 0;
}
