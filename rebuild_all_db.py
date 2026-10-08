import sys
import os
import struct
import json
import base64
from datetime import datetime
from dataclasses import dataclass, field
from typing import List

# Reuse functions from read_all_idx.py
from read_all_idx import (
    read_all_idx,
    read_all_db,
    build_sections,
    Section,
    KeyEntry,
)


def serialize_section(section: Section) -> bytes:
    """
    Serializes a Section object into the all.db binary format.

    Format per section:
    - Header: 4 little-endian 32-bit integers
      [0] = Direction size
      [1] = Selection size
      [2] = Key size
      [3] = Section size (16 + direction + selection + key)
    - Direction: raw bytes
    - Selection: strings joined with '\r\n' + trailing '\r\n'
    - Key: strings joined with '\r\n' + trailing '\r\n'
    """
    # Direction: raw bytes
    direction = section.direction

    # Selection: strings joined with '\r\n' + trailing '\r\n'
    selection_bytes = b''
    if section.selections:
        selection_bytes = b'\r\n'.join(s.encode('ascii', errors='replace') for s in section.selections)
        selection_bytes += b'\r\n'

    # Key: check if key is 7 bytes, if so add ':' (0x3A) to make it 8 bytes
    key_bytes_list = []
    for key_entry in section.keys:
        key = key_entry.key
        if len(key) == 7:
            key = key + b':'
        key_bytes_list.append(key)

    key_bytes = b''
    if key_bytes_list:
        key_bytes = b'\r\n'.join(key_bytes_list)
        key_bytes += b'\r\n'

    # Header
    direction_size = len(direction)
    selection_size = len(selection_bytes)
    key_size = len(key_bytes)
    section_size = 16 + direction_size + selection_size + key_size

    header = struct.pack('<4i', direction_size, selection_size, key_size, section_size)

    return header + direction + selection_bytes + key_bytes


def rebuild_all_db(combined, output_db_path):
    """
    Rebuilds all.db from combined Section data.
    Sections are written sequentially without using the original addresses.
    """
    if not combined:
        print("No sections to write.")
        return None

    # Calculate sequential addresses
    sections_data = []
    current_offset = 0

    for section in combined:
        data = serialize_section(section)
        new_address = current_offset
        sections_data.append({
            'section': section.name,
            'original_address': section.address,
            'new_address': new_address,
            'size': len(data),
            'data': data,
        })
        current_offset += len(data)

    # Write all.db
    with open(output_db_path, 'wb') as f:
        for sec in sections_data:
            f.write(sec['data'])

    print(f"Rebuilt DB file: {output_db_path}")
    print(f"Total size: {current_offset} bytes")
    print(f"Total sections: {len(sections_data)}")
    print()

    return sections_data


