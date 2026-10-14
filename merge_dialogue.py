import sys
import os
import struct
import json
import base64
from dataclasses import dataclass, field
from typing import List

# Reuse functions from read_all_idx.py and rebuild_all_db.py
from read_all_idx import (
    read_all_idx,
    read_all_db,
    build_sections,
    Section,
    KeyEntry,
)
from rebuild_all_db import (
    serialize_section,
    rebuild_all_db,
    write_all_idx,
    write_por_files,
    write_address_verification,
)


def load_combined_from_json(json_path):
    """
    Loads combined Section data from a JSON file.

    Decoding:
    - key: latin-1 decoded to bytes
    - direction: base64 decoded to bytes
    - selections, dialogue: euc-kr decoded to str
    """
    with open(json_path, 'rb') as f:
        raw_data = f.read()

    # Read as latin-1 to preserve all bytes, then parse JSON
    data = json.loads(raw_data.decode('latin-1'))

    combined = []
    for sec_data in data:
        # Decode selections from euc-kr
        selections = []
        for sel in sec_data['selections']:
            try:
                selections.append(sel.encode('latin-1').decode('euc-kr'))
            except (UnicodeDecodeError, UnicodeEncodeError):
                selections.append(sel)

        section = Section(
            name=sec_data['name'],
            address=sec_data['address'],
            direction=base64.b64decode(sec_data['direction']),
            selections=selections,
        )
        for key_data in sec_data['keys']:
            # Decode dialogue from euc-kr
            dialogue = key_data['dialogue']
            try:
                dialogue = dialogue.encode('latin-1').decode('euc-kr')
            except (UnicodeDecodeError, UnicodeEncodeError):
                pass

            section.keys.append(KeyEntry(
                key=key_data['key'].encode('latin-1'),
                dialogue=dialogue,
                speaker=key_data['speaker'],
                desc=key_data['desc'],
            ))
        combined.append(section)

    print(f"Combined data loaded from JSON: {json_path}")
    print(f"Total sections: {len(combined)}")

    return combined


def is_empty(value):
    """
    Checks if a value is considered 'empty' for merge purposes.
    Treats None, empty string, and the string 'null' (case-insensitive) as empty.
    """
    if value is None:
        return True
    if isinstance(value, str):
        if value == '':
            return True
        if value.lower() == 'null':
            return True
    return False


def merge_dialogue_data(base_combined, merge_combined):
    """
    Merges dialogue data from merge_combined into base_combined.

    For each section:
    - Compares 'selections' between base and merge
    - Compares 'dialogue' for each key between base and merge
    - If different, uses the merge data
    - If merge data is empty (None, '', or 'null'), keeps the base data

    Returns a new combined list with merged data.
    """
    # Build lookup dict for merge data by section name
    merge_by_name = {}
    for section in merge_combined:
        merge_by_name[section.name] = section

    merged = []
    merge_count = 0
    selection_merge_count = 0
    dialogue_merge_count = 0

    for base_section in base_combined:
        # Create a copy of the base section
        new_section = Section(
            name=base_section.name,
            address=base_section.address,
            direction=base_section.direction,
            selections=list(base_section.selections),
        )

        # Find matching merge section
        merge_section = merge_by_name.get(base_section.name)

        if merge_section is not None:
            # Compare selections
            # For each selection, if merge selection is empty, keep base selection
            # Otherwise, if different, use merge selection
            if merge_section.selections != base_section.selections:
                merged_selections = []
                selection_changed = False
                for i, base_sel in enumerate(base_section.selections):
                    if i < len(merge_section.selections):
                        merge_sel = merge_section.selections[i]
                        if is_empty(merge_sel):
                            # B selection is empty, keep A's selection
                            merged_selections.append(base_sel)
                        elif merge_sel != base_sel:
                            # B selection is non-empty and different, use B
                            merged_selections.append(merge_sel)
                            selection_changed = True
                        else:
                            # Same, keep A
                            merged_selections.append(base_sel)
                    else:
                        # B has fewer selections, keep A's remaining selections
                        merged_selections.append(base_sel)

                # If B has more selections than A, add the extra ones
                if len(merge_section.selections) > len(base_section.selections):
                    for i in range(len(base_section.selections), len(merge_section.selections)):
                        merge_sel = merge_section.selections[i]
                        if not is_empty(merge_sel):
                            merged_selections.append(merge_sel)
                            selection_changed = True

                if merged_selections != base_section.selections:
                    new_section.selections = merged_selections
                    selection_merge_count += 1
                    print(f"  [SELECTIONS] Section '{base_section.name}': "
                          f"{len(base_section.selections)} -> {len(merged_selections)} items")

            # Build lookup for merge keys by key bytes
            merge_keys = {}
            for key_entry in merge_section.keys:
                merge_keys[key_entry.key] = key_entry

            # Compare dialogue for each key
            for key_entry in base_section.keys:
                merge_key_entry = merge_keys.get(key_entry.key)
                if merge_key_entry is not None:
                    # Check if merge dialogue is empty - if so, keep base data
                    if is_empty(merge_key_entry.dialogue):
                        # B dialogue is empty, keep A's data
                        new_section.keys.append(KeyEntry(
                            key=key_entry.key,
                            dialogue=key_entry.dialogue,
                            speaker=key_entry.speaker,
                            desc=key_entry.desc,
                        ))
                    elif merge_key_entry.dialogue != key_entry.dialogue:
                        # Use merge data for dialogue
                        # For speaker and desc, use merge data if non-empty, otherwise keep base
                        speaker = merge_key_entry.speaker
                        if is_empty(speaker):
                            speaker = key_entry.speaker
                        desc = merge_key_entry.desc
                        if is_empty(desc):
                            desc = key_entry.desc

                        new_section.keys.append(KeyEntry(
                            key=key_entry.key,
                            dialogue=merge_key_entry.dialogue,
                            speaker=speaker,
                            desc=desc,
                        ))
                        dialogue_merge_count += 1
                        print(f"  [DIALOGUE] Section '{base_section.name}' key '{key_entry.key.decode('latin-1')}': "
                              f"'{key_entry.dialogue[:50]}...' -> '{merge_key_entry.dialogue[:50]}...'")
                    else:
                        # Same dialogue, keep base data
                        new_section.keys.append(KeyEntry(
                            key=key_entry.key,
                            dialogue=key_entry.dialogue,
                            speaker=key_entry.speaker,
                            desc=key_entry.desc,
                        ))
                else:
                    # Key not in merge, keep base data
                    new_section.keys.append(KeyEntry(
                        key=key_entry.key,
                        dialogue=key_entry.dialogue,
                        speaker=key_entry.speaker,
                        desc=key_entry.desc,
                    ))
        else:
            # No merge section, keep all base data
            for key_entry in base_section.keys:
                new_section.keys.append(KeyEntry(
                    key=key_entry.key,
                    dialogue=key_entry.dialogue,
                    speaker=key_entry.speaker,
                    desc=key_entry.desc,
                ))

        merged.append(new_section)
        merge_count += 1

    print()
    print(f"=== Merge Summary ===")
    print(f"Total sections processed: {merge_count}")
    print(f"Sections with selection changes: {selection_merge_count}")
    print(f"Keys with dialogue changes: {dialogue_merge_count}")

    return merged


