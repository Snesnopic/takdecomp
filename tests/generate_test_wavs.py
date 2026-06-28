"""
generates synthetic wav files used by the test matrix.
requires ffmpeg in path.

produces a test_matrix/ directory with files named:
    {sr}_{ch}ch_{bits}b_{signal}.wav

usage:
    python3 tests/generate_test_wavs.py
"""

import os
import subprocess
import itertools
import sys

SAMPLE_RATES = [44100, 48000, 88200, 96000, 192000]
CHANNELS = [1, 2, 6]
BIT_DEPTHS = [8, 16, 24]

# (ffmpeg lavfi source expression, duration in seconds)
# NOTE: duration is passed via the -t flag, NOT appended to the source expression,
# to avoid ffmpeg option-separator ambiguity with sources like anoisesrc.
SIGNALS: dict[str, tuple[str, float]] = {
    "sine":     ("sine=frequency=1000",  0.5),   # standard 1kHz tone
    "silence":  ("aevalsrc=0",           0.5),   # all-zeros — edge case for encoder
    "noise":    ("anoisesrc",            0.5),   # white noise — maximum entropy
    "long":     ("sine=frequency=1000",  5.0),   # multi-frame stress test
    "short":    ("sine=frequency=1000",  0.05),  # ~2205 samples @ 44.1kHz, partial frame
    "impulse":  ("aevalsrc=not(n)",      0.5),   # 1 at n=0, 0 elsewhere (no commas in expr)
}

OUTPUT_DIR = "test_matrix"


def codec_for_bits(bits: int) -> str:
    if bits == 8:
        return "pcm_u8"
    if bits == 16:
        return "pcm_s16le"
    return "pcm_s24le"


def generate_wav(filepath: str, sr: int, ch: int, bits: int,
                 source: str, duration: float) -> bool:
    # Duration is passed via -t rather than embedded in the filter string so that
    # sources like `anoisesrc` (no existing options) and complex expressions like
    # `aevalsrc=not(n)` are not broken by lavfi option-separator rules.
    cmd = [
        "ffmpeg", "-y",
        "-f", "lavfi",
        "-i", source,
        "-t", str(duration),
        "-ac", str(ch),
        "-ar", str(sr),
        "-c:a", codec_for_bits(bits),
        filepath,
    ]
    result = subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return result.returncode == 0


def main() -> int:
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    total = len(SAMPLE_RATES) * len(CHANNELS) * len(BIT_DEPTHS) * len(SIGNALS)
    done = 0
    failed: list[str] = []

    for (sr, ch, bits), (sig_name, (source, dur)) in itertools.product(
        itertools.product(SAMPLE_RATES, CHANNELS, BIT_DEPTHS),
        SIGNALS.items(),
    ):
        wav_path = os.path.join(OUTPUT_DIR, f"{sr}_{ch}ch_{bits}b_{sig_name}.wav")
        if generate_wav(wav_path, sr, ch, bits, source, dur):
            done += 1
        else:
            print(f"FAIL  {wav_path}", file=sys.stderr)
            failed.append(wav_path)

    print(f"Generated {done}/{total} WAV files in '{OUTPUT_DIR}/'")
    if failed:
        print(f"Failed ({len(failed)}):", file=sys.stderr)
        for f in failed:
            print(f"  {f}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
