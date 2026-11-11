# regression test: cross-FIR decorrelation (modes 6/7) on loud stereo must keep the decoder's int16 history
# usage: TAKC_BIN=./build/bin/takc [TAKC_EXE=<path>/Takc.exe] python3 tests/test_loud_stereo.py

import os
import random
import subprocess
import sys
import wave

TAKC_BIN = os.environ.get("TAKC_BIN", "./build/bin/takc")
TAKC_EXE = os.environ.get("TAKC_EXE")
WINE_ENV = {**os.environ, "WINEDEBUG": "-all"}
OUT_DIR = "test_matrix/loud"
SAMPLE_RATE = 44100
N_SAMPLES = SAMPLE_RATE * 2
PRESETS = ["p0", "p2", "p4"]
INT16_MAX = 32767


def clip(v: float, bits: int) -> int:
    lim = (1 << (bits - 1)) - 1
    return max(-lim - 1, min(lim, round(v)))


def loud_pair(bits: int, seed: int) -> tuple[list[int], list[int]]:
    # clipped low-passed noise and a smoothed noisy copy of it: the encoder picks a cross-FIR mode
    rng = random.Random(seed)
    scale = 1 << (bits - 16)
    y = 0.0
    loud = []
    for _ in range(N_SAMPLES):
        y = 0.97 * y + rng.gauss(0, 1)
        loud.append(clip(10000 * scale * y, bits))
    copy = []
    for i in range(N_SAMPLES):
        avg = sum(loud[max(0, i - k)] for k in range(3)) / 3
        copy.append(clip(avg + rng.gauss(0, 300) * scale, bits))
    return loud, copy


def write_wav(path: str, ch1: list[int], ch2: list[int], bits: int) -> None:
    width = bits // 8
    data = bytearray()
    for a, b in zip(ch1, ch2):
        data += a.to_bytes(width, "little", signed=True)
        data += b.to_bytes(width, "little", signed=True)
    with wave.open(path, "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(width)
        w.setframerate(SAMPLE_RATE)
        w.writeframes(bytes(data))


def same_audio(path1: str, path2: str) -> bool:
    try:
        with wave.open(path1, "rb") as w1, wave.open(path2, "rb") as w2:
            return (w1.getparams()[:4] == w2.getparams()[:4]
                    and w1.readframes(w1.getnframes()) == w2.readframes(w2.getnframes()))
    except Exception:
        return False


def run_cmd(cmd: list[str], env: dict | None = None) -> tuple[bool, str]:
    try:
        res = subprocess.run(cmd, capture_output=True, text=True, timeout=120, env=env)
        return res.returncode == 0, res.stderr.strip()
    except subprocess.TimeoutExpired:
        return False, "TIMEOUT"


def decode_check(cmd: list[str], src: str, dec: str, env: dict | None = None) -> str:
    if os.path.exists(dec):
        os.remove(dec)
    ok, err = run_cmd(cmd, env)
    if not ok:
        return "TIMEOUT" if err == "TIMEOUT" else "FAIL"
    return "PASS" if same_audio(src, dec) else "BAD"


def run() -> int:
    os.makedirs(OUT_DIR, exist_ok=True)
    decoders = ["ours"] + (["takc.exe"] if TAKC_EXE else [])
    header = f"{'SIGNAL':<16} {'PRESET':<7} {'Enc':<7} " + " ".join(f"{d:<9}" for d in decoders)
    print(header)
    print("-" * len(header))

    failures = 0
    total = 0
    for bits in (16, 24):
        loud, copy = loud_pair(bits, seed=1234)
        # without jumps beyond int16 the old bug cannot show, so the test would pass for nothing
        jump = max(abs(loud[i] - loud[i - 1]) for i in range(1, N_SAMPLES))
        if jump <= INT16_MAX:
            print(f"signal too quiet for this test: largest jump {jump} at {bits} bit")
            return 1

        for side, (ch1, ch2) in (("left", (loud, copy)), ("right", (copy, loud))):
            name = f"{bits}b_loud_{side}"
            wav_path = f"{OUT_DIR}/{name}.wav"
            write_wav(wav_path, ch1, ch2, bits)

            for preset in PRESETS:
                total += 1
                tak_path = f"{OUT_DIR}/{name}_{preset}.tak"
                if os.path.exists(tak_path):
                    os.remove(tak_path)
                enc_ok, enc_err = run_cmd([TAKC_BIN, f"-{preset}", "-e", "-overwrite", "-q", wav_path, tak_path])
                if not enc_ok:
                    print(f"{name:<16} {preset:<7} {'FAIL':<7}")
                    if enc_err and enc_err != "TIMEOUT":
                        print(f"  [enc] {enc_err[:120]}")
                    failures += 1
                    continue

                results = []
                ours_dec = f"{OUT_DIR}/{name}_{preset}_dec.wav"
                results.append(decode_check([TAKC_BIN, "-d", "-overwrite", "-q", tak_path, ours_dec],
                                            wav_path, ours_dec))
                if TAKC_EXE:
                    exe_dec = f"{OUT_DIR}/{name}_{preset}_exe.wav"
                    results.append(decode_check(["wine", TAKC_EXE, "-d", "-overwrite", tak_path, exe_dec],
                                                wav_path, exe_dec, WINE_ENV))

                if any(r != "PASS" for r in results):
                    failures += 1
                print(f"{name:<16} {preset:<7} {'OK':<7} " + " ".join(f"{r:<9}" for r in results))

    print()
    print(f"Results: {total - failures}/{total} passed.")
    if failures:
        print(f"FAILED: {failures} test(s) failed.")
    else:
        print("All tests passed.")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(run())
