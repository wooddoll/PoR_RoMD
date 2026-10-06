import sys
import os


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
            'text': text[:-4],
            'extra_bytes': lo_bytes,
        })
        entry_index += 1

    return entries


def print_entries(entries):
    """Print the parsed entries in a readable format."""
    if not entries:
        print("No entries found.")
        return

    print(f"Total entries: {len(entries)}")
    print()

    for entry in entries:
        extra_hex = hex(entry['extra_bytes'])
        print(f"[{entry['index']:4d}] text='{entry['text']}' extra={extra_hex}")


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python read_all_idx.py <all.idx_file_path>")
        sys.exit(1)

    entries = read_all_idx(sys.argv[1])
    if entries is not None:
        print_entries(entries)