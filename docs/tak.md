# TAK format specification

TAK stands for Tom's lossless Audio Kompressor. As an aside, it is also a reminiscence of a (not very philanthropic) character from Stephen King's "The Regulators". Early semi-public evaluation versions operated under the working title YALAC.

## Features

TAK is designed to offer high compression, often matching the strongest modes of Monkey's Audio and OptimFrog, particularly on classical music or speech recordings. It is built for extremely high compression speed, with the Turbo or Fast modes being among the fastest available for their respective compression ratios. Decompression speed is very high, performing on the level of FLAC and significantly faster than most symmetric compressors. 

The format natively supports up to four threads to take advantage of multi-core CPUs during compression. It handles all common audio formats and supports streaming by embedding an info frame containing all necessary decoding information every two seconds. Error tolerance is a priority, as a single bit error never damages more than 250 milliseconds of audio data due to the use of completely independent frames. The decoder processes even heavily damaged files, optionally replacing affected data with silence.

Each frame is protected by a 24-bit checksum for error detection, and the container includes MD5 checksums for rapid identification of audio material. Fast, sample-accurate access to any playback position is achieved through a seek table in the file header containing index positions at one-second intervals. Even without this table, random access is efficient using synchronization codes and offset values optionally included in frame headers. A flexible structure also allows the inclusion of non-audio metadata such as pictures or cuesheets, and it is fully compatible with APEv2 tags.

## Audio compression principles

Audio compressors reduce the storage space required for audio files. Through compression or encoding, a compact representation of the data is generated and written to a file. The counterpart is decompression or decoding, which transforms the compressed data back into a form suitable for playback or further processing.

### Lossless vs lossy

TAK belongs to the family of lossless audio compressors, alongside FLAC, WavPack, and Monkey's Audio. In contrast to lossy audio compression methods like MP3, they allow the original file to be restored exactly from the compressed data. There are no losses, and the result is a bit-exact copy of the original. Lossless audio compressors behave similarly to familiar ZIP programs, which reproduce data like text without alteration.

Lossy audio compressors irrevocably remove components of the original audio signal that are typically not perceived by humans with normal hearing. The original signal cannot be fully recovered. While lossy formats achieve significantly higher compression ratios, lossless compressors typically only reduce the file size by a factor of two, depending heavily on the audio material.

### Applications of lossless audio compression

While dealing with lossy audio compression is commonplace today, lossless methods still occupy a niche. Nevertheless, they are superior to lossy methods in many cases. 

In music production, the original signal often goes through numerous processing steps. If intermediate results are saved and processed later, losses are completely unacceptable because the losses from all saving processes accumulate and quickly lead to audible distortions. Furthermore, signal alterations from lossy compression that are normally inaudible are quickly raised above the perception threshold by typical sound manipulations during production.

For archiving private music collections, lossy compression might initially seem sufficient. However, if a switch to another lossy format is made later (for example, to establish compatibility with newer playback devices), there is a risk that the accumulation of signal distortions from both compression methods will lead to audible artifacts. Currently, only lossless methods can guarantee unadulterated sound quality.

## How it works

To achieve a more compact representation of data, all lossless audio compressors search for regularities in the audio signal. Usually, there is a strong dependence between successive signal values, so that subsequent values can be predicted from preceding ones. To do this, suitable parameters must be calculated that describe the nature of the dependence as accurately as possible. These can then be used for prediction. Instead of the original data, the encoder stores the differences between the prediction and the original signal, known as the prediction error. Since the differences in a good prediction are much smaller than the original values, and smaller values require less storage space, compression is achieved. 

The decoder makes the same prediction, adding the prediction error stored by the encoder to the predicted values to recover the original values. Calculating the optimal parameters for prediction is the most time-consuming process of compression. Since prediction parameters must fit the signal they are supposed to predict, any changes in crucial aspects of the audio signal require a recalculation or adaptation of the prediction parameters.

## Asymmetric vs symmetric compressors

TAK is essentially based on adaptive linear forward prediction. This same technique is used by FLAC, LPAC, Mpeg4Als, and Shorten. These programs belong to the class of asymmetric audio compressors. The asymmetry refers to the different computational effort required for encoding and decoding.

