import struct
import os

idx_path = r'D:\workspace\Pool of Radiance\data\Dialogue\all.idx'
db_path = r'D:\workspace\Pool of Radiance\data\Dialogue\all.db'

with open(idx_path, 'rb') as f:
    idx_data = f.read()

entries = []
pos = 0
while pos < len(idx_data):
    text_bytes = bytearray()
    while pos < len(idx_data) and len(text_bytes) < 16:
        b = idx_data[pos]
        if b == 0x00:
            break
        text_bytes.append(b)
        pos += 1
    text = text_bytes.decode('ascii', errors='replace')
    if not text.endswith('.all'):
        break
    if pos + 8 > len(idx_data):
        break
    extra_bytes = idx_data[pos:pos + 8]
    pos += 8
    hi = int.from_bytes(extra_bytes[:4], byteorder='little')
    lo = int.from_bytes(extra_bytes[4:], byteorder='little')
    if hi != 0:
        break
    entries.append({'section': text[:-4], 'address': lo})

with open(db_path, 'rb') as f:
    db_data = f.read()

# Check first few sections in detail
for i in range(5):
    entry = entries[i]
    addr = entry['address']
    if i + 1 < len(entries):
        end = entries[i+1]['address']
    else:
        end = len(db_data)
    
    header = struct.unpack('<4i', db_data[addr:addr+16])
    dir_size, sel_size, key_size, total_size = header
    
    print(f'=== Section {i}: {entry["section"]} (addr=0x{addr:X}) ===')
    print(f'  Header: dir={dir_size}, sel={sel_size}, key={key_size}, total={total_size}')
    print(f'  Actual gap: {end - addr}')
    
    # Direction bytes
    dir_start = addr + 16
    dir_end = dir_start + dir_size
    direction = db_data[dir_start:dir_end]
    print(f'  Direction ({dir_size} bytes): {direction[:80]!r}')
    
    # Selection bytes
    sel_start = dir_end
    sel_end = sel_start + sel_size
    selection = db_data[sel_start:sel_end]
    print(f'  Selection ({sel_size} bytes): {selection[:120]!r}')
    
    # Key bytes
    key_start = sel_end
    key_end = key_start + key_size
    key = db_data[key_start:key_end]
    print(f'  Key ({key_size} bytes): {key[:120]!r}')
    print()