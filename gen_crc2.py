basis = [
    0xFB4C86, # 1
    0x0DD58A, # 2
    0xE1E693, # 4
    0x3981A1, # 8
    0x894EC5, # 16
    0xB0CF64, # 32
    0xE9D10C, # 64
    0x3B7215, # 128
]
table = []
for i in range(256):
    val = 0
    for b in range(8):
        if i & (1 << b):
            val ^= basis[b]
    table.append(val)

with open("crc.txt", "w") as f:
    f.write("constexpr std::array<uint32_t, 256> crc24_table = {\n")
    for i in range(0, 256, 8):
        f.write("    " + ", ".join(f"0x{x:06X}" for x in table[i:i+8]) + ",\n")
    f.write("};\n")
