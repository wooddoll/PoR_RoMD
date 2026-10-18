import sys
import os
import struct
import json
import base64


def load_text_from_json(json_path):
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

    combined = {}
    for sec_name, sec_data in data.items():
        # Decode selections from euc-kr
        if sec_data.get('selections'):
            selections = []
            for sel in sec_data['selections']:
                try:
                    selections.append(sel.encode('latin-1').decode('euc-kr'))
                except (UnicodeDecodeError, UnicodeEncodeError):
                    selections.append(sel)

            section = {
                'selections': selections,
            }
        else:
            section = {}
            
        if sec_data.get('keys'):
            section['keys'] = {}
            for key_name, key_data in sec_data['keys'].items():
                # Decode dialogue from euc-kr
                dialogue = key_data['dialogue']
                try:
                    dialogue = dialogue.encode('latin-1').decode('euc-kr')
                except (UnicodeDecodeError, UnicodeEncodeError):
                    pass

                section['keys'][key_name] = {
                    'dialogue': dialogue,
                    'speaker': key_data['speaker'],
                }
                if key_data.get('desc'):
                    section['keys'][key_name]['desc'] = key_data['desc']
                    
        combined[sec_name] = section

    print(f"Combined data loaded from JSON: {json_path}")
    print(f"Total sections: {len(combined)}")

    return combined

def analyze_json(sections):
    line_count = 0
    speakers = set()
    for sec_name, sec_data in sections.items():
        if sec_data.get('selections'):
            line_count += len(sec_data['selections'])
        if sec_data.get('keys'):
            line_count += len(sec_data['keys'])
            for key_name, key_data in sec_data['keys'].items():
                speakers.add(key_data['speaker'])
    speakers_sorted = sorted(speakers)
    
    print(f"Total lines is {line_count}")
    print(f"Total speakers is {len(speakers_sorted)}")
    #print(speakers_sorted)
    
    speakers_count = {}
    for name in speakers_sorted:
        speakers_count[name] = 0
        
    for sec_name, sec_data in sections.items():
        if sec_data.get('keys'):
            for key_name, key_data in sec_data['keys'].items():
                speaker_name = key_data['speaker']
                speakers_count[speaker_name] += 1
    
    print(speakers_count)
    
    
    
if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python analuze_text.py <to_test.json>")
        sys.exit(1)

    json_path = sys.argv[1]
    combined = load_text_from_json(json_path)
    print()
    analyze_json(combined)
    