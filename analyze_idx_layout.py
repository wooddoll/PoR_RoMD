import struct

idx_path = r'D:\workspace\Pool of Radiance\data\Dialogue\all.idx'

with open(idx_path, 'rb') as f:
    data = f.read()

print(f"File size: {len(data)} bytes")
print()

# Parse first 10 entries and show raw bytes
pos = 0
for i in range(10):
    start = pos
    # Read text until 0x00 or 16 bytes
    text_bytes = bytearray()
    while pos < len(data) and len(text_bytes) < 16:
        b = data[pos]
        if b == 0x00:
            break
        text_bytes.append(b)
        pos += 1
    # Skip null terminator if present
    if pos < len(data) and data[pos] == 0x00:
        pos += 1
    # Read 8 bytes
    extra = data[pos:pos+8]
    pos += 8
    end = pos

    text = bytes(text_bytes).decode('ascii', errors='replace')
    lo = int.from_bytes(extra[4:], 'little')
    print(f"[{i}] '{text}' addr=0x{lo:X} raw_bytes={data[start:end].hex()} len={end-start}")
    print(f"    text_len={len(text_bytes)} total_len={end-start}")

print()
print("Total entries:", 27222 // 20, "if fixed 20 bytes per entry")
print("Total entries:", 27222 // 19, "if fixed 19 bytes per entry")
print("Remainder 27222 % 20 =", 27222 % 20)
print("Remainder 27222 % 19 =", 27222 % 19)