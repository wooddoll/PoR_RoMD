import struct
import os

def parse_tee_trs(file_path):
    if not os.path.exists(file_path):
        print(f"[오류] 파일이 존재하지 않습니다: {file_path}")
        return None

    with open(file_path, 'rb') as f:
        print(f"=== '{file_path}' 파일 분석 시작 ===")
        
        # 1. 헤더 분석 (버전 및 더미 데이터)
        # i: 4바이트 정수 (int/uint32)
        version_data = f.read(4)
        if len(version_data) < 4:
            print("[오류] 파일 크기가 너무 작아 헤더를 읽을 수 없습니다.")
            return None
            
        version = struct.unpack('<I', version_data)[0]
        print(f"파일 버전: 0x{version:02X} ({version})")
        
        # 기드라 코드상 버전은 반드시 0x68 (104)여야 함
        if version != 0x68:
            print(f"[경고] 유효하지 않은 버전입니다. expected: 0x68, got: 0x{version:02X}")
        
        # 더미 4바이트 건너뛰기/읽기
        dummy = struct.unpack('<I', f.read(4))[0]
        
        # 2. 블록 1 읽기
        block1_count = struct.unpack('<I', f.read(4))[0]
        block1_item_size = 0x1E  # 30바이트
        print(f"블록 1 아이템 개수: {block1_count} 개 (각 {block1_item_size}바이트)")
        
        block1_data = []
        for i in range(block1_count):
            item_bytes = f.read(block1_item_size)
            # 내부 구조를 모를 때는 일단 16진수 문자열이나 바이너리 그대로 보관합니다.
            block1_data.append({
                'index': i,
                'raw_hex': item_bytes.hex()
            })
            
        # 3. 블록 2 읽기
        block2_count = struct.unpack('<I', f.read(4))[0]
        block2_item_size = 0x48  # 72바이트
        print(f"블록 2 아이템 개수: {block2_count} 개 (각 {block2_item_size}바이트)")
        
        block2_data = []
        for i in range(block2_count):
            item_bytes = f.read(block2_item_size)
            block2_data.append({
                'index': i,
                'raw_hex': item_bytes.hex()
            })
            
        # 4. 블록 3 읽기
        block3_count = struct.unpack('<I', f.read(4))[0]
        block3_item_size = 0x32  # 50바이트
        print(f"블록 3 아이템 개수: {block3_count} 개 (각 {block3_item_size}바이트)")
        
        block3_data = []
        for i in range(block3_count):
            item_bytes = f.read(block3_item_size)
            block3_data.append({
                'index': i,
                'raw_hex': item_bytes.hex()
            })
            
        # 남은 데이터가 있는지 확인 (정상적인 파일이라면 딱 맞게 끝나야 함)
        remaining = f.read()
        if remaining:
            print(f"[주의] 파일 끝에 {len(remaining)} 바이트의 남은 데이터가 존재합니다.")
            
        return {
            'version': version,
            'block1': block1_data,
            'block2': block2_data,
            'block3': block3_data
        }

# --- 사용 예시 ---
if __name__ == "__main__":
    file_name = "D:/workspace/Pool of Radiance/data/TEE.trs"  # 파일 경로를 입력하세요
    parsed_result = parse_tee_trs(file_name)
    
    if parsed_result:
        print("\n=== 파싱 완료 샘플 출력 ===")
        # 예시로 블록 1의 첫 3개 아이템만 출력해보기
        print("블록 1 (첫 3개):")
        for item in parsed_result['block1'][:3]:
            print(f"  아이템 [{item['index']}]: {item['raw_hex']}")

