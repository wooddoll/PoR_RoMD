import sys
import os
import struct

def read_idx_entries(idx_path):
    """Read all.idx entries."""
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

        hi_bytes = int.from_bytes(extra_bytes[:4], byteorder='little')
        lo_bytes = int.from_bytes(extra_bytes[4:], byteorder='little')

        if hi_bytes != 0:
            break

        entries.append({
            'section': text[:-4],
            'address': lo_bytes,
        })

    return entries


def verify_rebuild(original_db_path, rebuilt_db_path, idx_path):
    """Verify that the rebuilt all.db matches the original byte-for-byte."""
    with open(original_db_path, 'rb') as f:
        original_data = f.read()

    with open(rebuilt_db_path, 'rb') as f:
        rebuilt_data = f.read()

    entries = read_idx_entries(idx_path)

    print(f"Original DB size: {len(original_data)} bytes")
    print(f"Rebuilt DB size: {len(rebuilt_data)} bytes")
    print(f"Total sections: {len(entries)}")
    print()

    if len(original_data) != len(rebuilt_data):
        print(f"ERROR: File sizes differ!")
        return

    # Verify each section byte-by-byte
    mismatches = 0
    for i, entry in enumerate(entries):
        addr = entry['address']
        section_name = entry['section']

        if i + 1 < len(entries):
            end = entries[i + 1]['address']
        else:
            end = len(original_data)

        orig_section = original_data[addr:end]
        rebuilt_section = rebuilt_data[addr:end]

        if orig_section != rebuilt_section:
            mismatches += 1
            # Find first difference
            for j in range(min(len(orig_section), len(rebuilt_section))):
                if orig_section[j] != rebuilt_section[j]:
                    print(f"  MISMATCH: Section {i} '{section_name}' at offset {j} (0x{j:X})")
                    print(f"    Original: {orig_section[max(0,j-8):j+8]!r}")
                    print(f"    Rebuilt:  {rebuilt_section[max(0,j-8):j+8]!r}")
                    break
            else:
                print(f"  MISMATCH: Section {i} '{section_name}' - length differs")
                print(f"    Original len: {len(orig_section)}, Rebuilt len: {len(rebuilt_section)}")

    if mismatches == 0:
        print("ALL SECTIONS MATCH - Rebuilt DB is byte-identical to original!")
    else:
        print(f"\n{mismatches}/{len(entries)} sections have mismatches")


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python verify_rebuild.py <rebuilt_db_path>")
        sys.exit(1)

    rebuilt_db_path = sys.argv[1]
    original_db_path = r'D:\workspace\Pool of Radiance\data\Dialogue\all.db'
    idx_path = r'D:\workspace\Pool of Radiance\data\Dialogue\all.idx'

    verify_rebuild(original_db_path, rebuilt_db_path, idx_path)