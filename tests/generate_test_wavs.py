"""
Genera i file WAV sintetici usati dalla matrix di test.
Richiede ffmpeg nel PATH.

Uso:
    python3 tests/generate_test_wavs.py
"""

import os
import subprocess
import itertools

SAMPLE_RATES = [44100, 48000, 88200, 96000, 192000]
CHANNELS = [1, 2, 6]
BIT_DEPTHS = [8, 16, 24]

OUTPUT_DIR = "test_matrix"


def generate_wav(filepath: str, sr: int, ch: int, bits: int) -> bool:
    """Genera un file WAV sintetico (sine sweep da 0.5 sec) tramite ffmpeg."""
    if bits == 8:
        codec = "pcm_u8"
    elif bits == 16:
        codec = "pcm_s16le"
    else:
        codec = "pcm_s24le"

    cmd = [
        "ffmpeg", "-y",
        "-f", "lavfi",
        "-i", "sine=frequency=1000:duration=0.5",
        "-ac", str(ch),
        "-ar", str(sr),
        "-c:a", codec,
        filepath
    ]
    result = subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return result.returncode == 0


def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    total = len(SAMPLE_RATES) * len(CHANNELS) * len(BIT_DEPTHS)
    done = 0

    for sr, ch, bits in itertools.product(SAMPLE_RATES, CHANNELS, BIT_DEPTHS):
        wav_path = os.path.join(OUTPUT_DIR, f"{sr}_{ch}ch_{bits}b.wav")
        if not generate_wav(wav_path, sr, ch, bits):
            print(f"FAIL  {wav_path}")
        else:
            done += 1

    print(f"Generated {done}/{total} WAV files in '{OUTPUT_DIR}/'")


if __name__ == "__main__":
    main()
