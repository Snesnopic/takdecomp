import re
with open('/tmp/ffmpeg/libavcodec/takdec.c', 'r') as f:
    text = f.read()

matches = re.findall(r'\{\s*0x[0-9A-Fa-f]+,\s*0x[0-9A-Fa-f]+,\s*0x[0-9A-Fa-f]+,\s*0x[0-9A-Fa-f]+,\s*0x[0-9A-Fa-f]+\s*\}', text)
for m in matches:
    parts = m.replace('{', '').replace('}', '').split(',')
    init = int(parts[0].strip(), 16)
    escape = int(parts[1].strip(), 16)
    scale = int(parts[2].strip(), 16)
    aescape = int(parts[3].strip(), 16)
    bias = int(parts[4].strip(), 16)
    limit = 1 << init
    if aescape >= limit:
        print(f"init={init}, limit={limit}, escape={escape}, scale={scale}, aescape={aescape}, bias={bias}")
print(f"Total xcodes: {len(matches)}")
