import sys
import os
import copy
import json

# Reuse functions from read_all_idx.py and rebuild_all_db.py
from read_all_idx import (
    read_all_idx,
    read_all_db,
    build_sections,
    is_null,
    save_combined_to_json,
)
from rebuild_all_db import (
    serialize_section,
    rebuild_all_db,
    write_all_idx,
    write_por_files,
    write_address_verification,
    load_combined_from_json,
    restore_from_combined,
)



def merge_dialogue_data(base_combined, merge_combined):
    """
    base_combined를 기준으로 merge_combined의 'selections'와 'dialogue'만 병합합니다.

    - merge_combined 쪽 값이 없거나 비어있는 경우: base 유지
    - merge_combined 쪽 값이 유효한 경우: 업데이트
    - speaker, desc, header, address 등 나머지 필드는 원본 그대로 유지
    """
    # 원본 데이터 보호를 위해 깊은 복사
    merged = copy.deepcopy(base_combined)

    total_sections = len(base_combined)
    selection_updated_sections = 0
    dialogue_updated_keys = 0

    for sec_key, base_sec in merged.items():
        # merge_combined에 해당 섹션이 없으면 통과
        merge_sec = merge_combined.get(sec_key)
        if not merge_sec:
            continue

        # ---------------------------------------------------------
        # 1. 'selections' 병합 ([str] 리스트)
        # ---------------------------------------------------------
        merge_selections = merge_sec.get('selections')
        if isinstance(merge_selections, list) and len(merge_selections) > 0:
            base_selections = base_sec.get('selections', [])
            updated_selections = list(base_selections)
            selection_changed = False

            for i, m_sel in enumerate(merge_selections):
                if not is_null(m_sel):
                    if i < len(updated_selections):
                        # 기존 항목이 있고 값이 다르면 업데이트
                        if updated_selections[i] != m_sel:
                            updated_selections[i] = m_sel
                            selection_changed = True
                    else:
                        # base보다 merge 쪽에 더 많은 selection 항목이 있는 경우 추가
                        updated_selections.append(m_sel)
                        selection_changed = True

            if selection_changed:
                base_sec['selections'] = updated_selections
                selection_updated_sections += 1

        # ---------------------------------------------------------
        # 2. 'keys' 내부의 'dialogue' 병합
        # ---------------------------------------------------------
        merge_keys = merge_sec.get('keys')
        base_keys = base_sec.get('keys')

        if isinstance(merge_keys, dict) and isinstance(base_keys, dict):
            for k_name, b_key_entry in base_keys.items():
                m_key_entry = merge_keys.get(k_name)
                if not m_key_entry or not isinstance(m_key_entry, dict):
                    continue

                m_dialogue = m_key_entry.get('dialogue')
                b_dialogue = b_key_entry.get('dialogue')

                # merge 쪽 dialogue가 비어있지 않고 원본과 다르면 업데이트
                if not is_null(m_dialogue) and m_dialogue != b_dialogue:
                    b_key_entry['dialogue'] = m_dialogue
                    dialogue_updated_keys += 1

    print("=== Merge Summary ===")
    print(f"Total sections: {total_sections}")
    print(f"Sections with updated selections: {selection_updated_sections}")
    print(f"Keys with updated dialogue: {dialogue_updated_keys}")

    return merged


def check_keys_a_in_b(combined_a, combined_b, show_details=True):
    """
    A를 기준으로 B에 누락된 섹션 키 및 내부 대사 키(keys)가 있는지 검사합니다.

    :param combined_a: 기준이 되는 딕셔너리 (A)
    :param combined_b: 검증 대상 딕셔너리 (B)
    :param show_details: 누락된 키 상세 목록 출력 여부
    :return: 검증 결과 요약 딕셔너리
    """
    missing_sections = []
    missing_inner_keys = {}  # { sec_key: [missing_key1, missing_key2, ...] }

    total_a_sections = len(combined_a)
    total_a_inner_keys = 0
    total_missing_inner_keys = 0

    # 1. A의 섹션 순회
    for sec_key, sec_data_a in combined_a.items():
        keys_a = sec_data_a.get("keys", {}) if isinstance(sec_data_a, dict) else {}
        total_a_inner_keys += len(keys_a)

        # B에 해당 섹션 자체가 없는 경우
        if sec_key not in combined_b:
            missing_sections.append(sec_key)
            total_missing_inner_keys += len(keys_a)
            continue

        # 2. B에 섹션은 있으나 내부 keys 딕셔너리 비교
        sec_data_b = combined_b[sec_key]
        keys_b = sec_data_b.get("keys", {}) if isinstance(sec_data_b, dict) else {}

        # A에는 있지만 B에는 없는 내부 대사 키 추출
        diff_keys = [k for k in keys_a if k not in keys_b]
        if diff_keys:
            missing_inner_keys[sec_key] = diff_keys
            total_missing_inner_keys += len(diff_keys)

    # 모든 키 존재 여부 (완전 일치 여부)
    is_all_present = (len(missing_sections) == 0 and total_missing_inner_keys == 0)

    # ---------------------------------------------------------
    # 결과 출력
    # ---------------------------------------------------------
    print("=" * 50)
    print("           [ Key Comparison: A -> B ]")
    print("=" * 50)
    print(f"기준 A 섹션 수         : {total_a_sections}")
    print(f"B에 누락된 섹션 수     : {len(missing_sections)}")
    print(f"기준 A 대사 키 총합    : {total_a_inner_keys}")
    print(f"B에 누락된 대사 키 총합: {total_missing_inner_keys}")
    print("-" * 50)

    if is_all_present:
        print(">> [PASS] A의 모든 섹션 및 내부 대사 키가 B에 존재합니다!")
    else:
        print(">> [FAIL] B에 일부 키가 누락되어 있습니다.")

        if show_details:
            if missing_sections:
                print("\n[누락된 섹션 목록]")
                for sec in missing_sections:
                    print(f"  - {sec}")

            if missing_inner_keys:
                print("\n[섹션 내 누락된 대사 키 목록]")
                for sec, keys in missing_inner_keys.items():
                    print(f"  - 섹션 '{sec}' (누락 {len(keys)}개): {keys[:5]}" + ("..." if len(keys) > 5 else ""))

    print("=" * 50)

    return {
        "is_all_present": is_all_present,
        "missing_sections": missing_sections,
        "missing_inner_keys": missing_inner_keys,
        "stats": {
            "total_a_sections": total_a_sections,
            "missing_sections_count": len(missing_sections),
            "total_a_inner_keys": total_a_inner_keys,
            "missing_inner_keys_count": total_missing_inner_keys
        }
    }

if __name__ == '__main__':
    ##========================
    #json_A = load_combined_from_json("dump/dump.json")
    #json_B = load_combined_from_json("dump/to_merge_fixed.json")
    #check_keys_a_in_b(json_B, json_A)
    #exit()
    ##==========================
        
    if len(sys.argv) < 3:
        print("Usage: python merge_dialogue.py <game_dir> <to_merge.json>")
        print("  <game_dir>     - Path to the game directory (e.g., '../Pool of Radiance')")
        print("  <to_merge.json> - JSON file containing merged dialogue data")
        sys.exit(1)

    game_dir = sys.argv[1]
    merge_json_path = sys.argv[2]

    # Paths
    dialogue_dir = os.path.join(game_dir, 'Data', 'Dialogue')
    output_dir = os.path.join(game_dir, 'Data_kr', 'Dialogue')

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