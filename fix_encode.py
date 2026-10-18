
import sys
import os
import copy
import re
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

def repair_and_load_json(broken_json_path, base_combined=None):
    print("1. 손상된 JSON 파일 텍스트 로드...")
    with open(broken_json_path, "r", encoding="utf-8", errors="replace") as f:
        text = f.read()

    # 2. 증발한 닫는 따옴표(") 복구
    # 패턴: "키이름? 뒤에 콜론(:)이 바로 오는 경우 -> "키이름?": 로 복구
    # (일반 물음표 '?' 또는 유니코드 대체문자 '\ufffd' 대응)
    fixed_text = re.sub(r'("[^"\n\r]*[\?\ufffd])(\s*:)', r'\1"\2', text)

    print("2. JSON 파싱 시도 (strict=False)...")
    try:
        data = json.loads(fixed_text, strict=False)
        print(">> JSON 로드 성공!")
    except Exception as e:
        print(f">> 여전히 JSON 파싱 에러 발생: {e}")
        return None

    # 3. base_combined가 있다면 '?'로 깨진 키를 원본 키값으로 복원
    if base_combined:
        print("3. 깨진 키(?)를 원본 base_combined 키로 복원 중...")
        repaired_keys_count = 0

        for sec_name, sec_data in data.items():
            base_sec = base_combined.get(sec_name)
            if not base_sec or 'keys' not in sec_data:
                continue

            base_keys = base_sec.get('keys', {})
            current_keys = sec_data['keys']
            new_keys = {}

            for k, v in current_keys.items():
                if k.endswith('?') or k.endswith('\ufffd'):
                    prefix = k[:-1]  # '?' 앞부분 (예: 'caldmaY')
                    # 원본에서 해당 접두사로 시작하는 정상 키 찾기
                    matched_orig_key = None
                    for orig_k in base_keys.keys():
                        orig_k_str = orig_k.decode('latin-1') if isinstance(orig_k, bytes) else str(orig_k)
                        if orig_k_str.startswith(prefix):
                            matched_orig_key = orig_k
                            break

                    if matched_orig_key:
                        new_keys[matched_orig_key] = v
                        repaired_keys_count += 1
                        # print(f"  복원: '{k}' -> '{matched_orig_key}'")
                    else:
                        new_keys[k] = v
                else:
                    new_keys[k] = v

            sec_data['keys'] = new_keys

        print(f">> 총 {repaired_keys_count}개의 깨진 대사 키 복원 완료!")

    return data

def test1(json_path):
    # check_raw.py
    target_line = 6093

    with open(json_path, "r", encoding="utf-8", errors="replace") as f:
        lines = f.readlines()

    print(f"=== {target_line}번째 줄 주변 원본 텍스트 확인 ===")
    for i in range(max(0, target_line - 10), min(len(lines), target_line + 15)):
        line_no = i + 1
        mark = ">>" if line_no == target_line else "  "
        print(f"{mark} {line_no:5d}: {lines[i]}", end="")


def is_empty_val(val):
    """값이 비어있거나, None, 빈 문자열, 'null' 문자열인지 확인"""
    if val is None:
        return True
    if isinstance(val, str) and (val.strip() == "" or val.strip().lower() == "null"):
        return True
    if 'is_null' in globals() and callable(globals()['is_null']):
        return globals()['is_null'](val)
    return False


