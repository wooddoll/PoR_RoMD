import sys
import os
import struct

def analyze_gaps(idx_path, db_path):
    """Analyze the gap between section addresses and header section_size."""
    # Read all.idx
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

    # Read all.db
    with open(db_path, 'rb') as f:
        db_data = f.read()

    print(f"DB file size: {len(db_data)} bytes")
    print(f"Total entries: {len(entries)}")
    print()

    # Analyze gaps
    print(f"{'Index':<6} {'Section':<20} {'Address':<12} {'Next Addr':<12} {'Gap':<8} {'Header Size':<12} {'Diff':<8}")
    print("-" * 90)

    total_gap = 0
    total_header = 0
    diff_count = 0

    for i, entry in enumerate(entries):
        address = entry['address']
        section_name = entry['section']

        if i + 1 < len(entries):
            next_address = entries[i + 1]['address']
            gap = next_address - address
        else:
            gap = len(db_data) - address

        # Read header
        if address + 16 <= len(db_data):
            header = struct.unpack('<4i', db_data[address:address + 16])
            header_size = header[3]
        else:
            header_size = -1

        diff = gap - header_size
        if diff != 0:
            diff_count += 1

        total_gap += gap
        total_header += header_size if header_size > 0 else 0

        if diff != 0 or i < 5:
            print(f"{i:<6} {section_name:<20} 0x{address:08X}   0x{next_address if i+1 < len(entries) else len(db_data):08X}    {gap:<8} {header_size:<12} {diff:<8}")

    print()
    print(f"Total gap sum: {total_gap}")
    print(f"Total header size sum: {total_header}")
    print(f"Total diff: {total_gap - total_header}")
    print(f"Sections with non-zero diff: {diff_count}/{len(entries)}")

    # Show distribution of diffs
    from collections import Counter
    diffs = []
    for i, entry in enumerate(entries):
        address = entry['address']
        if i + 1 < len(entries):
            gap = entries[i + 1]['address'] - address
        else:
            gap = len(db_data) - address

        if address + 16 <= len(db_data):
            header = struct.unpack('<4i', db_data[address:address + 16])
            header_size = header[3]
        else:
            header_size = -1

        diff = gap - header_size
        diffs.append(diff)

    print(f"\nDiff distribution: {Counter(diffs)}")


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python analyze_gap.py <all.idx_file_path>")
        sys.exit(1)

    idx_path = sys.argv[1]
    db_path = os.path.splitext(idx_path)[0] + ".db"
    analyze_gaps(idx_path, db_path)