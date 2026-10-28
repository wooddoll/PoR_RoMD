#!/usr/bin/env python3
"""
STR <-> JSON <-> Glossary Converter for Pool of Radiance

Converts .str files (0x0A-separated strings) to a single JSON file and back,
and also converts between the JSON format and a glossary format.

Usage:
    python str_json_converter.py to-json [--str-dir DIR] [--json-file FILE]
    python str_json_converter.py from-json [--str-dir DIR] [--json-file FILE]
    python str_json_converter.py to-glossary [--json-file FILE] [--glossary-file FILE]
    python str_json_converter.py from-glossary [--json-file FILE] [--glossary-file FILE]

Options:
    --str-dir DIR        Directory containing .str files (default: D:\\workspace\\Pool of Radiance\\data\\str)
    --json-file FILE     JSON file path (default: str_data.json)
    --glossary-file FILE Glossary JSON file path (default: glossary.json)
"""

import argparse
import functools
import json
import os
import sys

DEFAULT_STR_DIR = r'E:/Games/Pool of Radiance/Data/STR'
DEFAULT_PATCH_DIR = r'E:/Games/Pool of Radiance/Data_kr/STR'
DEFAULT_JSON_FILE = 'translation/str_data.json'
DEFAULT_GLOSSARY_FILE = 'translation/str_glossary.json'

# Category mapping for glossary conversion
CATEGORY_ITEM = 'Item'
CATEGORY_MONSTER = 'Monster / Character'
CATEGORY_LOCATION = 'Location'

# Files that map to specific categories
ITEM_FILES = {'TEE'}
MONSTER_FILES = {'monsters'}


def str_to_json(str_dir: str, json_file: str) -> None:
    """Read all .str files and write them to a single JSON file."""
    if not os.path.isdir(str_dir):
        print(f'Error: Directory not found: {str_dir}', file=sys.stderr)
        sys.exit(1)

    # Collect all .str files, sorted case-insensitively by filename
    str_files = [f for f in os.listdir(str_dir) if f.lower().endswith('.str')]
    str_files.sort(key=str.lower)

    if not str_files:
        print(f'Error: No .str files found in: {str_dir}', file=sys.stderr)
        sys.exit(1)

    data = {}
    for filename in str_files:
        filepath = os.path.join(str_dir, filename)
        key = os.path.splitext(filename)[0]  # Remove .str extension

        with open(filepath, 'rb') as f:
            raw = f.read()

        # Split by 0x0A and remove trailing empty string if file ends with 0x0A
        lines = raw.split(b'\x0a')
        if lines and lines[-1] == b'':
            lines.pop()

        # Decode as ASCII (all files are pure ASCII)
        data[key] = [line.decode('euc-kr') for line in lines]

    with open(json_file, 'w', encoding='utf-8') as f:
        json.dump(data, f, ensure_ascii=False, indent=2)

    print(f'Converted {len(str_files)} .str files to {json_file}')


def json_to_str(str_dir: str, json_file: str) -> None:
    """Read the JSON file and restore all .str files."""
    if not os.path.isfile(json_file):
        print(f'Error: JSON file not found: {json_file}', file=sys.stderr)
        sys.exit(1)

    with open(json_file, 'r', encoding='utf-8') as f:
        data = json.load(f)

    if not isinstance(data, dict):
        print('Error: JSON root must be an object', file=sys.stderr)
        sys.exit(1)

    os.makedirs(str_dir, exist_ok=True)

    count = 0
    for key, lines in data.items():
        if not isinstance(lines, list):
            print(f'Error: Value for "{key}" must be a list of strings', file=sys.stderr)
            sys.exit(1)

        # Encode each line and join with 0x0A, then append trailing 0x0A
        encoded_lines = []
        for line in lines:
            if not isinstance(line, str):
                print(f'Error: Value in "{key}" must be a string', file=sys.stderr)
                sys.exit(1)
            encoded_lines.append(line.encode('euc-kr'))

        content = b'\x0a'.join(encoded_lines) + b'\x0a'

        filepath = os.path.join(str_dir, key + '.str')
        with open(filepath, 'wb') as f:
            f.write(content)
        count += 1

    print(f'Restored {count} .str files to {str_dir}')


