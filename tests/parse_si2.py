import sys
h = "028c376a0b00404d"
b = bytes.fromhex(h)
print(list(b))

def get_bits(n, bit_stream):
    res = 0
    for i in range(n):
        res |= (next(bit_stream) << i)
    return res

def bit_gen():
    for byte in b:
        for i in range(8):
            yield (byte >> i) & 1

bg = bit_gen()
codec = get_bits(2, bg)
_ = get_bits(1, bg)
fst = get_bits(4, bg)
ts = get_bits(35, bg)

print(f"ts: {ts}")
