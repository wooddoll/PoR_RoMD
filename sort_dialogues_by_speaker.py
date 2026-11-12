import json

def main():
    input_path = "translation/revised_dialogues.json"
    output_path = "translation/revised_dialogues_sorted.json"

    with open(input_path, "r", encoding="utf-8") as f:
        data = json.load(f)

    # speaker별로 dialogue_prev와 dialogue를 수집
    result = {}
    for quest_id, quest_data in data.items():
        for dialogue_id, entry in quest_data.items():
            speaker = entry.get("speaker", "")
            dialogue_prev = entry.get("dialogue_prev", "")
            dialogue = entry.get("dialogue", "")

            if speaker not in result:
                result[speaker] = {
                    "dialogue_prev": [],
                    "dialogue": []
                }

            if dialogue_prev:
                result[speaker]["dialogue_prev"].append(dialogue_prev)
            if dialogue:
                result[speaker]["dialogue"].append(dialogue)

    with open(output_path, "w", encoding="utf-8") as f:
        json.dump(result, f, ensure_ascii=False, indent=2)

    print(f"완료: {output_path}")

if __name__ == "__main__":
    main()