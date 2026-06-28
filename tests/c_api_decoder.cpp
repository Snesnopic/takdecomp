#include "tak_deco_lib.h"
#include <iostream>
#include <fstream>
#include <vector>

void write_wav_header(std::ostream &os, int sample_rate, int channels, int bps, int total_samples) {
    os.write("RIFF", 4);
    uint32_t data_size = total_samples * channels * (bps / 8);
    uint32_t file_size = 36 + data_size;
    os.write(reinterpret_cast<const char *>(&file_size), 4);
    os.write("WAVE", 4);
    os.write("fmt ", 4);
    uint32_t fmt_size = 16;
    os.write(reinterpret_cast<const char *>(&fmt_size), 4);
    uint16_t audio_format = 1; // PCM
    os.write(reinterpret_cast<const char *>(&audio_format), 2);
    uint16_t num_channels = channels;
    os.write(reinterpret_cast<const char *>(&num_channels), 2);
    uint32_t sr = sample_rate;
    os.write(reinterpret_cast<const char *>(&sr), 4);
    uint32_t byte_rate = sample_rate * channels * (bps / 8);
    os.write(reinterpret_cast<const char *>(&byte_rate), 4);
    uint16_t block_align = channels * (bps / 8);
    os.write(reinterpret_cast<const char *>(&block_align), 2);
    uint16_t bits_per_sample = bps;
    os.write(reinterpret_cast<const char *>(&bits_per_sample), 2);
    os.write("data", 4);
    os.write(reinterpret_cast<const char *>(&data_size), 4);
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: c_api_decoder <input.tak> <output.wav>\n";
        return 1;
    }

    TtakSeekableStreamDecoder decoder = tak_SSD_Create_FromFile(argv[1], nullptr, nullptr, nullptr);
    if (!decoder) {
        std::cerr << "Error: Could not open " << argv[1] << "\n";
        return 1;
    }

    Ttak_str_StreamInfo info;
    if (tak_SSD_GetStreamInfo(decoder, &info) != tak_res_Ok) {
        std::cerr << "Error: Could not read StreamInfo\n";
        tak_SSD_Destroy(decoder);
        return 1;
    }

    std::ofstream os(argv[2], std::ios::binary);
    if (!os) {
        std::cerr << "Error: Could not open " << argv[2] << " for writing\n";
        tak_SSD_Destroy(decoder);
        return 1;
    }

    write_wav_header(os, info.Audio.SampleRate, info.Audio.ChannelNum, info.Audio.SampleBits, 0);

    int block_size = 4096;
    int bytes_per_sample = info.Audio.ChannelNum * (info.Audio.SampleBits / 8);
    std::vector<uint8_t> pcm_buf(block_size * bytes_per_sample);

    int total_samples_written = 0;
    while (true) {
        TtakInt32 read_num = 0;
        TtakResult res = tak_SSD_ReadAudio(decoder, pcm_buf.data(), block_size, &read_num);
        if (res != tak_res_Ok) {
            std::cerr << "ReadAudio error: " << res << "\n";
            break;
        }
        if (read_num == 0) break;

        os.write(reinterpret_cast<char*>(pcm_buf.data()), read_num * bytes_per_sample);
        total_samples_written += read_num;
    }

    os.seekp(0);
    write_wav_header(os, info.Audio.SampleRate, info.Audio.ChannelNum, info.Audio.SampleBits, total_samples_written);
    
    tak_SSD_Destroy(decoder);
    return 0;
}
