import sys, struct
with open(sys.argv[1], 'rb') as f:
    riff = f.read(12)
    while True:
        header = f.read(8)
        if len(header) < 8: break
        cid, csize = struct.unpack('<4sI', header)
        print(f"Chunk: {cid}, size: {csize}")
        f.read(csize)
