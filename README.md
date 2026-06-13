# takdecomp

`takdecomp` is an open-source C++20 suite for decoding and encoding the **TAK** (Tom's lossless Audio Kompressor) audio format.
Designed to offer full cross-platform compatibility with the original `takc.exe` and `takd.exe` executables, it provides a clean, easily integrable, multithreaded codebase that is completely independent of Windows.

## Features

- **Full Decoding (`takdec`)**: 1:1 bitstream reproduction to WAV, MD5 extraction, APEv2 decoding.
- **High-Performance Encoding (`takenc`)**: Audio compression into frames, multithreading support (`-tn#`), APEv2 tagging, wave metadata extraction, and bitstream verification.
- **Cross-Platform Support**: Tested and natively compilable on Windows (x64, x86, ARM64), Linux, and macOS.
- **Built as a Library**: Modular C++ structure to easily integrate the encoder or decoder into your own software ecosystem (players, converters, DAWs).

## Building

The project uses CMake. You can easily build it from the command line:

```bash
git clone https://github.com/your-username/takdecomp.git
cd takdecomp
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

This will produce the `takenc` and `takdec` executables inside the `bin/` directory.

## CLI Usage

The syntax and flags faithfully replicate the original TAK executables.

### Decoding (takdec)

```bash
./takdec input.tak [output.wav] [options]
```
- `-t`: Performs only a file integrity test (does not write the audio output).
- `-md5`: Calculates and verifies the MD5 of the bitstream against the one contained in the header.
- If the output is not specified, the `.wav` file will be saved in the same folder with the same name.

### Encoding (takenc)

```bash
./takenc input.wav [output.tak] [options]
```
- `-p#`: Compression preset (from 0 to 5). P5 = maximum compression, P0 = maximum speed. Extra: `-p#m`, `-p#e` for maximum effort.
- `-tn#`: Specifies the number of threads to use (default: 1).
- `-tt "Key=Value"`: Writes APEv2 tags to the output file.
- `-wm#`: Writes raw wave file metadata (0 = ignore, 1 = copy foreign chunks, default: 1).
- `-ihs`: Ignores the header size (useful when piping streams of unknown length).
- `-overwrite`: Silently overwrites the destination file.
- `-v`: Verifies frame integrity by automatically invoking post-encode decoding.

## Using as a Library

Both engines are wrapped in the static targets `takdec_core` e `takenc_core`.
By adding it to your project via CMake:

```cmake
add_subdirectory(takdecomp)
target_link_libraries(your_executable PRIVATE takdec_core takenc_core)
```

Basic decoding example:
```cpp
#include "tak_decoder/decoder.hpp"
#include <iostream>

int main() {
    try {
        takdecomp::DecodeResult res = takdecomp::Decoder::decode_file("audio.tak", "audio.wav");
        std::cout << "Decoded " << res.samples_decoded << " samples!\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }
}
```

Basic encoding example:
```cpp
#include "tak_encoder/encoder.hpp"

int main() {
    takenc::EncoderConfig cfg;
    cfg.preset = 2; // normal preset
    cfg.threads = 4; // use 4 threads

    takenc::EncodeResult res = takenc::Encoder::encode_file("audio.wav", "audio.tak", cfg, nullptr);
    return 0;
}
```

## License

This project is released under the terms of the license specified in the repository. The implementation of the compression/decompression algorithm is based on the specifications of the open format Tom's lossless Audio Kompressor.
