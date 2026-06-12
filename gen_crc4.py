def bswap24(x):
    return ((x & 0xFF) << 16) | (x & 0xFF00) | ((x >> 16) & 0xFF)

table = []
poly = 0x864CFB
for i in range(256):
    c = i << 16
    for j in range(8):
        if c & 0x800000:
            c = ((c << 1) ^ poly) & 0xFFFFFF
        else:
            c = (c << 1) & 0xFFFFFF
    table.append(bswap24(c))

with open("crc.txt", "w") as f:
    f.write("constexpr std::array<uint32_t, 256> crc24_table = {\n")
    for i in range(0, 256, 8):
        f.write("    " + ", ".join(f"0x{x:06X}" for x in table[i:i+8]) + ",\n")
    f.write("};\n")
