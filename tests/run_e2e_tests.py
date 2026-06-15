import os
import sys
import glob
import subprocess
import wave
import hashlib
import tempfile

OUR_ENC = os.path.abspath(os.path.join(os.path.dirname(__file__), "../build/bin/takenc"))
OUR_DEC = os.path.abspath(os.path.join(os.path.dirname(__file__), "../build/bin/takdec"))
THEIR_CLI = "WINEDEBUG=-all wine /Users/giuseppefrancione/Downloads/TAK_2.3.3/Applications/takc.exe"
FFMPEG = "ffmpeg"

def get_wav_pcm_hash(wav_path):
    try:
        with wave.open(wav_path, 'rb') as w:
            frames = w.readframes(w.getnframes())
            return hashlib.md5(frames).hexdigest()
    except Exception as e:
        return f"ERROR: {e}"

def run_cmd(cmd, cwd=None):
    try:
        res = subprocess.run(cmd, shell=True, capture_output=True, text=True, cwd=cwd, timeout=15, input="y\n")
        if res.returncode != 0:
            print(f"  [!] Command failed: {cmd}")
            if res.stderr:
                print(f"      {res.stderr.strip()}")
            return False
        return True
    except subprocess.TimeoutExpired:
        print(f"  [!] Command timed out: {cmd}")
        return False

def test_file(wav_path):
    print(f"\nTesting: {os.path.basename(wav_path)}", flush=True)
    
    with tempfile.TemporaryDirectory() as tmpdir:
        short_wav = os.path.join(tmpdir, "short.wav")
        try:
            subprocess.run(f'{FFMPEG} -y -i "{wav_path}" -t 1 "{short_wav}"', shell=True, capture_output=True, timeout=10)
        except subprocess.TimeoutExpired:
            print(f"  [!] FFmpeg timeout on {wav_path}")
            return False
        
        orig_hash = get_wav_pcm_hash(short_wav)
        if "ERROR" in orig_hash:
            print(f"  [!] Failed to read short WAV: {orig_hash}")
            return False
            
        our_tak = "our.tak"
        their_tak = "their.tak"
        their_wav = "their_from_ours.wav"
        ours_wav = "ours_from_theirs.wav"
        ffmpeg_wav = "ffmpeg.wav"
        roundtrip_wav = "our_roundtrip.wav"

        a_pass = False
        if run_cmd(f'"{OUR_ENC}" -p2 "short.wav" "{our_tak}"', cwd=tmpdir):
            if run_cmd(f'{THEIR_CLI} -d "{our_tak}" "{their_wav}"', cwd=tmpdir):
                if get_wav_pcm_hash(os.path.join(tmpdir, their_wav)) == orig_hash:
                    a_pass = True

        b_pass = False
        if run_cmd(f'{THEIR_CLI} -e -p2 "short.wav" "{their_tak}"', cwd=tmpdir):
            if run_cmd(f'"{OUR_DEC}" "{their_tak}" "{ours_wav}"', cwd=tmpdir):
                if get_wav_pcm_hash(os.path.join(tmpdir, ours_wav)) == orig_hash:
                    b_pass = True

        c_pass = False
        if os.path.exists(os.path.join(tmpdir, our_tak)):
            if run_cmd(f'{FFMPEG} -y -i "{our_tak}" "{ffmpeg_wav}"', cwd=tmpdir):
                if get_wav_pcm_hash(os.path.join(tmpdir, ffmpeg_wav)) == orig_hash:
                    c_pass = True

        d_pass = False
        if os.path.exists(os.path.join(tmpdir, our_tak)):
            if run_cmd(f'"{OUR_DEC}" "{our_tak}" "{roundtrip_wav}"', cwd=tmpdir):
                if get_wav_pcm_hash(os.path.join(tmpdir, roundtrip_wav)) == orig_hash:
                    d_pass = True

        def fmt(p): return "\033[92mPASS\033[0m" if p else "\033[91mFAIL\033[0m"
        print(f"  Test A (Our Enc -> Their Dec): {fmt(a_pass)}")
        print(f"  Test B (Their Enc -> Our Dec): {fmt(b_pass)}")
        print(f"  Test C (Our Enc -> FFmpeg Dec): {fmt(c_pass)}")
        print(f"  Test D (Our Enc -> Our Dec):   {fmt(d_pass)}")
        
        return all([a_pass, b_pass, c_pass, d_pass])

def main():
    search_dir = "/Users/giuseppefrancione/Music/FREAC Output/"
    files = glob.glob(os.path.join(search_dir, "*.wav"))
    
    passed = 0
    total = len(files)
    print(f"Starting E2E verification on all {total} WAV files...", flush=True)
    for f in files:
        if test_file(f):
            passed += 1
            
    print(f"\n==========================================")
    print(f"E2E Verification Complete: {passed}/{total} passed.")
    print(f"==========================================")
    if passed != total:
        sys.exit(1)
            
if __name__ == "__main__":
    main()
