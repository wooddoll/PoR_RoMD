import base64
import json
import os
import struct
import sys

def manual_offset_to_va(exe_path, file_offset):
    """
    파일 내 오프셋을 가상 메모리 주소(VA)로 변환합니다.
    
    Args:
        exe_path: PE 실행 파일 경로
        file_offset: 파일 내 오프셋 (int)
    
    Returns:
        int 또는 None: 가상 메모리 주소(VA) 또는 변환 실패 시 None
    """
    with open(exe_path, 'rb') as f:
        # DOS 헤더에서 PE 헤더 시작 위치(e_lfanew) 읽기 (오프셋 0x3C)
        f.seek(0x3C)
        pe_offset = struct.unpack('<I', f.read(4))[0]
        
        # Optional Header에서 ImageBase 읽기 (PE 헤더 시작점 기준 + 52바이트 위치)
        f.seek(pe_offset + 52)
        image_base = struct.unpack('<I', f.read(4))[0]
        
        # 섹션 개수 읽기 (PE 헤더 시작점 기준 + 6바이트 위치)
        f.seek(pe_offset + 6)
        num_of_sections = struct.unpack('<H', f.read(2))[0]
        
        # 섹션 헤더 테이블 시작 위치로 이동 (PE 시그니처 4B + FileHeader 20B + OptionalHeader 224B)
        # ※ 일반적인 32비트 가정이므로 실행 파일 구조에 따라 오프셋은 미세하게 다를 수 있습니다.
        section_table_offset = pe_offset + 4 + 20 + 224
        
        # 각 섹션을 돌며 오프셋이 포함된 섹션 찾기
        for i in range(num_of_sections):
            f.seek(section_table_offset + (i * 40)) # 섹션 헤더 하나당 40바이트
            section_name = f.read(8).decode('utf-8', errors='ignore').strip('\x00')
            
            f.seek(section_table_offset + (i * 40) + 12)
            virtual_address = struct.unpack('<I', f.read(4))[0] # 메모리 시작점(RVA)
            
            f.seek(section_table_offset + (i * 40) + 16)
            size_of_raw_data = struct.unpack('<I', f.read(4))[0] # 파일 내 섹션 크기
            
            f.seek(section_table_offset + (i * 40) + 20)
            pointer_to_raw_data = struct.unpack('<I', f.read(4))[0] # 파일 시작점(RAW)
            
            # 입력한 오프셋이 이 섹션 범위 안에 있는지 확인
            if pointer_to_raw_data <= file_offset < (pointer_to_raw_data + size_of_raw_data):
                # 공식 적용: RVA = 오프셋 - 파일시작점 + 메모리시작점
                rva = file_offset - pointer_to_raw_data + virtual_address
                final_va = rva + image_base
                
                print(f"[{section_name} 섹션에서 주소 발견]")
                print(f"-> Memory VA: 0x{final_va:X}")
                return final_va
                
        print("해당 오프셋이 어떤 섹션 범위에도 속하지 않습니다.")
        return None


def va_to_manual_offset(exe_path, va):
    """
    가상 메모리 주소(VA)를 파일 내 오프셋으로 변환합니다.
    
    Args:
        exe_path: PE 실행 파일 경로
        va: 가상 메모리 주소 (int)
    
    Returns:
        int 또는 None: 파일 내 오프셋 또는 변환 실패 시 None
    """
    with open(exe_path, 'rb') as f:
        # DOS 헤더에서 PE 헤더 시작 위치(e_lfanew) 읽기 (오프셋 0x3C)
        f.seek(0x3C)
        pe_offset = struct.unpack('<I', f.read(4))[0]
        
        # Optional Header에서 ImageBase 읽기 (PE 헤더 시작점 기준 + 52바이트 위치)
        f.seek(pe_offset + 52)
        image_base = struct.unpack('<I', f.read(4))[0]
        
        # 섹션 개수 읽기 (PE 헤더 시작점 기준 + 6바이트 위치)
        f.seek(pe_offset + 6)
        num_of_sections = struct.unpack('<H', f.read(2))[0]
        
        # 섹션 헤더 테이블 시작 위치로 이동 (PE 시그니처 4B + FileHeader 20B + OptionalHeader 224B)
        section_table_offset = pe_offset + 4 + 20 + 224
        
        # RVA 계산
        rva = va - image_base
        
        # 각 섹션을 돌며 RVA가 포함된 섹션 찾기
        for i in range(num_of_sections):
            f.seek(section_table_offset + (i * 40)) # 섹션 헤더 하나당 40바이트
            section_name = f.read(8).decode('utf-8', errors='ignore').strip('\x00')
            
            f.seek(section_table_offset + (i * 40) + 12)
            virtual_address = struct.unpack('<I', f.read(4))[0] # 메모리 시작점(RVA)
            
            f.seek(section_table_offset + (i * 40) + 16)
            size_of_raw_data = struct.unpack('<I', f.read(4))[0] # 파일 내 섹션 크기
            
            f.seek(section_table_offset + (i * 40) + 20)
            pointer_to_raw_data = struct.unpack('<I', f.read(4))[0] # 파일 시작점(RAW)
            
            # RVA가 이 섹션 범위 안에 있는지 확인
            if virtual_address <= rva < (virtual_address + size_of_raw_data):
                # 공식 적용: 파일 오프셋 = RVA - 메모리시작점 + 파일시작점
                file_offset = rva - virtual_address + pointer_to_raw_data
                
                print(f"[{section_name} 섹션에서 오프셋 발견]")
                print(f"-> File Offset: 0x{file_offset:X}")
                return file_offset
                
        print("해당 VA가 어떤 섹션 범위에도 속하지 않습니다.")
        return None
        