def restore_and_merge_safely(base_combined, broken_json_path):
    """
    손상된 JSON(따옴표 누락, '?' 중복 키)을 안전하게 읽어
    base_combined에 'selections'와 'dialogue'만 선택적으로 병합합니다.
    """
    print("1. 손상된 JSON 텍스트 로드 및 따옴표(\") 정밀 복구...")
    with open(broken_json_path, "r", encoding="utf-8", errors="replace") as f:
        text = f.read()

    # 따옴표가 빠진 '  "caldmaY?: {' 형태를 '"caldmaY?": {' 로 자동 복구
    fixed_text = re.sub(
        r'(^\s*"[^"\n\r:]+[\?\ufffd])(\s*:\s*\{)',
        r'\1"\2',
        text,
        flags=re.MULTILINE
    )

    print("2. 중복 키 보존 모드(List)로 JSON 파싱...")
    try:
        # object_pairs_hook=list로 파싱하여 '?'로 중복된 키가 덮어써지지 않도록 순서대로 보존
        raw_sections_list = json.loads(fixed_text, strict=False, object_pairs_hook=list)
    except Exception as e:
        print(f">> 파싱 에러 발생: {e}")
        return None

    print("3. 'selections' 및 'dialogue' 데이터 병합 시작...")
    restored_dialogue_count = 0
    duplicate_recovered_count = 0
    updated_selection_sections = 0

    for sec_name, sec_pairs in raw_sections_list:
        base_sec = base_combined.get(sec_name)
        if not base_sec:
            continue

        sec_dict = dict(sec_pairs)

        # -------------------------------------------------------------
        # 1. 'selections' (선택지) 병합
        # -------------------------------------------------------------
        merge_sels = sec_dict.get("selections")
        if isinstance(merge_sels, list) and len(merge_sels) > 0:
            base_sels = base_sec.get("selections", [])
            updated_sels = list(base_sels)
            sel_changed = False

            for i, m_sel in enumerate(merge_sels):
                # merge 쪽 선택지 값이 비어있지 않은 경우에만 반영
                if not is_empty_val(m_sel):
                    if i < len(updated_sels):
                        if updated_sels[i] != m_sel:
                            updated_sels[i] = m_sel
                            sel_changed = True
                    else:
                        # base보다 merge 쪽에 선택지가 더 많으면 추가
                        updated_sels.append(m_sel)
                        sel_changed = True

            if sel_changed:
                base_sec["selections"] = updated_sels
                updated_selection_sections += 1

        # -------------------------------------------------------------
        # 2. 'keys' 내부 'dialogue' 병합 (순서 기반 1:1 매칭)
        # -------------------------------------------------------------
        raw_keys_list = sec_dict.get("keys", [])
        if not isinstance(raw_keys_list, list):
            continue

        base_keys = base_sec.get("keys", {})
        used_base_keys = set()  # 중복 키 매칭 방지를 위한 사용 추적 셋

        for m_key, m_val_pairs in raw_keys_list:
            m_val = dict(m_val_pairs) if isinstance(m_val_pairs, list) else m_val_pairs
            if not isinstance(m_val, dict):
                continue

            m_dialogue = m_val.get("dialogue")
            # 대사가 없거나 비어있으면 원본 유지
            if is_empty_val(m_dialogue):
                continue

            target_base_key = None

            # [경우 A] 키가 깨진 경우 ('?' 또는 '\ufffd'로 끝남)
            if m_key.endswith("?") or m_key.endswith("\ufffd"):
                prefix = m_key[:-1]  # 예: 'caldmaY'
                
                # 원본 키들 중 prefix로 시작하며 아직 매칭되지 않은 키를 순서대로 탐색
                for orig_k in base_keys.keys():
                    orig_k_str = orig_k.decode('latin-1') if isinstance(orig_k, bytes) else str(orig_k)
                    if orig_k_str.startswith(prefix) and orig_k not in used_base_keys:
                        target_base_key = orig_k
                        used_base_keys.add(orig_k)
                        duplicate_recovered_count += 1
                        break

            # [경우 B] 정상 키인 경우
            else:
                for orig_k in base_keys.keys():
                    orig_k_str = orig_k.decode('latin-1') if isinstance(orig_k, bytes) else str(orig_k)
                    if orig_k_str == m_key:
                        target_base_key = orig_k
                        used_base_keys.add(orig_k)
                        break

            # 원본 데이터에만 존재하는 키 타입(bytes/str)을 그대로 유지하며 dialogue만 갱신
            if target_base_key is not None and target_base_key in base_keys:
                if base_keys[target_base_key].get("dialogue") != m_dialogue:
                    base_keys[target_base_key]["dialogue"] = m_dialogue
                    restored_dialogue_count += 1

    print("=" * 55)
    print(">> [병합 및 복원 결과]")
    print(f">> selections 변경된 섹션 수         : {updated_selection_sections}개")
    print(f">> dialogue 갱신된 대사 수          : {restored_dialogue_count}개")
    print(f">> '?' 중복 키 충돌에서 구출된 키 수 : {duplicate_recovered_count}개")
    print("=" * 55)

    return base_combined


def init_transl_check(to_translation):
    for sec_name, sec_data in to_translation.items():
        if sec_data.get("selections"):
            to_translation[sec_name]['_check'] = False
        
        if sec_data.get("keys"):
            for key_name in sec_data['keys'].keys():
                to_translation[sec_name]['keys'][key_name]['_check'] = False
       
# --- 실행 부 ---
if __name__ == "__main__":
    basefile = "dump/dump.json"
    base_combined = load_combined_from_json(basefile)
    init_transl_check(base_combined)
    
    filepath = "translation/translated11.json"
    repaired_data = restore_and_merge_safely(base_combined, filepath)
    
    if repaired_data:
        with open("dump/to_merge_fixed.json", "w", encoding="utf-8") as f:
            json.dump(repaired_data, f, ensure_ascii=False, indent=2)
        print(">> 'to_merge_fixed.json'으로 안전하게 저장되었습니다!")