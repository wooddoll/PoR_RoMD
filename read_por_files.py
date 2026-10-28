import sys
import os
import struct
import json
import base64
from datetime import datetime
from dataclasses import dataclass, field
from typing import List

from read_all_idx import (
    read_all_por_file,
)

def read_por_files(dialogue_dir, json_path):
    entries = read_all_por_file(dialogue_dir)
    if entries is not None:
        output_dir = os.path.dirname(json_path)
        os.makedirs(output_dir, exist_ok=True)

        with open(json_path, 'w', encoding='utf-8') as f:
            json.dump(entries, f, ensure_ascii=False, indent=2)
            print(f"Combined data saved to JSON: {json_path}")
            print(f"Total sections: {len(entries)}")


def write_por_file(section, por_path):
    """
    Writes .por files for each section to the specified output directory.

    .por file format (repeated):
    [dialogue key][tab][dialogue text][tab][speaker key][tab][date/scene description][tab][0x0D, 0x0A]
    The last field (date/scene description) may be absent.
    """
    if not section:
        print("No section to write.")
        return

    output_dir = os.path.dirname(por_path)
    os.makedirs(output_dir, exist_ok=True)

    lines = []
    for key_name, key_entry in section.items():
        parts = [key_name.encode('latin-1', errors='replace'), 
                 key_entry['dialogue'].encode('euc-kr', errors='replace'),
                 key_entry['speaker'].encode('ascii', errors='replace'),
                 key_entry['desc'].encode('ascii', errors='replace')]
        line = b'\t'.join(parts)
        lines.append(line)
    content = b'\r\n'.join(lines)
    if content:
        content += b'\r\n'
    with open(por_path, 'wb') as f:
        f.write(content)

    #print(f"Total {len(section)} keys written to {por_path} file")

def write_por_files(json_path, dialogue_dir):
    with open(json_path, 'rb') as f:
        raw_data = f.read()
        # Read as latin-1 to preserve all bytes, then parse JSON
        entries = json.loads(raw_data.decode('utf-8'))

        if entries is not None:
            for key_name, key_data in entries.items():
                por_name = key_name + ".por"
                por_path = os.path.join(dialogue_dir, por_name)

                write_por_file(key_data, por_path)

        print(f"Total {len(entries)} keys written to {dialogue_dir}")


def convert_test(json_path, export_json):
    new_entries = {}
    with open(json_path, 'rb') as f:
        raw_data = f.read()
        # Read as latin-1 to preserve all bytes, then parse JSON
        sections = json.loads(raw_data.decode('utf-8'))
        if sections is not None:
            for sec_name, sec_data in sections.items():
                new_entries[sec_name] = {}
                for key_name, key_data in sec_data['keys'].items():
                    new_entries[sec_name][key_name] = key_data

    with open(export_json, 'w', encoding='utf-8') as f:
        json.dump(new_entries, f, ensure_ascii=False, indent=2)

    print(f"Converted data saved to JSON: {export_json}")
    print(f"Total sections: {len(new_entries)}")

def rebuild_json(en_dialoure, kr_dialoure, export_json):
    en_entries = read_all_por_file(en_dialoure)
    kr_entries = read_all_por_file(kr_dialoure)

    if en_entries is not None and kr_entries is not None:
        output_dir = os.path.dirname(export_json)
        os.makedirs(output_dir, exist_ok=True)

        entries = {}
        for sec_name, sec_data in en_entries.items():
            section = {}
            for key_name, key_data in sec_data.items():
                # Add Korean dialogue if available
                if sec_name in kr_entries and key_name in kr_entries[sec_name]:
                    dialogue = kr_entries[sec_name][key_name]['dialogue']
                else:
                    dialogue = key_data['dialogue']  # Fallback to English if Korean not available

                dialogue_en = key_data['dialogue']  # Store English dialogue separately
                if not dialogue_en or dialogue_en == "null":
                    continue

                section[key_name] = {
                    'dialogue_en': dialogue_en,
                    'dialogue': dialogue,
                    'speaker': key_data['speaker'],
                    'desc': key_data['desc'],
                }
            if section:  # Only add non-empty sections
                entries[sec_name] = section
                

        with open(export_json, 'w', encoding='utf-8') as f:
            json.dump(entries, f, ensure_ascii=False, indent=2)
            print(f"Combined data saved to JSON: {export_json}")
            print(f"Total sections: {len(entries)}")



if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("Usage: python read_all_idx.py por-json <dialogue_dir> <to_export.json>")
        print("       python read_all_idx.py json-por <to_export.json> <dialogue_dir>")
        print("       python read_all_idx.py combined <eng_dialogue_dir> <kor_dialogue_dir> <to_export.json>")
        print("  <dialogue_dir>     - Path to the game directory (e.g., '../Pool of Radiance/Data/Dialogue')")
        print("  <to_export.json> - JSON file containing combined dialogue data")
        sys.exit(1)

    if sys.argv[1] == 'por-json':
        dialogue_dir = sys.argv[2]
        json_path = sys.argv[3]
        read_por_files(dialogue_dir, json_path)
    elif sys.argv[1] == 'json-por':
        json_path = sys.argv[2]
        dialogue_dir = sys.argv[3]
        write_por_files(json_path, dialogue_dir)
    elif sys.argv[1] == 'combined':
        eng_dialogue_dir = sys.argv[2]
        kor_dialogue_dir = sys.argv[3]
        json_path = sys.argv[4]

        rebuild_json(eng_dialogue_dir, kor_dialogue_dir, json_path)