def read_binary_to_json(binary_path, json_path, base_address=0x00000000):
    """
    바이너리 파일을 128바이트씩 읽어서 base64로 인코딩하여 JSON에 저장합니다.
    
    Args:
        binary_path: 읽을 바이너리 파일 경로
        json_path: 저장할 JSON 파일 경로
        base_address: 시작 주소 (기본값: 0x00000000)
    """
    result = {}
    
    with open(binary_path, 'rb') as f:
        # 128바이트씩 4번 읽기 (총 512바이트)
        for i in range(4):
            data = f.read(128)
            if not data:
                break
            
            # 주소 계산 (0x00xxxxxx 형식의 32비트 주소)
            address = base_address + (i * 128)
            address_str = f"0x{address:08x}"
            
            # base64 인코딩
            encoded = base64.b64encode(data).decode('ascii')
            
            # JSON 구조: { name : { address: <주소>, checkdata: <base64> } }
            name = f"block_{i}"
            result[name] = {
                "address": address_str,
                "checkdata": encoded
            }
    
    # JSON 파일로 저장
    with open(json_path, 'w', encoding='utf-8') as f:
        json.dump(result, f, indent=2, ensure_ascii=False)
    
    print(f"완료: {binary_path} -> {json_path}")
    print(f"총 {len(result)}개 블록 저장됨")
    for name, info in result.items():
        print(f"  {name}: address={info['address']}, checkdata 길이={len(info['checkdata'])}")


def load_checkdata_json(json_path):
    """
    JSON 파일을 읽어서 address는 int로, checkdata는 바이트 배열로 변환하여 반환합니다.
    
    Args:
        json_path: 읽을 JSON 파일 경로
    
    Returns:
        dict: { name: { "address": int, "checkdata": bytes } } 형식의 딕셔너리
    """
    with open(json_path, 'r', encoding='utf-8') as f:
        data = json.load(f)
    
    result = {}
    for name, info in data.items():
        # address 문자열("0x00400000")을 int로 변환
        address = int(info["address"], 16)
        # checkdata base64 문자열을 bytes로 디코딩
        checkdata = base64.b64decode(info["checkdata"])
        
        result[name] = {
            "address": address,
            "checkdata": checkdata
        }
    
    return result


def find_by_address(data, address):
    """
    주소로 블록을 조회합니다.
    
    Args:
        data: load_checkdata_json()의 반환값
        address: 조회할 주소 (int)
    
    Returns:
        dict 또는 None: { "address": int, "checkdata": bytes } 또는 해당 주소가 없으면 None
    """
    for name, info in data.items():
        if info["address"] == address:
            return info
    return None


def find_by_name(data, name):
    """
    이름으로 블록을 조회합니다.
    
    Args:
        data: load_checkdata_json()의 반환값
        name: 조회할 블록 이름 (예: "block_0")
    
    Returns:
        dict 또는 None: { "address": int, "checkdata": bytes } 또는 해당 이름이 없으면 None
    """
    return data.get(name)


