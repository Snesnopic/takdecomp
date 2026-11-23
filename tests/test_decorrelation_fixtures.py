# decoder vs takc.exe on stereo decorrelation beyond int16: modes 6/7 saturate the history, modes 4/5 wrap the source
# usage: TAKC_BIN=./build/bin/takc python3 tests/test_decorrelation_fixtures.py

import hashlib
import os
import subprocess
import sys
import tempfile
import wave

TAKC_BIN = os.environ.get("TAKC_BIN", "./build/bin/takc")
DATA_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "data", "decorrelation")

# written by our encoder before 2026-11-14 from test_loud_stereo.py signals (-p2);
# md5 is of the PCM that takc.exe 2.3.3 decodes from each file
FIXTURES = {
    "mode4_24b.tak": ((2, 3, 44100, 88200), "7cb80cd4422972dcda91af3b0ab21425"),
    "mode5_24b.tak": ((2, 3, 44100, 88200), "e87fc4f3f485a686944ba2b79b466cfd"),
    "mode6_16b.tak": ((2, 2, 44100, 88200), "c2ff1311de423247206d55252e5f2e80"),
    "mode7_16b.tak": ((2, 2, 44100, 88200), "b6deae354e3448296c6ca7dcc6f6b4eb"),
}


def run() -> int:
    failures = 0
    with tempfile.TemporaryDirectory() as tmp:
        for name, (params, md5) in FIXTURES.items():
            dec = os.path.join(tmp, name[:-4] + ".wav")
            res = subprocess.run([TAKC_BIN, "-d", "-overwrite", "-q", os.path.join(DATA_DIR, name), dec],
                                 capture_output=True, text=True, timeout=120)
            if res.returncode != 0 or not os.path.exists(dec):
                print(f"{name:<16} FAIL (decoder exit {res.returncode})")
                failures += 1
                continue
            with wave.open(dec, "rb") as w:
                got_params = w.getparams()[:4]
                got_md5 = hashlib.md5(w.readframes(w.getnframes())).hexdigest()
            ok = got_params == params and got_md5 == md5
            failures += not ok
            print(f"{name:<16} {'PASS' if ok else 'BAD'}")
            if not ok:
                print(f"    expected {params} {md5}, got {got_params} {got_md5}")

    print()
    print(f"Results: {len(FIXTURES) - failures}/{len(FIXTURES)} passed.")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(run())