def save_combined_to_json(combined, output_json_path):
    """
    Saves the combined Section data to a JSON file.

    Encoding:
    - key: latin-1 encoded string
    - direction: base64 encoded string
    - selections, dialogue: euc-kr encoded (stored as latin-1 in file)
    - speaker, desc: plain strings
    """
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
        for key_entry in section.keys:
            # Encode dialogue to euc-kr bytes, then decode as latin-1 to preserve bytes
            dialogue = key_entry.dialogue
            try:
                dialogue = dialogue.encode('euc-kr').decode('latin-1')
            except (UnicodeEncodeError, UnicodeDecodeError):
                pass

            sec_data['keys'].append({
                'key': key_entry.key.decode('latin-1'),
                'dialogue': dialogue,
                'speaker': key_entry.speaker,
                'desc': key_entry.desc,
            })
        data.append(sec_data)

    with open(output_json_path, 'w', encoding='latin-1') as f:
        json.dump(data, f, ensure_ascii=False, indent=2)

    print(f"Combined data saved to JSON: {output_json_path}")
    print(f" Total sections: {len(data)}")


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

    ## Step 4: Write address verification list
    #print("=== Writing address verification list ===")
    #verification_path = os.path.join(output_dir, 'address_verification.txt')
    #write_address_verification(sections_data, verification_path)
    #print()

    return True


if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("Usage: python merge_dialogue.py <game_dir> <to_merge.json>")
        print("  <game_dir>     - Path to the game directory (e.g., '../Pool of Radiance')")
        print("  <to_merge.json> - JSON file containing merged dialogue data")
        sys.exit(1)

    game_dir = sys.argv[1]
    merge_json_path = sys.argv[2]

    # Paths
    dialogue_dir = os.path.join(game_dir, 'data', 'Dialogue')
    output_dir = os.path.join(game_dir, 'data', 'Dialogue_kr')

    idx_path = os.path.join(dialogue_dir, 'all.idx')
    db_path = os.path.join(dialogue_dir, 'all.db')

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
    base_combined = build_sections(sections, dialogue_dir)
    print(f"Combined {len(base_combined)} sections from original data")
    print()

    # Step 4: Load to_merge.json
    print("=== Step 4: Loading to_merge.json ===")
    merge_combined = load_combined_from_json(merge_json_path)
    print()

    # Step 5: Merge dialogue data
    print("=== Step 5: Merging dialogue data ===")
    merged_combined = merge_dialogue_data(base_combined, merge_combined)
    print()

    ## Step 6: Save merged data to JSON (for reference)
    #print("=== Step 6: Saving merged data to JSON ===")
    #merged_json_path = os.path.join(output_dir, 'merged.json')
    #os.makedirs(output_dir, exist_ok=True)
    #save_combined_to_json(merged_combined, merged_json_path)
    #print()

    # Step 7: Restore files to Dialogue_kr
    print("=== Step 7: Restoring files to Dialogue_kr ===")
    success = restore_from_combined(merged_combined, output_dir)
    if not success:
        print("Failed to restore files")
        sys.exit(1)
    print()

    print("=== Done ===")
    print(f"Output directory: {output_dir}")
    print(f"  - {os.path.join(output_dir, 'all.db')}")
    print(f"  - {os.path.join(output_dir, 'all.idx')}")
    #print(f"  - {os.path.join(output_dir, 'address_verification.txt')}")
    #print(f"  - {os.path.join(output_dir, 'merged.json')}")
    print(f"  - .por files ({len(merged_combined)} files)")