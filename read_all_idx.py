import sys
import os
import struct
from datetime import datetime
from dataclasses import dataclass, field
from typing import List


@dataclass
class KeyEntry:
    """Dialogue key entry with associated data."""
    key: bytes = b''
    dialogue: str = ''
    speaker: str = ''
    desc: str = ''


@dataclass
class Section:
    """Combined section structure."""
    name: str = ''
    address: int = 0
    direction: bytes = b''
    selections: List[str] = field(default_factory=list)
    keys: List[KeyEntry] = field(default_factory=list)


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
    - Header: 4 little-endian 32-bit integers (Direction size, Selection size, Key size, total size)
    - Direction: bytes array of size = header[0]
    - Selection: strings of size = header[1], each terminated by '\r\n' (may be absent)
    - Key: strings of size = header[2], each terminated by '\r\n'
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

        # Read header: 4 little-endian 32-bit integers (Direction, Selection, Key, total sizes)
        if address + 16 > section_end:
            print(f"Warning: Section '{section_name}' header is incomplete.")
            continue

        header = struct.unpack('<4i', data[address:address + 16])
        direction_size = header[0]   # Direction bytes size
        selection_size = header[1]   # Selection strings size (may be 0)
        key_size = header[2]         # Key strings size
        section_size = header[3]     # Section size
        if (16+direction_size+selection_size+key_size) != section_size:
            print(f"Warning: Section '{section_name}' Header validation failed.")
            continue

        # Read Direction: bytes array of size header[0]
        direction_start = address + 16
        direction_end = direction_start + direction_size
        if direction_end > section_end:
            print(f"Warning: Section '{section_name}' Direction is out of bounds.")
            continue

        direction = data[direction_start:direction_end]

        # Helper to parse strings terminated by '\r\n' within a byte range
        def parse_strings(start, end):
            strings = []
            pos = start
            while pos < end:
                # Find '\r\n' terminator within the range
                term = data.find(b'\r\n', pos, end)
                if term == -1:
                    break
                string_bytes = data[pos:term]
                if string_bytes:
                    strings.append(string_bytes)
                pos = term + 2  # Skip '\r\n'
            return strings

        # Read Selection: strings of size header[1] (may be absent)
        selection_start = direction_end
        selection_end = selection_start + selection_size
        if selection_end > section_end:
            print(f"Warning: Section '{section_name}' Selection is out of bounds.")
            continue
        _selections = parse_strings(selection_start, selection_end)
        selections = [b.decode('ascii', errors='replace') for b in _selections]

        # Read Key: byte arrays of size header[2]
        key_start = selection_end
        key_end = key_start + key_size
        if key_end > section_end:
            print(f"Warning: Section '{section_name}' Key is out of bounds.")
            continue
        key_bytes_list = parse_strings(key_start, key_end)

        # Clean up keys: remove trailing ':' (0x3A) if present
        for idx, key in enumerate(key_bytes_list):
            if key.endswith(b':'):
                key_bytes_list[idx] = key[:-1]

        sections.append({
            'section': section_name,
            'address': address,
            'header': header,
            'direction': direction,
            'selections': selections,
            'keys': key_bytes_list,
        })

    return sections


def read_por_file(por_path, keys):
    """
    Reads a .por file and extracts dialogue entries matching the given keys.

    .por file format (repeated):
    [dialogue key][tab][dialogue text][tab][speaker key][tab][date/scene description][tab][0x0D, 0x0A]
    The last field (date/scene description) may be absent.
    """
    try:
        with open(por_path, 'rb') as f:
            data = f.read()
    except FileNotFoundError:
        print(f"Error: File '{por_path}' not found.")
        return None
    except Exception as e:
        print(f"Error reading file: {e}")
        return None

    # Split by lines (each line ends with \r\n) - keep as bytes
    lines = data.split(b'\r\n')

    entries = []
    for line in lines:
        if not line:
            continue
        # Split by tab (0x09)
        parts = line.split(b'\t')
        if len(parts) < 2:
            continue

        key = parts[0]
        dialogue = parts[1] if len(parts) > 1 else b''
        speaker = parts[2] if len(parts) > 2 else b''
        desc = parts[3] if len(parts) > 3 else b''

        # Only include entries whose key is in the keys list
        if key in keys:
            entries.append({
                'key': key,
                'dialogue': dialogue,
                'speaker': speaker,
                'desc': desc,
            })

    return entries


def build_sections(sections, idx_dir):
    """
    Combines all.db section data with .por file data into a single Section structure.
    """
    result = []
    for section in sections:
        sec = Section(
            name=section['section'],
            address=section['address'],
            direction=section['direction'],
            selections=section['selections'],
        )

        # Load .por file for this section
        por_name = section['section'] + ".por"
        por_path = os.path.join(idx_dir, por_name)
        por_entries = read_por_file(por_path, section['keys'])

        if por_entries is not None:
            # Create KeyEntry for each key
            for key_bytes in section['keys']:
                # Find matching por entry
                matching = next((e for e in por_entries if e['key'] == key_bytes), None)
                if matching:
                    sec.keys.append(KeyEntry(
                        key=key_bytes,
                        dialogue=matching['dialogue'].decode('ascii', errors='replace'),
                        speaker=matching['speaker'].decode('ascii', errors='replace'),
                        desc=matching['desc'].decode('ascii', errors='replace'),
                    ))
                else:
                    sec.keys.append(KeyEntry(key=key_bytes))

        result.append(sec)

    return result


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
            # Remove trailing empty fields
            #while parts and not parts[-1]:
            #    parts.pop()
            line = b'\t'.join(parts)
            lines.append(line)

        content = b'\r\n'.join(lines)
        if content:
            content += b'\r\n'

        with open(por_path, 'wb') as f:
            f.write(content)

        #print(f"  Written: {por_path}")

    print(f"Total .por files written: {len(sections)}")


def print_sections(sections):
    """Print the combined Section structures in a readable format."""
    if not sections:
        print("No sections found.")
        return

    print(f"Total sections: {len(sections)}")
    print()

    for section in sections:
        print(f"=== Section: {section.name} (address: {section.address}) ===")
        print(f"  Direction size: {len(section.direction)}")
        print(f"  Selections ({len(section.selections)}):")
        for i, s in enumerate(section.selections):
            print(f"    [{i:4d}] {s}")
        print(f"  Keys ({len(section.keys)}):")
        for i, k in enumerate(section.keys):
            print(f"    [{i:4d}] key={k.key!r} dialogue='{k.dialogue}' "
                  f"speaker='{k.speaker}' desc='{k.desc}'")
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
    idx_dir = os.path.dirname(idx_path)

    entries = read_all_idx(idx_path)
    if entries is not None:
        #print_entries(entries)

        # Load all.db file
        sections = read_all_db(db_path, entries)
        if sections is not None:
            # Combine all.db data with .por file data
            combined = build_sections(sections, idx_dir)
            #print_sections(combined)

            # Create output directory with current date/time
            timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
            output_dir = os.path.join('.', 'dump')
            os.makedirs(output_dir, exist_ok=True)
            print(f"Output directory: {output_dir}")
            print()

            # Write .por files
            write_por_files(combined, output_dir)