def write_address_verification(sections_data, output_path):
    """
    Writes a verification list comparing original addresses with new sequential addresses.
    """
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write("=== Address Verification List ===\n")
        f.write(f"Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
        f.write(f"Total sections: {len(sections_data)}\n")
        f.write("\n")
        f.write(f"{'Index':<6} {'Section':<20} {'Original Addr':<16} {'New Addr':<16} {'Size':<10} {'Match':<6}\n")
        f.write("-" * 80 + "\n")

        for i, sec in enumerate(sections_data):
            orig = sec['original_address']
            new = sec['new_address']
            match = "YES" if orig == new else "NO"
            f.write(f"{i:<6} {sec['section']:<20} 0x{orig:08X}      0x{new:08X}      {sec['size']:<10} {match:<6}\n")

        f.write("\n")
        f.write("=== Summary ===\n")
        matches = sum(1 for s in sections_data if s['original_address'] == s['new_address'])
        f.write(f"Addresses matching original: {matches}/{len(sections_data)}\n")
        f.write(f"Addresses different: {len(sections_data) - matches}/{len(sections_data)}\n")

    print(f"Address verification list written: {output_path}")


def write_all_idx(sections_data, output_idx_path):
    """
    Writes a new all.idx file with the new sequential addresses.
    """
    with open(output_idx_path, 'wb') as f:
        for sec in sections_data:
            # Section name + '.all' (null-terminated, up to 16 bytes)
            name_bytes = (sec['section'] + '.all').encode('ascii')
            f.write(name_bytes)
            #f.write(b'\x00')  # null terminator 없음

            # 8 bytes: 4 zero bytes + 4-byte little-endian address
            f.write(b'\x00\x00\x00\x00')
            f.write(struct.pack('<I', sec['new_address']))

    print(f"Rebuilt idx file: {output_idx_path}")


def save_combined_to_json(combined, output_json_path):
    """
    Saves the combined Section data to a JSON file.

    Encoding:
    - key: latin-1 encoded string
    - direction: base64 encoded string
    - dialogue, speaker, desc: plain strings
    """
    data = []
    for section in combined:
        sec_data = {
            'name': section.name,
            'address': section.address,
            'direction': base64.b64encode(section.direction).decode('ascii'),
            'selections': section.selections,
            'keys': [],
        }
        for key_entry in section.keys:
            sec_data['keys'].append({
                'key': key_entry.key.decode('latin-1'),
                'dialogue': key_entry.dialogue,
                'speaker': key_entry.speaker,
                'desc': key_entry.desc,
            })
        data.append(sec_data)

    with open(output_json_path, 'w', encoding='utf-8') as f:
        json.dump(data, f, ensure_ascii=False, indent=2)

    print(f"Combined data saved to JSON: {output_json_path}")
    print(f"Total sections: {len(data)}")


def load_combined_from_json(json_path):
    """
    Loads combined Section data from a JSON file.

    Decoding:
    - key: latin-1 decoded to bytes
    - direction: base64 decoded to bytes
    """
    with open(json_path, 'r', encoding='utf-8') as f:
        data = json.load(f)

    combined = []
    for sec_data in data:
        section = Section(
            name=sec_data['name'],
            address=sec_data['address'],
            direction=base64.b64decode(sec_data['direction']),
            selections=sec_data['selections'],
        )
        for key_data in sec_data['keys']:
            section.keys.append(KeyEntry(
                key=key_data['key'].encode('latin-1'),
                dialogue=key_data['dialogue'],
                speaker=key_data['speaker'],
                desc=key_data['desc'],
            ))
        combined.append(section)

    print(f"Combined data loaded from JSON: {json_path}")
    print(f"Total sections: {len(combined)}")

    return combined


def restore_from_combined(combined, output_dir):
    """
    Restores all.db, all.idx, and .por files from combined Section data.
    """
    os.makedirs(output_dir, exist_ok=True)

    # Step 1: Rebuild all.db
    print("=== Restoring all.db ===")
    output_db_path = os.path.join(output_dir, 'all.db')
    sections_data = rebuild_all_db(combined, output_db_path)
    if sections_data is None:
        print("Failed to rebuild all.db")
        return False
    print()

    # Step 2: Write new all.idx
    print("=== Restoring all.idx ===")
    output_idx_path = os.path.join(output_dir, 'all.idx')
    write_all_idx(sections_data, output_idx_path)
    print()

    # Step 3: Write .por files
    print("=== Restoring .por files ===")
    write_por_files(combined, output_dir)
    print()

    # Step 4: Write address verification list
    print("=== Writing address verification list ===")
    verification_path = os.path.join(output_dir, 'address_verification.txt')
    write_address_verification(sections_data, verification_path)
    print()

    return True


def write_por_files(sections, output_dir):
    """
    Writes .por files for each section to the specified output directory.

    .por file format (repeated):
    [dialogue key][tab][dialogue text][tab][speaker key][tab][date/scene description][tab][0x0D, 0x0A]
    The last field (date/scene description) may be absent.
    """
    if not sections:
        print("No sections to write.")
        return

    os.makedirs(output_dir, exist_ok=True)

    for section in sections:
        por_name = section.name + ".por"
        por_path = os.path.join(output_dir, por_name)

        lines = []
        for key_entry in section.keys:
            parts = [key_entry.key, key_entry.dialogue.encode('ascii', errors='replace'),
                     key_entry.speaker.encode('ascii', errors='replace'),
                     key_entry.desc.encode('ascii', errors='replace')]
            line = b'\t'.join(parts)
            lines.append(line)

        content = b'\r\n'.join(lines)
        if content:
            content += b'\r\n'

        with open(por_path, 'wb') as f:
            f.write(content)

    print(f"Total .por files written: {len(sections)}")


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage:")
        print("  python rebuild_all_db.py save <all.idx_file_path> [output_dir]")
        print("  python rebuild_all_db.py restore <combined.json_path> [output_dir]")
        sys.exit(1)

    mode = sys.argv[1]

    if mode == 'save':
        # Save mode: read all.idx/all.db/.por files, combine, and save to JSON
        if len(sys.argv) < 3:
            print("Usage: python rebuild_all_db.py save <all.idx_file_path> [output_dir]")
            sys.exit(1)

        idx_path = sys.argv[2]
        db_path = os.path.splitext(idx_path)[0] + ".db"
        idx_dir = os.path.dirname(idx_path)

        # Optional output directory
        if len(sys.argv) >= 4:
            output_dir = sys.argv[3]
        else:
            timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
            output_dir = os.path.join('.', f'rebuild_{timestamp}')

        os.makedirs(output_dir, exist_ok=True)

        # Step 1: Read all.idx
        print("=== Step 1: Reading all.idx ===")
        entries = read_all_idx(idx_path)
        if entries is None:
            print("Failed to read all.idx")
            sys.exit(1)
        print(f"Read {len(entries)} entries from all.idx")
        print()

        # Step 2: Read all.db
        print("=== Step 2: Reading all.db ===")
        sections = read_all_db(db_path, entries)
        if sections is None:
            print("Failed to read all.db")
            sys.exit(1)
        print(f"Read {len(sections)} sections from all.db")
        print()

        # Step 3: Combine with .por data
        print("=== Step 3: Combining with .por data ===")
        combined = build_sections(sections, idx_dir)
        print(f"Combined {len(combined)} sections")
        print()

        # Step 4: Save combined to JSON
        print("=== Step 4: Saving combined to JSON ===")
        json_path = os.path.join(output_dir, 'combined.json')
        save_combined_to_json(combined, json_path)
        print()

        # Step 5: Rebuild all.db
        #print("=== Step 5: Rebuilding all.db ===")
        #output_db_path = os.path.join(output_dir, 'all.db')
        #sections_data = rebuild_all_db(combined, output_db_path)
        #if sections_data is None:
        #    print("Failed to rebuild all.db")
        #    sys.exit(1)
        #print()

        # Step 6: Write address verification list
        #print("=== Step 6: Writing address verification list ===")
        #verification_path = os.path.join(output_dir, 'address_verification.txt')
        #write_address_verification(sections_data, verification_path)
        #print()

        # Step 7: Write new all.idx
        #print("=== Step 7: Writing new all.idx ===")
        #output_idx_path = os.path.join(output_dir, 'all.idx')
        #write_all_idx(sections_data, output_idx_path)
        #print()

        print("=== Done ===")
        print(f"Output directory: {output_dir}")
        print(f"  - {json_path}")
        #print(f"  - {output_db_path}")
        #print(f"  - {output_idx_path}")
        #print(f"  - {verification_path}")

    elif mode == 'restore':
        # Restore mode: load combined from JSON and restore all files
        if len(sys.argv) < 3:
            print("Usage: python rebuild_all_db.py restore <combined.json_path> [output_dir]")
            sys.exit(1)

        json_path = sys.argv[2]

        # Optional output directory
        if len(sys.argv) >= 4:
            output_dir = sys.argv[3]
        else:
            timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
            output_dir = os.path.join('.', f'restore_{timestamp}')

        # Step 1: Load combined from JSON
        print("=== Step 1: Loading combined from JSON ===")
        combined = load_combined_from_json(json_path)
        if not combined:
            print("Failed to load combined data")
            sys.exit(1)
        print()

        # Step 2: Restore all files
        print("=== Step 2: Restoring files ===")
        success = restore_from_combined(combined, output_dir)
        if not success:
            print("Failed to restore files")
            sys.exit(1)
        print()

        print("=== Done ===")
        print(f"Output directory: {output_dir}")
        print(f"  - {os.path.join(output_dir, 'all.db')}")
        print(f"  - {os.path.join(output_dir, 'all.idx')}")
        print(f"  - {os.path.join(output_dir, 'address_verification.txt')}")
        print(f"  - .por files ({len(combined)} files)")

    else:
        print(f"Unknown mode: {mode}")
        print("Usage:")
        print("  python rebuild_all_db.py save <all.idx_file_path> [output_dir]")
        print("  python rebuild_all_db.py restore <combined.json_path> [output_dir]")
        sys.exit(1)