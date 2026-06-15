#include <stdio.h>
#include <stdint.h>
#include <libavformat/avformat.h>

extern AVInputFormat ff_tak_demuxer;

int main() {
    AVFormatContext *ctx = avformat_alloc_context();
    int ret = avformat_open_input(&ctx, "tests/our_fixed.tak", NULL, NULL);
    printf("ret = %d\n", ret);
    return 0;
}
