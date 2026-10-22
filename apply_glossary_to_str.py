#!/usr/bin/env python3
"""
Apply glossary_merged.json translations to str_data.json.

For each string in str_data.json, if the string exists as a key in
glossary_merged.json and has a non-empty 'korean' value, replace the
string with the Korean translation.

Usage:
    python apply_glossary_to_str.py [--str-json FILE] [--glossary FILE] [--output FILE]

Options:
    --str-json FILE   Input str_data.json file (default: str_data.json)
    --glossary FILE   Input glossary_merged.json file (default: glossary_merged.json)
    --output FILE     Output JSON file (default: str_data_korean.json)
"""

import argparse
import json
import os
import sys

DEFAULT_STR_JSON = 'str_data.json'
DEFAULT_GLOSSARY = 'glossary_merged.json'
DEFAULT_OUTPUT = 'str_data_korean.json'


def load_json(filepath: str) -> dict:
    """Load a JSON file and return its contents as a dict."""
    if not os.path.isfile(filepath):
        print(f'Error: File not found: {filepath}', file=sys.stderr)
        sys.exit(1)

    with open(filepath, 'r', encoding='utf-8') as f:
        data = json.load(f)

    if not isinstance(data, dict):
        print(f'Error: {filepath} must be a JSON object', file=sys.stderr)
        sys.exit(1)

    return data


def apply_glossary(str_json_file: str, glossary_file: str, output_file: str) -> None:
    """Replace strings in str_data.json with Korean translations from glossary."""
    str_data = load_json(str_json_file)
    glossary = load_json(glossary_file)

    # Build case-insensitive lookup for glossary names
    glossary_lookup = {}
    for name, info in glossary.items():
        if isinstance(info, dict):
            glossary_lookup[name.lower()] = info

    replaced_count = 0
    total_count = 0

    new_data = {}
    for file_key, lines in str_data.items():
        if not isinstance(lines, list):
            print(f'Error: Value for "{file_key}" must be a list of strings', file=sys.stderr)
            sys.exit(1)

        new_lines = []
        for line in lines:
            if not isinstance(line, str):
                print(f'Error: Value in "{file_key}" must be a string', file=sys.stderr)
                sys.exit(1)

            total_count += 1
            line_lower = line.lower()

            # Check if this line exists in the glossary
            if line_lower in glossary_lookup:
                info = glossary_lookup[line_lower]
                korean = info.get('korean', '')
                if korean:
                    new_lines.append(korean)
                    replaced_count += 1
                    continue

            # No match or empty korean: keep original
            new_lines.append(line)

        new_data[file_key] = new_lines

    with open(output_file, 'w', encoding='utf-8') as f:
        json.dump(new_data, f, ensure_ascii=False, indent=2)

    print(f'Processed {total_count} strings, replaced {replaced_count} with Korean translations')
    print(f'Output written to: {output_file}')


def main():
    parser = argparse.ArgumentParser(
        description='Apply glossary_merged.json translations to str_data.json'
    )
    parser.add_argument('--str-json', default=DEFAULT_STR_JSON,
                        help=f'Input str_data.json file (default: {DEFAULT_STR_JSON})')
    parser.add_argument('--glossary', default=DEFAULT_GLOSSARY,
                        help=f'Input glossary file (default: {DEFAULT_GLOSSARY})')
    parser.add_argument('--output', default=DEFAULT_OUTPUT,
                        help=f'Output JSON file (default: {DEFAULT_OUTPUT})')

    args = parser.parse_args()

    apply_glossary(args.str_json, args.glossary, args.output)


if __name__ == '__main__':
    main()