All parameters relevant for compression are calculated once during encoding and stored in the compressed file. The decoder simply reads these parameters from the file, does not need to repeat the calculations, and can therefore achieve very high speeds. It is also possible to increase the computational effort in the encoder to obtain better parameters for stronger compression without significantly increasing the computational effort in the decoder. As a result, the processing speed of the encoder drops while that of the decoder remains constant.

In symmetric methods (used in WavPack, Monkey's Audio, OptimFrog), the calculations of the compression parameters are carried out in both the encoder and the decoder, resulting in approximately the same computational effort on both sides. If the effort in the encoder is increased to improve compression performance, the effort in the decoder increases equally. However, symmetric methods use backward prediction, continuously calculating parameters based on the preceding signal without needing to store them, which saves space and increases the compression ratio. 

Performance comparisons of current lossless compression programs confirm that asymmetric methods decode significantly faster, while symmetric methods can achieve the highest compression ratios, albeit at the expense of decoding speed.

## TAK's technology

The entire design was optimized for high speed and differs significantly from other asymmetric compressors in several respects.

### Container format

The compressed frames generated by the encoder are packaged into a proprietary container format designed for simplicity, compactness, streaming capability, and high error tolerance.

The stream header is identified by the magic bytes `tBaK` (Tom Becker's Audio Kompressor). This header packs critical stream properties into a bit-aligned structure, including the total number of samples, the sample rate, the number of channels, the bit depth, and the frame size. Following the header and the frames, the container supports embedding an APEv2 tag for standard metadata.

The header also optionally includes a seek table, which consists of 24-bit or 32-bit byte offsets for each frame relative to the end of the header, allowing rapid navigation. 

### Independent audio frames

Audio data is fundamentally divided into completely independent frames of equal size, which can be chosen by default in the range of 94 to 250 milliseconds. Independent small frames offer the advantage that the destructive effect of individual data errors is limited to short sections of the audio data.

Each frame begins with a synchronization word and a header containing a timestamp, a flag indicating if the frame is the final one, and configuration flags. The frame data is bit-packed using a big-endian bitstream convention. For stereo audio, a decorrelation step transforms left and right channels into mid and side channels, reducing redundancy, with the exact decorrelation mode varying adaptively per frame.

### Linear predictive coding (LPC)

TAK relies on adaptive linear forward prediction using up to 160 predictor coefficients. These coefficients are compressed using a very fast method that achieves comparable compression ratios to PARCOR coefficients but manages without complex 64-bit calculations in the decoder. 

The linear prediction filter can be preceded by two additional filters that usually lead to a significant increase in efficiency. The core algorithm employs the Levinson-Durbin recursion. A critical mechanism discovered during the modern C++ reconstruction involves dynamic precision shifting for 24-bit audio. Because 24-bit samples generate massive values during autocorrelation that can exceed double-precision floating-point limits (causing loss of significance), the encoder dynamically downshifts the sample values based on density before calculating the autocorrelation matrix. Since the LPC equations are scale-invariant, this prevents precision drift, ensuring that the quantized coefficients chosen by the encoder exactly match the integer arithmetic reconstruction performed by the decoder.

### Entropy segmentation and the xcodes array

The final encoding of the prediction errors uses a method best described as a mixture of Huffman and Rice coding. It is significantly more efficient than comparatively simple Rice coding, but almost as fast.

A very fast algorithm divides each frame into up to five subframes of variable size to allow block-wise adaptation to changes in signal characteristics. The residual signal is then segmented. For each segment, the encoder selects an optimal coding mode based on local variance. 

A central architectural feature is the static `xcodes` lookup table, which defines escape thresholds, scale factors, and bias values for over 50 different coding modes. The encoder writes a mode index to the bitstream, and the decoder pulls the exact parameters from this array. A historical typo in this array during reverse engineering (a bias of `0x0000180` instead of `0x0001800` for mode 20) demonstrated how tightly coupled the encoder and decoder are; any deviation in these mathematical constants destroys the reconstructed audio.

### Integrity mechanisms

Data integrity is rigorously controlled via CRC. The stream header is protected by a CRC, and every single audio frame concludes with a 24-bit CRC (CRC-24). The decoder computes the CRC of the decompressed PCM samples for the frame and compares it against the stored CRC. This guarantees that any transmission error or bit flip is immediately detected, preventing corrupted audio output.

## Credits

*Original project and format design by Thomas Becker. Visit the original website here: [http://www.thbeck.de/Tak/Tak.html](http://www.thbeck.de/Tak/Tak.html)*
