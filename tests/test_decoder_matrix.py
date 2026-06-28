"""
Test matrice decoder: codifica con Takc.exe (originale), decodifica con il nostro takc,
confronta il PCM risultante con il WAV originale.

Variabili d'ambiente:
    TAKC_BIN   Path al nostro binario takc         (default: ./build/bin/takc)
    TAKC_EXE   Path al Takc.exe ufficiale          (default: Takc.exe)

Uso:
    TAKC_BIN=./build/bin/takc TAKC_EXE=./tak/Takc.exe python3 tests/test_decoder_matrix.py
"""

import os
import subprocess
import itertools
import sys
import wave

SAMPLE_RATES = [44100, 48000, 88200, 96000, 192000]
CHANNELS = [1, 2, 6]
BIT_DEPTHS = [8, 16, 24]

TAKC_BIN = os.environ.get("TAKC_BIN", "./build/bin/takc")
TAKC_EXE = os.environ.get("TAKC_EXE", "Takc.exe")


def compare_audio_data(file1: str, file2: str) -> bool:
    try:
        with wave.open(file1, "rb") as w1, wave.open(file2, "rb") as w2:
            return w1.readframes(w1.getnframes()) == w2.readframes(w2.getnframes())
    except Exception:
        return False


def run() -> int:
    print(f"{'SR':<8} {'CH':<4} {'BPS':<4} | {'Takc Enc':<10} | {'takc Dec':<10} | {'Match':<10}")
    print("-" * 65)

    os.makedirs("test_matrix", exist_ok=True)
    failures = 0

    for sr, ch, bits in itertools.product(SAMPLE_RATES, CHANNELS, BIT_DEPTHS):
        base_name = f"test_matrix/{sr}_{ch}ch_{bits}b"
        wav_path = f"{base_name}.wav"
        tak_path = f"{base_name}_off_enc.tak"
        dec_path = f"{base_name}_off_enc_dec.wav"

        if not os.path.exists(wav_path):
            print(f"{sr:<8} {ch:<4} {bits:<4} | {'SKIP':<10} | {'(no wav)':<10} | {'-':<10}")
            continue

        for p in [tak_path, dec_path]:
            if os.path.exists(p):
                os.remove(p)

        # 1. Encode with official Takc.exe via wine
        try:
            res_enc = subprocess.run(
                ["wine", TAKC_EXE, "-e", "-overwrite", wav_path, tak_path],
                text=True, capture_output=True, timeout=30,
                env={**os.environ, "WINEDEBUG": "-all"}
            )
            enc_status = "OK" if res_enc.returncode == 0 else "FAIL"
        except subprocess.TimeoutExpired as e:
            enc_status = "TIMEOUT"
            out = e.stdout.decode() if isinstance(e.stdout, bytes) else (e.stdout or "")
            err = e.stderr.decode() if isinstance(e.stderr, bytes) else (e.stderr or "")
            print(f"{sr:<8} {ch:<4} {bits:<4} | {enc_status:<10} | {'-':<10} | {'-':<10}")
            print(f"  [Takc.exe timeout] {out.strip()} {err.strip()}")
            failures += 1
            continue

        if enc_status != "OK":
            print(f"{sr:<8} {ch:<4} {bits:<4} | {enc_status:<10} | {'-':<10} | {'-':<10}")
            print(f"  [Takc.exe error] {res_enc.stderr.strip()}")
            failures += 1
            continue

        # 2. Decode with our takc
        try:
            res_dec = subprocess.run(
                [TAKC_BIN, "-d", "-overwrite", tak_path, dec_path],
                text=True, capture_output=True, timeout=30
            )
            dec_status = "OK" if res_dec.returncode == 0 else "FAIL"
        except subprocess.TimeoutExpired:
            dec_status = "TIMEOUT"
            print(f"{sr:<8} {ch:<4} {bits:<4} | {enc_status:<10} | {dec_status:<10} | {'-':<10}")
            failures += 1
            continue

        if dec_status != "OK":
            print(f"{sr:<8} {ch:<4} {bits:<4} | {enc_status:<10} | {dec_status:<10} | {'-':<10}")
            print(f"  [takc error] {res_dec.stderr.strip()}")
            failures += 1
            continue

        # 3. Compare PCM data
        data_match = compare_audio_data(wav_path, dec_path)
        match_str = "PASS" if data_match else "FAIL"
        if not data_match:
            failures += 1

        print(f"{sr:<8} {ch:<4} {bits:<4} | {enc_status:<10} | {dec_status:<10} | {match_str:<10}")

    print()
    if failures:
        print(f"FAILED: {failures} test(s) failed.")
    else:
        print("All tests passed.")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(run())