def _get_category(file_key: str) -> str:
    """Determine the category for a given file key."""
    if file_key in ITEM_FILES:
        return CATEGORY_ITEM
    elif file_key in MONSTER_FILES:
        return CATEGORY_MONSTER
    else:
        return CATEGORY_LOCATION


def _compare_names(a: str, b: str) -> int:
    """
    Compare two names word-by-word in dictionary order (case-insensitive).
    If one name's words are a prefix of the other's, the one with MORE words
    comes first (descending word count).
    """
    words_a = a.lower().split()
    words_b = b.lower().split()

    # Compare word by word
    for wa, wb in zip(words_a, words_b):
        if wa < wb:
            return -1
        elif wa > wb:
            return 1

    # All compared words are equal; the one with more words comes first
    if len(words_a) > len(words_b):
        return -1
    elif len(words_a) < len(words_b):
        return 1
    else:
        return 0


def _compare_glossary_entries(a: tuple, b: tuple) -> int:
    """
    Compare two (name, category) tuples.
    Category is the top-level sort key (alphabetical), then name comparison.
    """
    name_a, category_a = a
    name_b, category_b = b

    # Compare category first (alphabetical)
    if category_a < category_b:
        return -1
    elif category_a > category_b:
        return 1

    # Same category, compare names
    return _compare_names(name_a, name_b)


def json_to_glossary(json_file: str, glossary_file: str) -> None:
    """Convert str_data.json to glossary format."""
    if not os.path.isfile(json_file):
        print(f'Error: JSON file not found: {json_file}', file=sys.stderr)
        sys.exit(1)

    with open(json_file, 'r', encoding='utf-8') as f:
        data = json.load(f)

    if not isinstance(data, dict):
        print('Error: JSON root must be an object', file=sys.stderr)
        sys.exit(1)

    # Build glossary: deduplicate by name, keep first occurrence
    glossary = {}
    for file_key, lines in data.items():
        if not isinstance(lines, list):
            print(f'Error: Value for "{file_key}" must be a list of strings', file=sys.stderr)
            sys.exit(1)

        category = _get_category(file_key)
        for line in lines:
            if not isinstance(line, str):
                print(f'Error: Value in "{file_key}" must be a string', file=sys.stderr)
                sys.exit(1)
            if line not in glossary:
                glossary[line] = {
                    'korean': '',
                    'category': category,
                    'description': ''
                }

    # Sort: category first (alphabetical), then word-by-word dictionary order
    # with prefix match -> more words first
    sorted_entries = sorted(
        [(name, glossary[name]['category']) for name in glossary.keys()],
        key=functools.cmp_to_key(_compare_glossary_entries)
    )

    # Rebuild ordered glossary
    ordered_glossary = {name: glossary[name] for name, _ in sorted_entries}

    with open(glossary_file, 'w', encoding='utf-8') as f:
        json.dump(ordered_glossary, f, ensure_ascii=False, indent=2)

    print(f'Converted {json_file} to glossary {glossary_file} ({len(ordered_glossary)} unique names)')


