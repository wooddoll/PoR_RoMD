import sys
import os
import struct
import json
import base64
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


def check_non_ascii(data, label, context=''):
    """
    Checks if the given string data contains any character with ASCII value >= 128.
    Only str type data is checked. Returns a list of (offset, char) tuples for non-ASCII characters found.
    """
    if not isinstance(data, str):
        return []

    found = []
    for i, ch in enumerate(data):
        if ord(ch) >= 0x80:
            found.append((i, ch))

    if found:
        print(f"  [NON-ASCII] {label}: {len(found)} character(s) >= 0x80 found")
        for offset, ch in found:
            print(f"    offset={offset} char=U+{ord(ch):04X} ({ch!r})")
        if context:
            print(f"    context: {context!r}")

    return found


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

    print(f"[all.idx] File size: {len(data)} bytes")

    entries = []
    pos = 0
    entry_index = 0

    while pos < len(data):
        # Read text (up to 12 bytes, ending with '.all')
        text_bytes = bytearray()
        while pos < len(data) and len(text_bytes) < 12:
            b = data[pos]
            if b == 0x00:
                break
            text_bytes.append(b)
            pos += 1

        text = text_bytes.decode('ascii', errors='replace')
        # Check for non-ASCII characters in section name (str type)
        check_non_ascii(text, f"Section name (entry {entry_index})", text)
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

    print(f"[all.db] File size: {len(data)} bytes")

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
        # Check for non-ASCII characters in selections (str type)
        for sel_idx, sel_str in enumerate(selections):
            check_non_ascii(sel_str, f"Selection[{sel_idx}] (section '{section_name}')")

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
            # Check for non-ASCII characters in .por file data (str type only)
            # key, dialogue, speaker, desc are bytes type - decode to str for checking
            check_non_ascii(dialogue.decode('ascii', errors='replace'), f"POR dialogue (section '{os.path.basename(por_path)}')")
            check_non_ascii(speaker.decode('ascii', errors='replace'), f"POR speaker (section '{os.path.basename(por_path)}')")
            check_non_ascii(desc.decode('ascii', errors='replace'), f"POR desc (section '{os.path.basename(por_path)}')")

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
    por_count = 0
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
            por_count += 1
            # Create KeyEntry for each key
            for key_bytes in section['keys']:
                # Find matching por entry
                matching = next((e for e in por_entries if e['key'] == key_bytes), None)
                if matching:
                    # Check for non-ASCII characters in final KeyEntry data (str type only)
                    dialogue_str = matching['dialogue'].decode('ascii', errors='replace')
                    speaker_str = matching['speaker'].decode('ascii', errors='replace')
                    desc_str = matching['desc'].decode('ascii', errors='replace')
                    check_non_ascii(dialogue_str, f"KeyEntry dialogue (section '{section['section']}')")
                    check_non_ascii(speaker_str, f"KeyEntry speaker (section '{section['section']}')")
                    check_non_ascii(desc_str, f"KeyEntry desc (section '{section['section']}')")

                    sec.keys.append(KeyEntry(
                        key=key_bytes,
                        dialogue=dialogue_str,
                        speaker=speaker_str,
                        desc=desc_str,
                    ))
                else:
                    sec.keys.append(KeyEntry(key=key_bytes))

        result.append(sec)
    
    print(f"Imported {por_count} [.por] files")
    print()
    return result


