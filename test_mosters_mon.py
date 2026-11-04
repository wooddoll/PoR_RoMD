import struct
import os

def parse_monster_file(file_path):
    if not os.path.exists(file_path):
        print(f"[-] 파일을 찾을 수 없습니다: {file_path}")
        return None

    with open(file_path, 'rb') as f:
        # 1. 헤더 읽기 (버전 4바이트, 몬스터 개수 4바이트)
        header_data = f.read(8)
        if len(header_data) < 8:
            print("[-] 파일 헤더가 너무 짧습니다.")
            return None

        # 리틀 엔디안( < ), 2개의 unsigned int( II )로 파싱
        version, monster_count = struct.unpack('<II', header_data)

        # 코드 검증: 버전이 0x7a (십진수 122) 인지 확인
        if version != 0x7a:
            print(f"[-] 올바르지 않은 파일 버전입니다. (Expected: 0x7a, Found: {hex(version)})")
            return None

        print(f"[+] 파일 버전 검증 성공: {hex(version)}")
        print(f"[+] 로드할 몬스터 총 개수: {monster_count} 마리")
        print("-" * 50)

        monsters = []

        # 2. 각 몬스터 엔트리 루프
        for i in range(monster_count):
            # 고정 크기 0xe4 (228 바이트) 읽기
            entry_data = f.read(0xe4)
            if len(entry_data) < 0xe4:
                print(f"[-] {i}번째 몬스터 데이터를 읽는 중 파일이 예기치 않게 끝났습니다.")
                break

            # 0x26 오프셋의 1바이트(byte) 크기 값 추출
            # 가변 특수 데이터의 레코드 개수를 의미함
            extra_records_count = entry_data[0x26]

            # 가변 데이터 읽기 (개수 * 9 바이트)
            extra_data_list = []
            if extra_records_count > 0:
                for _ in range(extra_records_count):
                    extra_chunk = f.read(9)
                    if len(extra_chunk) == 9:
                        extra_data_list.append(extra_chunk)
                    else:
                        print(f"[-] {i}번째 몬스터의 가변 데이터를 읽는 중 오류 발생")

            # 데이터 정리를 위해 딕셔너리 구조화
            # 필요에 따라 entry_data[0x00:0x26] 구조를 struct.unpack으로 세분화할 수 있습니다.
            monster_info = {
                "index": i,
                "raw_stats_block": entry_data[0x00:0x26],  # 38바이트 기본 능력치 영역
                "extra_records_count": extra_records_count,
                "raw_remaining_block": entry_data[0x27:0xe4], # 스크립트, 리소스 ID 영역
                "extra_data_chunks": extra_data_list        # 9바이트짜리 가변 데이터 리스트
            }
            
            monsters.append(monster_info)
            
        return monsters

# ==========================================
# 실행 예시
# ==========================================
if __name__ == "__main__":
    # monsters.mon 파일 경로 지정
    FILE_PATH = "D:/workspace/Pool of Radiance/data/monsters.mon" 
    
    parsed_data = parse_monster_file(FILE_PATH)
    
    if parsed_data:
        # 상위 3마리의 정보만 샘플로 출력
        for mon in parsed_data[:3]:
            print(f"[Monster #{mon['index']}]")
            print(f"  - 기본 스탯 블록(Hex): {mon['raw_stats_block'].hex().upper()[:30]}...")
            print(f"  - 가변 데이터 레코드 개수: {mon['extra_records_count']}")
            if mon['extra_data_chunks']:
                print(f"  - 가변 데이터(Hex): {[chunk.hex().upper() for chunk in mon['extra_data_chunks']]}")
            print("-" * 30)