def glossary_to_json(json_file: str, glossary_file: str) -> None:
    """Convert glossary format back to str_data.json format."""
    if not os.path.isfile(glossary_file):
        print(f'Error: Glossary file not found: {glossary_file}', file=sys.stderr)
        sys.exit(1)

    with open(glossary_file, 'r', encoding='utf-8') as f:
        glossary = json.load(f)

    if not isinstance(glossary, dict):
        print('Error: Glossary root must be an object', file=sys.stderr)
        sys.exit(1)

    # If the original str_data.json exists, use it to determine which file
    # each name belongs to (for Location category entries that may appear
    # in multiple files).
    original_data = {}
    if os.path.isfile(json_file):
        with open(json_file, 'r', encoding='utf-8') as f:
            original_data = json.load(f)

    # Build the new str_data.json structure
    new_data = {}
    for file_key, lines in original_data.items():
        if not isinstance(lines, list):
            print(f'Error: Value for "{file_key}" must be a list of strings', file=sys.stderr)
            sys.exit(1)

        new_data[file_key] = []
        for line in lines:
            if not isinstance(line, str):
                print(f'Error: Value in "{file_key}" must be a string', file=sys.stderr)
                sys.exit(1)

            new_words = glossary.get(line, {})
            if not new_words:
                print(f'Warning: "{line}" is not in glossary', file=sys.stderr)
                new_data[file_key].append(line)
            else:
                translated = new_words.get('korean', '')
                if translated:
                    new_data[file_key].append(translated)
                else:
                    new_data[file_key].append(line)

    with open(json_file, 'w', encoding='utf-8') as f:
        json.dump(new_data, f, ensure_ascii=False, indent=2)

    print(f'Converted glossary {glossary_file} to {json_file} ({len(new_data)} files)')

def rebuild_glossary_to_json(str_dir: str, patch_dir: str, glossary_file: str) -> None:
    """Read all .str files and write them to a single JSON file."""
    if not os.path.isdir(str_dir):
        print(f'Error: Directory not found: {str_dir}', file=sys.stderr)
        sys.exit(1)

    # Collect all .str files, sorted case-insensitively by filename
    str_files = [f for f in os.listdir(str_dir) if f.lower().endswith('.str')]
    str_files.sort(key=str.lower)

    if not str_files:
        print(f'Error: No .str files found in: {str_dir}', file=sys.stderr)
        sys.exit(1)

    src_data = {}
    for filename in str_files:
        filepath = os.path.join(str_dir, filename)
        key = os.path.splitext(filename)[0]  # Remove .str extension

        with open(filepath, 'rb') as f:
            raw = f.read()

        # Split by 0x0A and remove trailing empty string if file ends with 0x0A
        lines = raw.split(b'\x0a')
        if lines and lines[-1] == b'':
            lines.pop()

        # Decode as ASCII (all files are pure ASCII)
        src_data[key] = [line.decode('ascii') for line in lines]
    
    patch_data = {}
    for filename in str_files:
        filepath = os.path.join(patch_dir, filename)
        key = os.path.splitext(filename)[0]  # Remove .str extension

        with open(filepath, 'rb') as f:
            raw = f.read()

        # Split by 0x0A and remove trailing empty string if file ends with 0x0A
        lines = raw.split(b'\x0a')
        if lines and lines[-1] == b'':
            lines.pop()

        # Decode as ASCII (all files are pure ASCII)
        patch_data[key] = [line.decode('euc-kr') for line in lines]

    glossary = {}
    for file_key, lines in src_data.items():
        if not isinstance(lines, list):
            print(f'Error: Value for "{file_key}" must be a list of strings', file=sys.stderr)
            sys.exit(1)

        patch_lines = patch_data.get(file_key, [])
        if len(lines) != len(patch_lines):
            print(f'Error: Mismatch in number of lines for "{file_key}" between source and patch', file=sys.stderr)
            sys.exit(1)
            
        category = _get_category(file_key)
        for line, patch_line in zip(lines, patch_lines):
            if not isinstance(line, str):
                print(f'Error: Value in "{file_key}" must be a string', file=sys.stderr)
                sys.exit(1)
            if line not in glossary:
                glossary[line] = {
                    'korean': patch_line,
                    'category': category,
                    'description': ''
                }

    # Sort: category first (alphabetical), then word-by-word dictionary order
    # with prefix match -> more words first
    sorted_entries = sorted(
        [(name, glossary[name]['category']) for name in glossary.keys()],
        key=functools.cmp_to_key(_compare_glossary_entries)
    )

    # Rebuild ordered glossary
    ordered_glossary = {name: glossary[name] for name, _ in sorted_entries}

    with open(glossary_file, 'w', encoding='utf-8') as f:
        json.dump(ordered_glossary, f, ensure_ascii=False, indent=2)

    print(f'Rebuilt glossary with {len(ordered_glossary)} entries from source {str_dir} and patch {patch_dir} to {glossary_file}')

