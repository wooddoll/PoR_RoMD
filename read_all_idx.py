import sys
import os
import struct


def read_all_idx(file_path):
    """
    Reads an all.idx file and parses its contents.

    Format pattern (repeated):
    - Text (up to 16 bytes, ending with '.all')
    - 8 bytes
    """
    try:
        with open(file_path, 'rb') as f:
            data = f.read()
    except FileNotFoundError:
        print(f"Error: File '{file_path}' not found.")
        return None
    except Exception as e:
        print(f"Error reading file: {e}")
        return None

    print(f"File size: {len(data)} bytes")
    print()

    entries = []
    pos = 0
    entry_index = 0

    while pos < len(data):
        # Read text (up to 16 bytes, ending with '.all')
        text_bytes = bytearray()
        while pos < len(data) and len(text_bytes) < 16:
            b = data[pos]
            if b == 0x00:
                break
            text_bytes.append(b)
            pos += 1

        text = text_bytes.decode('ascii', errors='replace')
        #check text ends '.all'abs
        if not text.endswith('.all'):
            print(f"Warning: Incomplete text ends with .all")
            break

        # Read 8 bytes
        if pos + 8 > len(data):
            print(f"Warning: Incomplete 8-byte field at offset {pos}")
            break
        extra_bytes = data[pos:pos + 8]
        pos += 8

        hi_bytes = int.from_bytes(extra_bytes[:4], byteorder='little')
        lo_bytes = int.from_bytes(extra_bytes[4:], byteorder='little')
        
        # check hi is zero
        if hi_bytes != 0:
            print(f"Warning: first 4 bytes should be zero")
            break

        entries.append({
            'index': entry_index,
            'section': text[:-4],
            'address': lo_bytes,
        })
        entry_index += 1

    return entries


def read_all_db(db_path, idx_entries):
    """
    Reads an all.db file using addresses from all.idx entries.

    Each section starts at the address from all.idx.
    Format per section:
    - Header: 4 little-endian 32-bit integers
    - bytes_array of size = header[0]
    - Multiple strings
    """
    try:
        with open(db_path, 'rb') as f:
            data = f.read()
    except FileNotFoundError:
        print(f"Error: File '{db_path}' not found.")
        return None
    except Exception as e:
        print(f"Error reading file: {e}")
        return None

    print(f"DB file size: {len(data)} bytes")
    print()

    sections = []
    for i, entry in enumerate(idx_entries):
        address = entry['address']
        section_name = entry['section']

        # Determine section range: from this address to the next section's address
        if i + 1 < len(idx_entries):
            next_address = idx_entries[i + 1]['address']
            section_end = min(next_address, len(data))
        else:
            section_end = len(data)

        if address >= len(data):
            print(f"Warning: Section '{section_name}' address {address} is out of bounds.")
            continue

        # Read header: 4 little-endian 32-bit integers
        if address + 16 > section_end:
            print(f"Warning: Section '{section_name}' header is incomplete.")
            continue

        header = struct.unpack('<4i', data[address:address + 16])
        bytes_size = header[0]

        # Read bytes_array of size header[0]
        bytes_start = address + 16
        bytes_end = bytes_start + bytes_size
        if bytes_end > section_end:
            print(f"Warning: Section '{section_name}' bytes_array is out of bounds.")
            continue

        bytes_array = data[bytes_start:bytes_end]

        # Read strings after bytes_array, terminated by '\r\n' (0x0D 0x0A)
        strings = []
        pos = bytes_end
        while pos < section_end:
            # Find '\r\n' terminator
            end = data.find(b'\r\n', pos, section_end)
            if end == -1:
                # No more strings in this section
                break
            string_bytes = data[pos:end]
            if string_bytes:
                try:
                    strings.append(string_bytes.decode('ascii', errors='replace'))
                except:
                    strings.append(string_bytes.hex())
            pos = end + 2  # Skip '\r\n'

        sections.append({
            'section': section_name,
            'address': address,
            'header': header,
            'bytes_array': bytes_array,
            'strings': strings,
        })

    return sections


def print_sections(sections):
    """Print the parsed sections in a readable format."""
    if not sections:
        print("No sections found.")
        return

    print(f"Total sections: {len(sections)}")
    print()

    for section in sections:
        print(f"=== Section: {section['section']} (address: {section['address']}) ===")
        print(f"  Header: {section['header']}")
        print(f"  Bytes array size: {len(section['bytes_array'])}")
        print(f"  Strings ({len(section['strings'])}):")
        for i, s in enumerate(section['strings']):
            print(f"    [{i:4d}] {s}")
        print()


def print_entries(entries):
    """Print the parsed entries in a readable format."""
    if not entries:
        print("No entries found.")
        return

    print(f"Total entries: {len(entries)}")
    print()

    for entry in entries:
        extra_hex = hex(entry['address'])
        print(f"[{entry['index']:4d}] section='{entry['section']}' address={extra_hex}")


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python read_all_idx.py <all.idx_file_path>")
        sys.exit(1)

    idx_path = sys.argv[1]
    db_path = os.path.splitext(idx_path)[0] + ".db"

    entries = read_all_idx(idx_path)
    if entries is not None:
        print_entries(entries)

        # Load all.db file
        sections = read_all_db(db_path, entries)
        if sections is not None:
            print_sections(sections)