import os
import sys

def compare_folders(folder1, folder2):
    """
    두 폴더 내의 텍스트 파일들을 비교합니다.
    줄바꿈 직전의 tab(\\t)을 제외한 모든 내용(캐리지 리턴 포함)이 일치하는지 확인합니다.
    """
    # 1. 두 폴더의 파일 목록 가져오기
    files1 = set(os.listdir(folder1))
    files2 = set(os.listdir(folder2))
    
    # 폴더 구조(파일 목록) 자체가 다른 경우 처리
    if files1 != files2:
        print("⚠️ 두 폴더의 파일 구성이 일치하지 않습니다.")
        only_in_1 = files1 - files2
        only_in_2 = files2 - files1
        if only_in_1: print(f" - {folder1}에만 있는 파일: {only_in_1}")
        if only_in_2: print(f" - {folder2}에만 있는 파일: {only_in_2}")


    all_match = True

    # 2. 파일 내용 비교
    for filename in sorted(files1):
        if filename in only_in_1:
            continue

        file1_path = os.path.join(folder1, filename)
        file2_path = os.path.join(folder2, filename)
        
        # 디렉토리는 건너뛰고 파일만 비교
        if os.path.isdir(file1_path) or os.path.isdir(file2_path):
            continue
            
        # binary 모드('rb')로 읽어야 \\r\\n과 \\n의 차이(캐리지 리턴)를 왜곡 없이 그대로 보존합니다.
        with open(file1_path, 'rb') as f1, open(file2_path, 'rb') as f2:
            lines1 = f1.readlines()
            lines2 = f2.readlines()
            
            if len(lines1) != (lines2_len := len(lines2)):
                print(f"❌ 불일치: '{filename}' (줄 수가 다릅니다. {len(lines1)} lines vs {lines2_len} lines)")
                all_match = False
                continue
                
            # 각 줄을 비교하면서 줄바꿈(#13, #10) 직전의 탭(#9)만 제거
            file_match = True
            for line_idx, (l1, l2) in enumerate(zip(lines1, lines2), start=1):
                
                # 줄바꿈 기호(\r\n 또는 \n)와 탭(\t)을 분리하여 처리하기 위해 rstrip 사용
                # l1.endswith(b'\r\n') 등을 유지하면서 오직 줄바꿈 직전의 \t만 지우는 로직입니다.
                clean_l1 = bytes_rstrip_tab_before_newline(l1)
                clean_l2 = bytes_rstrip_tab_before_newline(l2)
                
                if clean_l1 != clean_l2:
                    print(f"❌ 불일치: '{filename}'의 {line_idx}번째 줄 내용이 다릅니다.")
                    file_match = False
                    all_match = False
                    break
            
            #if file_match:
            #    print(f"✅ 일치: '{filename}'")

    return all_match

def bytes_rstrip_tab_before_newline(line_bytes):
    """
    바이트 스트링에서 줄바꿈 기호(\\r, \\n) 바로 앞에 붙은 탭(\\t)만 제거합니다.
    """
    # 줄바꿈 기호가 어떻게 끝나는지 확인 (\r\n, \n, \r 또는 없음)
    newline = b''
    if line_bytes.endswith(b'\r\n'):
        newline = b'\r\n'
    elif line_bytes.endswith(b'\n'):
        newline = b'\n'
    elif line_bytes.endswith(b'\r'):
        newline = b'\r'
        
    # 줄바꿈 기호를 제외한 실제 본문 추출
    content = line_bytes[:-len(newline)] if newline else line_bytes
    
    # 본문 맨 우측의 탭(\t)만 제거한 뒤 다시 줄바꿈 기호와 결합
    return content.rstrip(b'\t') + newline

# --- 사용 예시 ---
# folder_a = "./dir_a"
# folder_b = "./dir_b"
# compare_folders(folder_a, folder_b)

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("Usage: python compare.py <.por_file_path1> <.por_file_path2>")
        sys.exit(1)

    matches = compare_folders(sys.argv[1], sys.argv[2])