import json

# 정렬본(speaker → "퀘스트,대사" 키)의 번역을 원본 구조로 되돌린다. 키로 찾으므로 순서가 달라도 안전
def main():
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

if __name__ == "__main__":
    main()