import json

def main():
    sorted_path = "translation/revised_dialogues_sorted.json"
    original_path = "translation/revised_dialogues.json"
    output_path = "translation/revised_dialogues_restored.json"

    with open(sorted_path, "r", encoding="utf-8") as f:
        sorted_data = json.load(f)

    with open(original_path, "r", encoding="utf-8") as f:
        original_data = json.load(f)

    # 원본 구조에서 quest_id와 dialogue_id를 speaker별로 수집
    # speaker -> [(quest_id, dialogue_id, dialogue_prev, dialogue)]
    original_entries = {}
    for quest_id, quest_data in original_data.items():
        for dialogue_id, entry in quest_data.items():
            speaker = entry.get("speaker", "")
            dialogue_prev = entry.get("dialogue_prev", "")
            dialogue = entry.get("dialogue", "")

            if speaker not in original_entries:
                original_entries[speaker] = []
            original_entries[speaker].append((quest_id, dialogue_id, dialogue_prev, dialogue))

    # 정렬된 데이터에서 speaker별 dialogue_prev/dialogue 목록을 가져와 원본 구조로 복구
    result = {}
    for speaker, speaker_data in sorted_data.items():
        dialogue_prev_list = speaker_data.get("dialogue_prev", [])
        dialogue_list = speaker_data.get("dialogue", [])

        if speaker not in original_entries:
            continue

        entries = original_entries[speaker]

        for i, (quest_id, dialogue_id, orig_prev, orig_dialogue) in enumerate(entries):
            if quest_id not in result:
                result[quest_id] = {}

            # 정렬된 데이터에서 해당 인덱스의 값을 사용 (없으면 원본 값 유지)
            new_prev = dialogue_prev_list[i] if i < len(dialogue_prev_list) else orig_prev
            new_dialogue = dialogue_list[i] if i < len(dialogue_list) else orig_dialogue

            result[quest_id][dialogue_id] = {
                "dialogue_prev": new_prev,
                "dialogue": new_dialogue
            }

    with open(output_path, "w", encoding="utf-8") as f:
        json.dump(result, f, ensure_ascii=False, indent=2)

    print(f"완료: {output_path}")

if __name__ == "__main__":
    main()