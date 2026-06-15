#include <stdio.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

int main(int argc, char **argv) {
    av_log_set_level(AV_LOG_TRACE);
    AVFormatContext *fmt_ctx = NULL;
    int ret = avformat_open_input(&fmt_ctx, "tests/our_fixed.tak", NULL, NULL);
    printf("avformat_open_input returned: %d\n", ret);
    return 0;
}
