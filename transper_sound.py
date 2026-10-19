import os
import json
import sys
import ssl

# 1. Disable SSL verification (equivalent to wget's --no-check-certificate)
# 1. Hugging Face �� ǥ�� ���̽� SSL ���� ���� ����ȭ ����
os.environ["CURL_CA_BUNDLE"] = ""
os.environ["HTTP_PROXY"] = ""
os.environ["HTTPS_PROXY"] = ""
os.environ["HF_HUB_DISABLE_SYMLINKS_WARNING"] = "1"
os.environ["PYTHONHTTPSVERIFY"] = "0"

# Hugging Face ��� ��ü�� SSL ���� �н� ����
os.environ["HF_HUB_DISABLE_SSL_VERIFY"] = "1" 

# urllib/requests ���� ���� ��Ȱ��ȭ
ssl._create_default_https_context = ssl._create_unverified_context

try:
    import requests
    from requests.adapters import HTTPAdapter
    
    # 기본 verify 설정을 False로 강제하는 커스텀 세션 적용
    original_session_request = requests.Session.request
    def unverified_request(self, method, url, *args, **kwargs):
        kwargs['verify'] = False
        return original_session_request(self, method, url, *args, **kwargs)
    requests.Session.request = unverified_request
    
    # requests의 인secure 경고창 숨기기
    import urllib3
    urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)
except ImportError:
    pass

import torch

# 1. 원래의 torch.load 함수를 백업합니다.
_original_torch_load = torch.load

# 2. weights_only를 무조건 False로 강제하는 래퍼 함수를 정의합니다.
def _patched_torch_load(*args, **kwargs):
    if "weights_only" in kwargs:
        kwargs["weights_only"] = False
    else:
        kwargs["weights_only"] = False
    return _original_torch_load(*args, **kwargs)

# 3. 오리지널 함수를 패치된 함수로 교체합니다.
torch.load = _patched_torch_load

torch.serialization.default_load_weights_only = False
#torch.serialization.add_safe_globals(["TTS.tts.configs.xtts_config.XttsConfig"])

from TTS.api import TTS
from TTS.tts.configs.xtts_config import XttsConfig
from TTS.tts.models.xtts import Xtts

# 1. GPU device selection
#device = "cuda" if torch.cuda.is_available() else "cpu"
device = 'cpu'
print(f"Using device: {device}")

# 2. Load XTTS v2 model
#checkpoint_dir = "/home/wooddoll/.local/share/tts/tts_models--multilingual--multi-dataset--xtts_v2"
#config_path = os.path.join(checkpoint_dir, "config.json")

print("Loading local XTTS v2 config...")
#config = XttsConfig()
#config.load_json(config_path)
#print("Initializing model from local checkpoint...")
#model = Xtts.init_from_config(config)
#model.load_checkpoint(config, checkpoint_dir=checkpoint_dir, eval=True)
#model.to(device)
tts = TTS("tts_models/multilingual/multi-dataset/xtts_v2").to(device)

# 3. Load sound matching data
def load_json(json_path):
    """Loads a JSON file and returns parsed data."""
    try:
        with open(json_path, 'r', encoding='utf-8') as f:
            return json.load(f)
    except FileNotFoundError:
        print(f"Error: File '{json_path}' not found.")
        return None
    except json.JSONDecodeError as e:
        print(f"Error: Invalid JSON in '{json_path}': {e}")
        return None

def main():
    sound_matching_path = 'translation/sound_matching.json'
    output_dir = 'sound'
    os.makedirs(output_dir, exist_ok=True)

    # Load the sound matching data
    sound_data = load_json(sound_matching_path)
    if sound_data is None:
        sys.exit(1)

    # Iterate over each speaker
    total_entries = sum(len(entries) for entries in sound_data.values())
    print(f"\nLoaded {total_entries} entries from {sound_matching_path}")
    print(f"Total speakers: {len(sound_data)}")

    processed = 0
    failed = 0

    for speaker, entries in sorted(sound_data.items()):
        # Collect reference audios for this speaker (all wav files of this speaker)
        reference_audios = []
        for key_name, entry in entries.items():
            file_path = entry.get('file_path', '')
            if file_path and os.path.exists(file_path):
                reference_audios.append(file_path)

        if not reference_audios:
            print(f"[SKIP] Speaker '{speaker}': no valid reference audio files found")
            continue

        print(f"\n--- Processing speaker: {speaker} ({len(entries)} entries) ---")
        print(f"    Reference audios: {len(reference_audios)}")

        # Process each entry for this speaker
        for key_name, entry in entries.items():
            processed += 1
            dialogue_kr = entry.get('dialogue_kr', '')
            file_path = entry.get('file_path', '')

            if not dialogue_kr:
                print(f"  [{processed}/{total_entries}] {key_name}: SKIP (no Korean dialogue)")
                failed += 1
                continue

            if not file_path:
                print(f"  [{processed}/{total_entries}] {key_name}: SKIP (no file path)")
                failed += 1
                continue

            # Output filename: same as original wav file name
            original_name = os.path.splitext(os.path.basename(file_path))[0]
            output_filename = os.path.join(output_dir, f"{original_name}.wav")

            print(f"  [{processed}/{total_entries}] {key_name}: {dialogue_kr[:30]}...")

            try:
                tts.tts_to_file(
                    text=dialogue_kr,
                    speaker_wav=reference_audios,
                    language="ko",
                    file_path=output_filename
                )
            except Exception as e:
                print(f"    ERROR: {e}")
                failed += 1

    print(f"\n=== Done === ")
    print(f"Total entries: {total_entries}")
    print(f"Processed: {processed - failed}")
    print(f"Failed/Skipped: {failed}")
    print(f"Output directory: {output_dir}/")

if __name__ == '__main__':
    main()
