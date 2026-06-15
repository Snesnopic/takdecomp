#include <stdio.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

int main(int argc, char **argv) {
    AVFormatContext *fmt_ctx = NULL;
    if (avformat_open_input(&fmt_ctx, "tests/our_fixed.tak", NULL, NULL) < 0) {
        printf("Failed to open input\n");
        return 1;
    }
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        printf("Failed to find stream info\n");
        return 1;
    }
    AVStream *st = fmt_ctx->streams[0];
    const AVCodec *codec = avcodec_find_decoder(st->codecpar->codec_id);
    AVCodecContext *dec_ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(dec_ctx, st->codecpar);
    if (avcodec_open2(dec_ctx, codec, NULL) < 0) {
        printf("Failed to open codec\n");
        return 1;
    }
    AVPacket *pkt = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    int frame_count = 0;
    while (av_read_frame(fmt_ctx, pkt) >= 0) {
        if (pkt->stream_index == 0) {
            if (avcodec_send_packet(dec_ctx, pkt) == 0) {
                while (avcodec_receive_frame(dec_ctx, frame) == 0) {
                    frame_count++;
                }
            }
        }
        av_packet_unref(pkt);
    }
    printf("Decoded %d frames successfully!\n", frame_count);
    return 0;
}
