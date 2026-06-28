# takdecomp

`takdecomp` is an open-source C++20 suite for decoding and encoding the TAK (Tom's lossless Audio Kompressor) audio format. Designed to offer full compatibility with the original closed-source executables, it provides a clean, easily integrable, multithreaded codebase.

For an in-depth understanding of how the TAK codec works under the hood, including its container layout, linear predictive coding math, and entropy segmentation logic, please refer to the [TAK format specification](docs/tak.md).

## Features

The project provides reproduction to WAV, MD5 checksum extraction, and APEv2 decoding. On the compression side, it handles audio framing, multi-threading support, tagging, wave metadata extraction, and bitstream verification.

Everything is cross-platform, having been heavily tested and natively compiled on Windows (x64, x86, ARM64), Linux, macOS and FreeBSD. Both the encoder and decoder have their own headers and targets.

## Building

```bash
git clone https://github.com/Snesnopic/takdecomp.git
cd takdecomp
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

This will produce a `takc` executable inside the `bin/` directory.

## CLI usage

The syntax and flags replicate the original TAK executables. The tool automatically detects whether you are trying to encode or decode based on the input file extension (`.wav` for encoding, `.tak` for decoding), or you can force the mode using `-e` or `-d`.

When encoding, you can specify the compression preset using `-p#` (from 0 to 5, where 5 is maximum compression and 0 is maximum speed). You can allocate multiple threads using `-tn#`. Additional flags like `-tt` allow you to write APEv2 tags, `-wm` handles foreign wave chunks, and `-v` verifies frame integrity by running a post-encode decoding check.

When decoding, passing `-t` performs only a file integrity test without writing audio output. The `-md5` flag calculates and verifies the MD5 checksum of the bitstream against the one stored in the header. If the output path is not specified, the `.wav` or `.tak` file will be generated in the same directory as the input.

```bash
# encoding
./takc input.wav [output.tak] [options]

# decoding
./takc input.tak [output.wav] [options]
```

## Using as a library

Both the encoder and decoder are wrapped in the targets `takdec_core` and `takenc_core`.
```cmake
add_subdirectory(takdecomp)
target_link_libraries(your_executable PRIVATE takdec_core takenc_core)
```

To perform a basic decode operation, include `tak_decoder/decoder.hpp` and call `takdecomp::Decoder::decode_file("audio.tak", "audio.wav")`. This returns a result struct containing the number of samples decoded, or throws an exception on failure.

To perform a basic encode operation, include `tak_encoder/encoder.hpp`, populate a `takenc::EncoderConfig` object with your desired preset and thread count, and call `takenc::Encoder::encode_file("audio.wav", "audio.tak", cfg, nullptr)`.

## License

This project is released under the terms of the license specified in the repository. The implementation of the compression and decompression algorithms is based on the specifications of the open format Tom's lossless Audio Kompressor.
