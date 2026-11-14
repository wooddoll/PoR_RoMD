import json

def main2():
    sorted_path = "translation/merged_en_kr_sorted.json"
    original_path = "dump/merged_en_kr.json"
    output_path = "translation/merged_en_kr_restored.json"

    with open(sorted_path, "r", encoding="utf-8") as f:
        sorted_data = json.load(f)

    with open(original_path, "r", encoding="utf-8") as f:
        original_data = json.load(f)
    
    for quest_id, quest_data in original_data.items():
        for dialogue_id, entry in quest_data.items():
            keyT = f"{quest_id},{dialogue_id}"            
            speaker = entry.get("speaker", "")

            
            _data = sorted_data[speaker][keyT]
            dialogue_en = _data.get("dialogue_en", "")
            dialogue = _data.get("dialogue", "")
            desc = _data.get("desc", "")
            
            if dialogue_en:
                original_data[quest_id][dialogue_id]["dialogue_en"] = dialogue_en
            if dialogue:
                original_data[quest_id][dialogue_id]["dialogue"] = dialogue
            if desc:
                original_data[quest_id][dialogue_id]["desc"] = desc
    
    with open(output_path, "w", encoding="utf-8") as f:
        json.dump(original_data, f, ensure_ascii=False, indent=2)

def main():
    sorted_path = "dump/merged_en_kr_sorted.json"
    original_path = "translation/merged_en_kr.json"
    output_path = "translation/merged_en_kr_restored.json"

    with open(sorted_path, "r", encoding="utf-8") as f:
        sorted_data = json.load(f)

    with open(original_path, "r", encoding="utf-8") as f:
        original_data = json.load(f)

    # 원본 구조에서 quest_id와 dialogue_id를 speaker별로 수집
    # speaker -> [(quest_id, dialogue_id, dialogue_en, dialogue)]
    original_entries = {}
    for quest_id, quest_data in original_data.items():
        for dialogue_id, entry in quest_data.items():
            speaker = entry.get("speaker", "")
            dialogue_en = entry.get("dialogue_en", "")
            dialogue = entry.get("dialogue", "")
            desc = entry.get("desc", "")

            if speaker not in original_entries:
                original_entries[speaker] = []
            original_entries[speaker].append((quest_id, dialogue_id, dialogue_en, dialogue))

    # 정렬된 데이터에서 speaker별 dialogue_en/dialogue 목록을 가져와 원본 구조로 복구
    result = {}
    for speaker, speaker_data in sorted_data.items():
        dialogue_en_list = speaker_data.get("dialogue_en", [])
        dialogue_list = speaker_data.get("dialogue", [])

        if speaker not in original_entries:
            continue

        entries = original_entries[speaker]

        for i, (quest_id, dialogue_id, orig_dialogue_en, orig_dialogue) in enumerate(entries):
            if quest_id not in result:
                result[quest_id] = {}

            # 정렬된 데이터에서 해당 인덱스의 값을 사용 (없으면 원본 값 유지)
            new_dialogue_en = dialogue_en_list[i] if i < len(dialogue_en_list) else orig_dialogue_en
            new_dialogue = dialogue_list[i] if i < len(dialogue_list) else orig_dialogue

            result[quest_id][dialogue_id] = {
                "dialogue_en": new_dialogue_en,
                "dialogue": new_dialogue,
                "speaker": speaker,
                "desc": original_data[quest_id][dialogue_id].get("desc", "")
            }

    with open(output_path, "w", encoding="utf-8") as f:
        json.dump(result, f, ensure_ascii=False, indent=2)

    print(f"완료: {output_path}")

if __name__ == "__main__":
    main2()