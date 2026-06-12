# TAK Decoder (C++20 Port)

This project is a modern C++20 port of the TAK (Tom's lossless Audio Kompressor) decoder, originally reverse-engineered by the FFmpeg project.

## Architecture

The project is structured with a clear separation between the core decoding logic and the CLI interface:
- `src/core`: Contains the decoder logic, compiled as a static library.
- `src/cli`: The command line interface executable.

### Components Built So Far
- **BitStreamReader**: A C++20 implementation of a bitstream reader using `std::span`, replacing FFmpeg's macro-heavy `GetBitContext`. Supports bits extraction (`get_bits`, `get_sbits`, `get_bits64`, `get_bits1`).
- **Decoder & StreamInfo Parser**: Replaces `tak_parse_streaminfo`. Extrapolates frame sizes, bit depth, channel layout, and sample rate.
- **Header Parsing**: Replaces `ff_tak_decode_frame_header`. Validates Sync IDs, extracts CRC24, and parses inner metadata.
- **Segment & Residue Decoder**: Implementation of `decode_segment` and `decode_residues` utilizing the C++20 bitstream parser to extract the Rice/Golomb encoded residuals.
- **LPC Predictor**: The Levinson-Durbin/LPC coefficient predictor (`decode_lpc`) reconstructed in clean C++.

### Next Steps for Full Decoding
1. **Subframe Decoding**: Porting `decode_subframe` (filtering and scalar products).
2. **Channel Decoding**: Porting `decode_channel`.
3. **Decorrelation**: Multi-channel decorrelation matrix (`decorrelate_ls`, `decorrelate_sm`, etc.).
4. **Demuxer**: APEv2 tag parsing and TAK container parsing (from `takdec_format.c`).

## Build Instructions

```bash
mkdir build
cd build
cmake ..
make
```
