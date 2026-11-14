import json

from typing import Dict, List, Any
from collections import defaultdict, OrderedDict


def check_and_sort(
    src: Dict[Any, List[str]],
    *,
    case_insensitive: bool = True,
) -> Dict[Any, List[str]]:
    """
    - src : {key: [str, …], …}
    - case_insensitive : True이면 키를 소문자로 통일해 비교·정렬한다.
    반환값은 정렬된 새 딕셔너리.
    중복 키가 발견되면 ValueError 를 발생시킨다.
    """
    # 1️⃣ 대소문자 무시한 키 → 원본 키(복수) 매핑
    norm_map: defaultdict[str, List[Any]] = defaultdict(list)
    for k in src.keys():
        norm_key = k.lower() if case_insensitive else k
        norm_map[norm_key].append(k)
    # 2️⃣ 중복 확인
    dup = {nk: orig for nk, orig in norm_map.items() if len(orig) > 1}
    if dup:
        dup_msg = ", ".join(
            f"'{nk}' → {orig}" for nk, orig in dup.items()
        )
        #raise ValueError(f"대소문자 무시 시 중복 키가 존재합니다: {dup_msg}")
        print(f"대소문자 무시 시 중복 키가 존재합니다: {dup_msg}")
        
    # 3️⃣ 정렬 (소문자 기준)
    # Python 3.7+ 은 삽입 순서를 보존하므로 OrderedDict 로 만든다.
    sorted_items = sorted(
        src.items(),
        key=lambda kv: kv[0].lower() if case_insensitive else kv[0]
    )
    return dict(sorted_items)   # or OrderedDict(sorted_items) if you prefer explicit order


def main():
    input_path = "translation/merged_en_kr.json"
    output_path = "translation/merged_en_kr_sorted.json"

    with open(input_path, "r", encoding="utf-8") as f:
        data = json.load(f)

    # speaker별로 dialogue_prev와 dialogue를 수집
    result = {}
    for quest_id, quest_data in data.items():
        for dialogue_id, entry in quest_data.items():
            speaker = entry.get("speaker", "")
            dialogue_en = entry.get("dialogue_en", "")
            dialogue = entry.get("dialogue", "")

            if speaker not in result:
                result[speaker] = {
                    "dialogue_en": [],
                    "dialogue": []
                }

            if dialogue_en:
                result[speaker]["dialogue_en"].append(dialogue_en)
            if dialogue:
                result[speaker]["dialogue"].append(dialogue)
    
    result_sorted = check_and_sort(result, case_insensitive=False)
    
    with open(output_path, "w", encoding="utf-8") as f:
        json.dump(result_sorted, f, ensure_ascii=False, indent=2)

    print(f"완료: {output_path}")


def main2():
    input_path = "translation/merged_en_kr.json"
    output_path = "dump/merged_en_kr_sorted.json"

    with open(input_path, "r", encoding="utf-8") as f:
        data = json.load(f)

    # speaker별로 dialogue_prev와 dialogue를 수집
    result = {}
    for quest_id, quest_data in data.items():
        for dialogue_id, entry in quest_data.items():
            speaker = entry.get("speaker", "")
            keyT = f"{quest_id},{dialogue_id}"
            
            if speaker not in result:
                result[speaker] = {}

            result[speaker][keyT] = {
                        "dialogue_en": entry.get("dialogue_en", ""),
                        "dialogue": entry.get("dialogue", ""),
                        "desc": entry.get("desc", ""),
                    }
    
    result_sorted = check_and_sort(result)
    
    with open(output_path, "w", encoding="utf-8") as f:
        json.dump(result_sorted, f, ensure_ascii=False, indent=2)

    print(f"완료: {output_path}")


if __name__ == "__main__":
    main2()