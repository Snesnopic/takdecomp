table = []
poly = 0xFB4C86
for i in range(256):
    crc = i
    for j in range(8):
        if crc & 1:
            crc = (crc >> 1) ^ poly
        else:
            crc >>= 1
    table.append(crc)

with open("crc.txt", "w") as f:
    f.write("constexpr std::array<uint32_t, 256> crc24_table = {\n")
    for i in range(0, 256, 8):
        f.write("    " + ", ".join(f"0x{x:06X}" for x in table[i:i+8]) + ",\n")
    f.write("};\n")
