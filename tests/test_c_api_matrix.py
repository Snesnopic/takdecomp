"""
decoder matrix test: encodes with the official takc.exe, decodes with our takc,
and compares pcm and wav header with the original.

environment variables:
    TAKC_BIN   path to our takc binary       (default: ./build/bin/takc)
    TAKC_EXE   path to the official takc.exe (default: Takc.exe)

usage:
    TAKC_BIN=./build/bin/takc TAKC_EXE=./tak/Takc.exe python3 tests/test_decoder_matrix.py
"""

import os
import subprocess
import itertools
import sys
import wave
from dataclasses import dataclass

SAMPLE_RATES = [44100, 48000, 88200, 96000, 192000]
CHANNELS = [1, 2, 6]
BIT_DEPTHS = [8, 16, 24]
SIGNALS = ["sine", "silence", "noise", "long", "short", "impulse"]

C_API_DEC = os.environ.get("C_API_DEC", "./build/c_api_decoder")
TAKC_EXE = os.environ.get("TAKC_EXE", "Takc.exe")

WINE_ENV = {**os.environ, "WINEDEBUG": "-all"}


@dataclass
class WavInfo:
    channels: int
    sample_rate: int
    sampwidth: int
    n_frames: int

    def matches(self, other: "WavInfo") -> bool:
        return (self.channels == other.channels and
                self.sample_rate == other.sample_rate and
                self.sampwidth == other.sampwidth and
                self.n_frames == other.n_frames)


def read_wav_info(path: str) -> WavInfo | None:
    try:
        with wave.open(path, "rb") as w:
            return WavInfo(w.getnchannels(), w.getframerate(),
                           w.getsampwidth(), w.getnframes())
    except Exception:
        return None


def compare_pcm(file1: str, file2: str) -> bool:
    try:
        with wave.open(file1, "rb") as w1, wave.open(file2, "rb") as w2:
            return w1.readframes(w1.getnframes()) == w2.readframes(w2.getnframes())
    except Exception:
        return False


def run_enc_wine(wav_path: str, tak_path: str) -> tuple[bool, str]:
    try:
        res = subprocess.run(
            ["wine", TAKC_EXE, "-e", "-overwrite", wav_path, tak_path],
            capture_output=True, text=True, timeout=60, env=WINE_ENV,
        )
        return res.returncode == 0, res.stderr.strip()
    except subprocess.TimeoutExpired:
        return False, "TIMEOUT"


def run_dec(tak_path: str, dec_path: str) -> tuple[bool, str]:
    try:
        res = subprocess.run(
            [C_API_DEC, tak_path, dec_path],
            capture_output=True, text=True, timeout=60,
        )
        return res.returncode == 0, res.stderr.strip()
    except subprocess.TimeoutExpired:
        return False, "TIMEOUT"


def run() -> int:
    header = (f"{'SR':<8} {'CH':<3} {'BPS':<4} {'SIGNAL':<9} "
              f"{'Enc':<7} {'Dec':<7} {'PCM':<6} {'HDR':<6}")
    sep = "-" * len(header)
    print(header)
    print(sep)

    os.makedirs("test_matrix", exist_ok=True)
    failures = 0
    total = 0

    combos = list(itertools.product(SAMPLE_RATES, CHANNELS, BIT_DEPTHS, SIGNALS))
    for sr, ch, bits, sig in combos:
        total += 1
        base = f"test_matrix/{sr}_{ch}ch_{bits}b_{sig}"
        wav_path = f"{base}.wav"
        tak_path = f"{base}_off_enc.tak"
        dec_path = f"{base}_off_enc_dec.wav"

        if not os.path.exists(wav_path):
            print(f"{sr:<8} {ch:<3} {bits:<4} {sig:<9} "
                  f"{'SKIP':<7} {'(no wav)':<7} {'-':<6} {'-':<6}")
            failures += 1
            continue

        for p in [tak_path, dec_path]:
            if os.path.exists(p):
                os.remove(p)

        # 1. Encode with official Takc.exe
        enc_ok, enc_err = run_enc_wine(wav_path, tak_path)
        if not enc_ok:
            tag = "TIMEOUT" if enc_err == "TIMEOUT" else "FAIL"
            print(f"{sr:<8} {ch:<3} {bits:<4} {sig:<9} "
                  f"{tag:<7} {'-':<7} {'-':<6} {'-':<6}")
            if enc_err and enc_err != "TIMEOUT":
                print(f"  [enc] {enc_err[:120]}")
            failures += 1
            continue

        # 2. Decode with our takc
        dec_ok, dec_err = run_dec(tak_path, dec_path)
        if not dec_ok:
            tag = "TIMEOUT" if dec_err == "TIMEOUT" else "FAIL"
            print(f"{sr:<8} {ch:<3} {bits:<4} {sig:<9} "
                  f"{'OK':<7} {tag:<7} {'-':<6} {'-':<6}")
            if dec_err and dec_err != "TIMEOUT":
                print(f"  [dec] {dec_err[:120]}")
            failures += 1
            continue

        # 3. Compare PCM
        pcm_ok = compare_pcm(wav_path, dec_path)

        # 4. Verify WAV header
        orig_info = read_wav_info(wav_path)
        dec_info  = read_wav_info(dec_path)
        hdr_ok = (orig_info is not None and dec_info is not None
                  and orig_info.matches(dec_info))

        if not pcm_ok or not hdr_ok:
            failures += 1

        print(f"{sr:<8} {ch:<3} {bits:<4} {sig:<9} "
              f"{'OK':<7} {'OK':<7} "
              f"{'PASS' if pcm_ok else 'FAIL':<6} "
              f"{'PASS' if hdr_ok else 'FAIL':<6}")
        if not hdr_ok and orig_info and dec_info:
            print(f"  [hdr] orig={orig_info} dec={dec_info}")

    print()
    print(f"Results: {total - failures}/{total} passed.")
    if failures:
        print(f"FAILED: {failures} test(s) failed.")
    else:
        print("All tests passed.")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(run())
