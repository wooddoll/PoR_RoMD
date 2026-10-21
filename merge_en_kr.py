import json

def merge_json_files_keep_order(en_file_path, kr_file_path, output_file_path):
    # 1. 영문 및 한글 JSON 파일 읽기 (파이썬 3.7+부터 dict는 순서를 유지합니다)
    with open(en_file_path, 'r', encoding='utf-8') as f:
        en_data = json.load(f)
        
    with open(kr_file_path, 'r', encoding='utf-8') as f:
        kr_data = json.load(f)
        
    merged_data = {}
    
    # 2. 영문 파일(원본)의 대분류 키 순서대로 순회
    for main_key, en_item in en_data.items():
        # 한글 파일에서 동일한 키 가져오기 (없으면 빈 딕셔너리)
        kr_item = kr_data.get(main_key, {})
        
        # 기본 구조 생성
        merged_data[main_key] = {
            "selections_en": en_item.get("selections", []),
            "selections_kr": kr_item.get("selections", []),
            "keys": {}
        }
        
        # 3. 영문 파일의 소분류 keys 순서대로 순회
        en_keys = en_item.get("keys", {})
        kr_keys = kr_item.get("keys", {})
        
        for sub_key, en_sub in en_keys.items():
            kr_sub = kr_keys.get(sub_key, {})
            
            # 순서를 그대로 유지하며 데이터 병합
            merged_data[main_key]["keys"][sub_key] = {
                "dialogue_en": en_sub.get("dialogue", ""),
                "dialogue_kr": kr_sub.get("dialogue", ""),
                "speaker": en_sub.get("speaker", kr_sub.get("speaker", "")),
                "desc": en_sub.get("desc", kr_sub.get("desc", ""))
            }
            
    # 4. 합쳐진 데이터를 새 파일로 저장
    with open(output_file_path, 'w', encoding='utf-8') as f:
        json.dump(merged_data, f, indent=2, ensure_ascii=False)
        
    print(f"성공적으로 원본 순서를 유지하여 '{output_file_path}'에 저장했습니다!")

# 사용 예시
en_json_path = 'dump/dump.json'
kr_json_path = 'translation/to_merge_fixed_reordered.json'
output_json_path = 'translation/merged_en_kr.json'

merge_json_files_keep_order(en_json_path, kr_json_path, output_json_path)