def update_checkdata_from_exe(json_path, exe_path, output_json_path=None):
    """
    JSON의 address(VA)에 해당하는 exe 파일 위치에서 128바이트를 읽어
    checkdata를 업데이트하고 JSON 파일을 저장합니다.
    
    Args:
        json_path: 읽을 JSON 파일 경로
        exe_path: PE 실행 파일 경로
        output_json_path: 저장할 JSON 파일 경로 (기본값: 입력 JSON 파일 덮어쓰기)
    
    Returns:
        dict: 업데이트된 데이터 { name: { "address": int, "checkdata": bytes } }
    """
    # JSON 로드
    data = load_checkdata_json(json_path)
    
    # exe 파일 전체를 메모리에 로드
    with open(exe_path, 'rb') as f:
        exe_data = f.read()
    
    print(f"exe 파일 크기: {len(exe_data)}바이트")
    print(f"JSON에서 {len(data)}개 블록 로드됨")
    print()
    
    updated_data = {}
    
    for name, info in data.items():
        va = info["address"]
        old_checkdata = info["checkdata"]
        
        print(f"=== {name} ===")
        print(f"  address(VA): 0x{va:08x}")
        print(f"  기존 checkdata 길이: {len(old_checkdata)}바이트")
        
        # VA를 파일 오프셋으로 변환
        file_offset = va_to_manual_offset(exe_path, va)
        
        if file_offset is None:
            print(f"  ❌ VA 0x{va:08x}를 파일 오프셋으로 변환할 수 없습니다!")
            updated_data[name] = {
                "address": va,
                "checkdata": old_checkdata  # 기존 데이터 유지
            }
            print()
            continue
        
        # 파일 오프셋에서 128바이트 읽기
        if file_offset + 128 > len(exe_data):
            print(f"  ❌ 파일 오프셋 0x{file_offset:08x}에서 128바이트를 읽을 수 없습니다 (파일 끝 초과)!")
            updated_data[name] = {
                "address": va,
                "checkdata": old_checkdata  # 기존 데이터 유지
            }
            print()
            continue
        
        new_checkdata = exe_data[file_offset:file_offset + 128]
        
        # 기존 데이터와 비교
        if new_checkdata == old_checkdata:
            print(f"  ✅ 파일 오프셋 0x{file_offset:08x}에서 128바이트 읽음 (기존과 동일)")
        else:
            print(f"  🔄 파일 오프셋 0x{file_offset:08x}에서 128바이트 읽음 (기존과 다름 - 업데이트됨)")
        
        updated_data[name] = {
            "address": va,
            "checkdata": new_checkdata
        }
        print()
    
    # JSON 파일로 저장
    if output_json_path is None:
        output_json_path = json_path
    
    # JSON 저장 형식으로 변환 (address는 0x 형식 문자열, checkdata는 base64)
    save_data = {}
    for name, info in updated_data.items():
        save_data[name] = {
            "address": f"0x{info['address']:08x}",
            "checkdata": base64.b64encode(info["checkdata"]).decode('ascii')
        }
    
    with open(output_json_path, 'w', encoding='utf-8') as f:
        json.dump(save_data, f, indent=2, ensure_ascii=False)
    
    print(f"완료: {output_json_path}에 저장됨")
    
    return updated_data


def find_checkdata_in_exe(json_path, exe_path):
    """
    exe_test.json의 각 checkdata가 exe 파일의 어디에 위치하는지 확인합니다.
    파일 내에서 checkdata가 여러 번 나타날 수 있으므로 모든 위치를 검색합니다.
    
    Args:
        json_path: JSON 파일 경로 (exe_test.json)
        exe_path: PE 실행 파일 경로
    
    Returns:
        dict: { name: { "address": int, "matches": [ { "file_offset": int, "va": int, "section": str } ] } }
    """
    # JSON 로드
    data = load_checkdata_json(json_path)
    
    # exe 파일 전체를 메모리에 로드
    with open(exe_path, 'rb') as f:
        exe_data = f.read()
    
    print(f"exe 파일 크기: {len(exe_data)}바이트")
    print(f"JSON에서 {len(data)}개 블록 로드됨")
    print()
    
    results = {}
    
    for name, info in data.items():
        checkdata = info["checkdata"]
        expected_va = info["address"]
        
        print(f"=== {name} ===")
        print(f"  JSON의 address: 0x{expected_va:08x}")
        print(f"  checkdata 길이: {len(checkdata)}바이트")
        
        # exe 파일에서 checkdata의 모든 위치 검색
        matches = []
        search_start = 0
        
        while True:
            found_offset = exe_data.find(checkdata, search_start)
            if found_offset == -1:
                break
            
            # 파일 오프셋을 VA로 변환
            va = manual_offset_to_va(exe_path, found_offset)
            
            match_info = {
                "file_offset": found_offset,
                "va": va,
                "section": "found"
            }
            matches.append(match_info)
            
            # 다음 검색 위치로 이동 (겹치지 않도록)
            search_start = found_offset + len(checkdata)
        
        if matches:
            print(f"  총 {len(matches)}개 위치에서 발견:")
            for i, match in enumerate(matches):
                va_str = f"0x{match['va']:08x}" if match['va'] else "변환 실패"
                match_str = f"  [{i+1}] 파일 오프셋: 0x{match['file_offset']:08x}, VA: {va_str}"
                
                # JSON의 address와 비교
                if match['va'] == expected_va:
                    match_str += " ✅ (JSON address와 일치)"
                else:
                    match_str += " ⚠️ (JSON address와 다름)"
                
                print(match_str)
            
            results[name] = {
                "address": expected_va,
                "matches": matches
            }
        else:
            print(f"  ❌ exe 파일에서 checkdata를 찾을 수 없습니다!")
            results[name] = {
                "address": expected_va,
                "matches": []
            }
        
        print()
    
    return results