def save_combined_to_json(combined, output_json_path, exclude_fields=None, drop_empty=False):
    """
    Saves the combined Section data to a JSON file.

    Encoding:
    - key: latin-1 encoded string
    - direction: base64 encoded string
    - selections, dialogue: euc-kr encoded (stored as latin-1 in file)
    - speaker, desc: plain strings

    Args:
        combined: List of Section objects to save.
        output_json_path: Path to the output JSON file.
        exclude_fields: Optional set/list of field names to exclude from the output. The excluded fields are set to empty string (''), not deleted. Supported values: 'name', 'address', 'direction', 'selections', 'key', 'dialogue', 'speaker', 'desc'.
        drop_empty: Optional. If True, fields whose value is the string 'null' (or None) are set to empty string (''). Empty string values are left untouched. Entries with an empty key are excluded from this processing. Default False.
    """
    if exclude_fields is None:
        exclude_fields = set()
    else:
        exclude_fields = set(exclude_fields)

    def is_null(value):
        """Returns True if value is None or the string 'null' (case-insensitive)."""
        if value is None:
            return True
        if isinstance(value, str) and value.lower() == 'null':
            return True
        return False

    data = []
    for section in combined:
        # Encode selections to euc-kr bytes, then decode as latin-1 to preserve bytes
        selections = []
        for sel in section.selections:
            try:
                selections.append(sel.encode('euc-kr').decode('latin-1'))
            except (UnicodeEncodeError, UnicodeDecodeError):
                selections.append(sel)

        sec_data = {
            'name': section.name,
            'address': section.address,
            'direction': base64.b64encode(section.direction).decode('ascii'),
            'selections': selections,
            'keys': [],
        }

        # Apply excluded top-level fields: set to empty string instead of delete
        for field in list(sec_data.keys()):
            if field in exclude_fields:
                sec_data[field] = ''

        # Apply drop-empty top-level: set to empty string, only for 'null' values
        if drop_empty:
            for field in list(sec_data.keys()):
                if field != 'keys' and is_null(sec_data[field]):
                    sec_data[field] = ''

        for key_entry in section.keys:
            # Encode dialogue to euc-kr bytes, then decode as latin-1 to preserve bytes
            dialogue = key_entry.dialogue
            try:
                dialogue = dialogue.encode('euc-kr').decode('latin-1')
            except (UnicodeEncodeError, UnicodeDecodeError):
                pass

            key_data = {
                'key': key_entry.key.decode('latin-1'),
                'dialogue': dialogue,
                'speaker': key_entry.speaker,
                'desc': key_entry.desc,
            }

            # Determine if this key entry has an empty key (excluded from drop-empty processing)
            is_empty_key = (key_entry.key is None or key_entry.key == b'')

            # Apply excluded key-level fields: set to empty string instead of delete
            for field in list(key_data.keys()):
                if field in exclude_fields:
                    key_data[field] = ''

            # Apply drop-empty key-level: set to empty string, only for 'null' values, skip if empty key
            if drop_empty and not is_empty_key:
                for field in list(key_data.keys()):
                    if field != 'key' and is_null(key_data[field]):
                        key_data[field] = ''

            sec_data['keys'].append(key_data)

        data.append(sec_data)

    with open(output_json_path, 'w', encoding='latin-1') as f:
        json.dump(data, f, ensure_ascii=False, indent=2)

    print(f"Combined data saved to JSON: {output_json_path}")
    print(f"Total sections: {len(data)}")
    if exclude_fields:
        print(f"Excluded fields: {', '.join(sorted(exclude_fields))} (set to empty)")
    if drop_empty:
        print("Null fields set to empty: True")

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
            parts = [key_entry.key, key_entry.dialogue.encode('euc-kr', errors='replace'),
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
    if len(sys.argv) < 3:
        print("Usage: python read_all_idx.py <game_dir> <to_export.json> [--exclude FIELD1,FIELD2] [--drop-empty]")
        print("  <game_dir>     - Path to the game directory (e.g., '../Pool of Radiance')")
        print("  <to_export.json> - JSON file containing combined dialogue data")
        print("  --exclude     Optional. Comma-separated list of fields to set to empty string in JSON output.")
        print("                Available fields: name, address, direction, selections, key, dialogue, speaker, desc")
        print("                Example: --exclude address,direction")
        print("  --drop-empty  Optional. Set fields with 'null' values to empty string in JSON output.")
        print("                Entries with an empty key are excluded from this processing.")
        print("                Example: --drop-empty")
        sys.exit(1)

    game_dir = sys.argv[1]
    export_json_path = sys.argv[2]

    # Parse optional arguments
    exclude_fields = None
    drop_empty = False
    i = 3
    while i < len(sys.argv):
        arg = sys.argv[i]
        if arg == "--exclude" or arg == "-e":
            if i + 1 >= len(sys.argv):
                print("Error: --exclude requires a comma-separated list of fields.")
                sys.exit(1)
            exclude_fields = [f.strip() for f in sys.argv[i + 1].split(",") if f.strip()]
            i += 2
        elif arg == "--drop-empty" or arg == "-d":
            drop_empty = True
            i += 1
        else:
            print("Error: Unknown argument '" + arg + "'. Use --exclude or --drop-empty.")
            sys.exit(1)

    dialogue_dir = os.path.join(game_dir, 'data', 'Dialogue')
    idx_path = os.path.join(dialogue_dir, 'all.idx')
    db_path = os.path.join(dialogue_dir, 'all.db')

    entries = read_all_idx(idx_path)
    if entries is not None:
        #print_entries(entries)

        # Load all.db file
        sections = read_all_db(db_path, entries)
        if sections is not None:
            # Combine all.db data with .por file data
            combined = build_sections(sections, dialogue_dir)
            #print_sections(combined)

            output_dir = os.path.dirname(export_json_path)
            os.makedirs(output_dir, exist_ok=True)

            save_combined_to_json(combined, export_json_path, exclude_fields, drop_empty)
