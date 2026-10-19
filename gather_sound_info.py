import os
import json
import sys


def list_wav_files(voice_dir):
    """
    Lists all .wav files in the given directory.
    Returns a list of filenames (without extension).
    """
    wav_files = []
    if not os.path.isdir(voice_dir):
        print(f"Error: Directory '{voice_dir}' not found.")
        return wav_files

    for filename in os.listdir(voice_dir):
        if filename.lower().endswith('.wav'):
            # Remove the .wav extension
            name_without_ext = os.path.splitext(filename)[0]
            wav_files.append(name_without_ext)

    return wav_files


def load_json(json_path):
    """
    Loads a JSON file.
    Returns the parsed JSON data.
    """
    try:
        with open(json_path, 'r', encoding='utf-8') as f:
            return json.load(f)
    except FileNotFoundError:
        print(f"Error: File '{json_path}' not found.")
        return None
    except json.JSONDecodeError as e:
        print(f"Error: Invalid JSON in '{json_path}': {e}")
        return None


def find_matching_entries(original_data, wav_names):
    """
    Finds entries in original.json whose keys match the wav filenames.
    Key matching is case-insensitive (both sides converted to lowercase).
    Returns a dict organized by speaker, with each speaker containing
    a dict of original key names mapped to their entry data.
    """
    # Build a case-insensitive lookup of all keys in original.json
    # Maps lowercase key -> (section_name, actual_key_name)
    key_lookup = {}
    for section_name, section_data in original_data.items():
        keys = section_data.get('keys', {})
        for key_name in keys:
            key_lookup[key_name.lower()] = (section_name, key_name)

    result = {}
    unmatched_wavs = []

    for wav_name in wav_names:
        wav_name_lower = wav_name.lower()
        if wav_name_lower in key_lookup:
            section_name, actual_key_name = key_lookup[wav_name_lower]
            key_data = original_data[section_name]['keys'][actual_key_name]
            speaker = key_data.get('speaker', '')
            if speaker not in result:
                result[speaker] = {}

            # Add the file path to the entry (exclude the speaker field)
            entry = {k: v for k, v in key_data.items() if k != 'speaker'}
            entry['file_path'] = os.path.join(
                '/home1/moonhyun/Workspace/Pool of Radiance/data/Sound/voice',
                wav_name + '.wav'
            )
            result[speaker][wav_name] = entry
        else:
            unmatched_wavs.append(wav_name)

    # Sort by speaker name
    result = dict(sorted(result.items()))
    return result, unmatched_wavs


def add_korean_dialogue(result, original_data, to_merge_data):
    """
    Adds 'dialogue_kr' field to each entry by looking up the dialogue
    from to_merge_fixed.json at the same location as in original.json.
    Key matching is case-insensitive (both sides converted to lowercase).
    Returns a list of keys that exist in to_merge_fixed.json but not in original.json.
    """
    # Build a case-insensitive lookup of all keys in original.json
    original_keys_lower = set()
    for section_name, section_data in original_data.items():
        for key_name in section_data.get('keys', {}).keys():
            original_keys_lower.add(key_name.lower())

    # Build a case-insensitive lookup of all keys in to_merge_fixed.json
    # Maps lowercase key -> (section_name, actual_key_name)
    to_merge_lookup = {}
    for section_name, section_data in to_merge_data.items():
        keys = section_data.get('keys', {})
        for key_name in keys:
            to_merge_lookup[key_name.lower()] = (section_name, key_name)

    # Find keys in to_merge_fixed.json that are not in original.json (case-insensitive)
    unmatched_merge_keys = sorted(
        key for key in to_merge_lookup.keys() if key not in original_keys_lower
    )

    # Add dialogue_kr to each entry in result
    for speaker, entries in result.items():
        for key_name, entry in entries.items():
            # Find the dialogue in to_merge_fixed.json at the same location (case-insensitive)
            key_lower = key_name.lower()
            if key_lower in to_merge_lookup:
                section_name, actual_key_name = to_merge_lookup[key_lower]
                entry['dialogue_kr'] = to_merge_data[section_name]['keys'][actual_key_name].get('dialogue', '')

    return unmatched_merge_keys


def save_result(result, output_path):
    """
    Saves the matching entries to a JSON file.
    """
    with open(output_path, 'w', encoding='utf-8') as f:
        json.dump(result, f, ensure_ascii=False, indent=2)
    total_entries = sum(len(v) for v in result.values())
    print(f"Saved {total_entries} matching entries (grouped by {len(result)} speakers) to: {output_path}")


def save_unmatched_wavs(unmatched_wavs, output_path, voice_dir):
    """
    Saves the sorted list of unmatched .wav files to a text file with full paths.
    """
    with open(output_path, 'w', encoding='utf-8') as f:
        for wav_name in sorted(unmatched_wavs):
            f.write(os.path.join(voice_dir, wav_name + '.wav') + '\n')
    print(f"Saved {len(unmatched_wavs)} unmatched .wav file paths to: {output_path}")


def save_unmatched_merge_keys(unmatched_keys, output_path):
    """
    Saves the list of keys in to_merge_fixed.json that are not in original.json.
    """
    with open(output_path, 'w', encoding='utf-8') as f:
        json.dump(unmatched_keys, f, ensure_ascii=False, indent=2)
    print(f"Saved {len(unmatched_keys)} unmatched keys from to_merge_fixed.json to: {output_path}")


def main():
    voice_dir = '/home1/moonhyun/Workspace/Pool of Radiance/data/Sound/voice'
    original_json_path = 'translation/original.json'
    to_merge_json_path = 'translation/to_merge_fixed.json'
    output_json_path = 'translation/sound_matching.json'
    unmatched_wavs_path = 'translation/unmatched_wavs.txt'
    unmatched_merge_keys_path = 'translation/unmatched_merge_keys.json'

    # 1. List all .wav files
    wav_names = list_wav_files(voice_dir)
    print(f"Found {len(wav_names)} .wav files in '{voice_dir}'")

    # 2. Load original.json
    original_data = load_json(original_json_path)
    if original_data is None:
        sys.exit(1)

    # 3. Load to_merge_fixed.json
    to_merge_data = load_json(to_merge_json_path)
    if to_merge_data is None:
        sys.exit(1)

    # 4. Find matching entries and unmatched wav files
    matching_entries, unmatched_wavs = find_matching_entries(original_data, wav_names)
    total_entries = sum(len(v) for v in matching_entries.values())
    print(f"Found {total_entries} matching entries in original.json (grouped by {len(matching_entries)} speakers)")
    print(f"Found {len(unmatched_wavs)} .wav files without matching keys in original.json")

    # 5. Add Korean dialogue from to_merge_fixed.json
    unmatched_merge_keys = add_korean_dialogue(matching_entries, original_data, to_merge_data)
    print(f"Found {len(unmatched_merge_keys)} keys in to_merge_fixed.json not in original.json")

    # 6. Save results
    save_result(matching_entries, output_json_path)
    save_unmatched_wavs(unmatched_wavs, unmatched_wavs_path, voice_dir)
    save_unmatched_merge_keys(unmatched_merge_keys, unmatched_merge_keys_path)


if __name__ == '__main__':
    main()