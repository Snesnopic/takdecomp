import sys
import struct

with open('tests/short2.pcm', 'rb') as f1, open('tests/ffmpeg_short2.pcm', 'rb') as f2:
    d1 = f1.read()
    d2 = f2.read()

for i in range(len(d1)//4):
    l1, r1 = struct.unpack('<hh', d1[i*4:i*4+4])
    l2, r2 = struct.unpack('<hh', d2[i*4:i*4+4])
    if l1 != l2 or r1 != r2:
        print(f"Mismatch at sample {i}: file1=({l1}, {r1}) file2=({l2}, {r2}) diff=({l1-l2}, {r1-r2})")
        if i > 4160 + 10:
            break