def main():
    parser = argparse.ArgumentParser(
        description='Convert .str files (0x0A-separated) to/from JSON and glossary formats'
    )
    subparsers = parser.add_subparsers(dest='command', required=True)

    # to-json subcommand
    to_json = subparsers.add_parser('to-json', help='Convert .str files to JSON')
    to_json.add_argument('--str-dir', default=DEFAULT_STR_DIR,
                         help=f'Directory containing .str files (default: {DEFAULT_STR_DIR})')
    to_json.add_argument('--json-file', default=DEFAULT_JSON_FILE,
                         help=f'Output JSON file (default: {DEFAULT_JSON_FILE})')

    # from-json subcommand
    from_json = subparsers.add_parser('from-json', help='Restore .str files from JSON')
    from_json.add_argument('--str-dir', default=DEFAULT_STR_DIR,
                           help=f'Directory to write .str files (default: {DEFAULT_STR_DIR})')
    from_json.add_argument('--json-file', default=DEFAULT_JSON_FILE,
                           help=f'Input JSON file (default: {DEFAULT_JSON_FILE})')

    # to-glossary subcommand
    to_glossary = subparsers.add_parser('to-glossary', help='Convert JSON to glossary format')
    to_glossary.add_argument('--json-file', default=DEFAULT_JSON_FILE,
                             help=f'Input JSON file (default: {DEFAULT_JSON_FILE})')
    to_glossary.add_argument('--glossary-file', default=DEFAULT_GLOSSARY_FILE,
                             help=f'Output glossary file (default: {DEFAULT_GLOSSARY_FILE})')

    # from-glossary subcommand
    from_glossary = subparsers.add_parser('from-glossary', help='Convert glossary format back to JSON')
    from_glossary.add_argument('--json-file', default=DEFAULT_JSON_FILE,
                               help=f'Output JSON file (default: {DEFAULT_JSON_FILE})')
    from_glossary.add_argument('--glossary-file', default=DEFAULT_GLOSSARY_FILE,
                               help=f'Input glossary file (default: {DEFAULT_GLOSSARY_FILE})')

    # rebuild-glossary subcommand
    rebuild_glossary = subparsers.add_parser('rebuild-glossary', help='Rebuild glossary format back to JSON')
    rebuild_glossary.add_argument('--str-dir', default=DEFAULT_STR_DIR,
                                  help=f'Directory containing .str files (default: {DEFAULT_STR_DIR})')
    rebuild_glossary.add_argument('--patch-dir', default=DEFAULT_PATCH_DIR,
                                  help=f'Directory containing .str files (default: {DEFAULT_PATCH_DIR})')
    rebuild_glossary.add_argument('--glossary-file', default=DEFAULT_GLOSSARY_FILE,
                                  help=f'Input glossary file (default: {DEFAULT_GLOSSARY_FILE})')

    args = parser.parse_args()

    if args.command == 'to-json':
        str_to_json(args.str_dir, args.json_file)
    elif args.command == 'from-json':
        json_to_str(args.str_dir, args.json_file)
    elif args.command == 'to-glossary':
        json_to_glossary(args.json_file, args.glossary_file)
    elif args.command == 'from-glossary':
        glossary_to_json(args.json_file, args.glossary_file)
    elif args.command == 'rebuild-glossary':
        rebuild_glossary_to_json(args.str_dir, args.patch_dir, args.glossary_file)


if __name__ == '__main__':
    main()