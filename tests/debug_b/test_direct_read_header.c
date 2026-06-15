#include <stdio.h>
#include <stdint.h>
#include <libavformat/avformat.h>

extern AVInputFormat ff_tak_demuxer;

int main() {
    AVFormatContext *ctx = avformat_alloc_context();
    AVIOContext *pb;
    avio_open(&pb, "tests/our_fixed.tak", AVIO_FLAG_READ);
    ctx->pb = pb;
    ctx->iformat = &ff_tak_demuxer;
    int ret = ff_tak_demuxer.read_header(ctx);
    printf("read_header ret = %d\n", ret);
    return 0;
}