def main():
    if len(sys.argv) < 2:
        print("사용법:")
        print("  저장: python read_binary_to_json.py <바이너리파일> [JSON파일] [시작주소]")
        print("  조회: python read_binary_to_json.py --read <JSON파일> [주소]")
        print("  검색: python read_binary_to_json.py --find <JSON파일> <exe파일>")
        print("  업데이트: python read_binary_to_json.py --update <JSON파일> <exe파일> [출력JSON파일]")
        print("예시:")
        print("  python read_binary_to_json.py hook_test.bin output.json 0x00400000")
        print("  python read_binary_to_json.py --read output.json 0x00400080")
        print("  python read_binary_to_json.py --find exe_test.json ../Pool of Radiance/por2main.exe")
        print("  python read_binary_to_json.py --update exe_test.json ../Pool of Radiance/por2main.exe")
        sys.exit(1)
    
    # 업데이트 모드
    if sys.argv[1] == "--update":
        if len(sys.argv) < 4:
            print("오류: JSON 파일 경로와 exe 파일 경로가 필요합니다.")
            sys.exit(1)
        
        json_path = sys.argv[2]
        exe_path = sys.argv[3]
        
        # 출력 JSON 파일 경로 (선택)
        output_json_path = None
        if len(sys.argv) >= 5:
            output_json_path = sys.argv[4]
        
        if not os.path.exists(json_path):
            print(f"오류: JSON 파일을 찾을 수 없습니다: {json_path}")
            sys.exit(1)
        
        if not os.path.exists(exe_path):
            print(f"오류: exe 파일을 찾을 수 없습니다: {exe_path}")
            sys.exit(1)
        
        update_checkdata_from_exe(json_path, exe_path, output_json_path)
        return
    
    # 검색 모드
    if sys.argv[1] == "--find":
        if len(sys.argv) < 4:
            print("오류: JSON 파일 경로와 exe 파일 경로가 필요합니다.")
            sys.exit(1)
        
        json_path = sys.argv[2]
        exe_path = sys.argv[3]
        
        if not os.path.exists(json_path):
            print(f"오류: JSON 파일을 찾을 수 없습니다: {json_path}")
            sys.exit(1)
        
        if not os.path.exists(exe_path):
            print(f"오류: exe 파일을 찾을 수 없습니다: {exe_path}")
            sys.exit(1)
        
        find_checkdata_in_exe(json_path, exe_path)
        return
    
    # 조회 모드
    if sys.argv[1] == "--read":
        if len(sys.argv) < 3:
            print("오류: JSON 파일 경로가 필요합니다.")
            sys.exit(1)
        
        json_path = sys.argv[2]
        data = load_checkdata_json(json_path)
        
        print(f"JSON 파일에서 {len(data)}개 블록 로드됨:")
        for name, info in data.items():
            print(f"  {name}: address=0x{info['address']:08x}, checkdata 길이={len(info['checkdata'])}바이트")
        
        # 주소로 조회
        if len(sys.argv) >= 4:
            address = int(sys.argv[3], 16)
            found = find_by_address(data, address)
            if found:
                print(f"\n주소 0x{address:08x} 조회 결과:")
                print(f"  address: 0x{found['address']:08x} (int: {found['address']})")
                print(f"  checkdata: {found['checkdata']} (bytes, {len(found['checkdata'])}바이트)")
            else:
                print(f"\n주소 0x{address:08x}에 해당하는 블록이 없습니다.")
        
        return
    
    # 저장 모드
    binary_path = sys.argv[1]
    
    # JSON 파일 경로 (기본값: 입력 파일명 + .json)
    if len(sys.argv) >= 3:
        json_path = sys.argv[2]
    else:
        base_name = os.path.splitext(binary_path)[0]
        json_path = base_name + ".json"
    
    # 시작 주소 (기본값: 0x00000000)
    if len(sys.argv) >= 4:
        base_address = int(sys.argv[3], 16)
    else:
        base_address = 0x00000000
    
    if not os.path.exists(binary_path):
        print(f"오류: 파일을 찾을 수 없습니다: {binary_path}")
        sys.exit(1)
    
    read_binary_to_json(binary_path, json_path, base_address)


if __name__ == "__main__":
    main()