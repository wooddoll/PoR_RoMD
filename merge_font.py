import math
import os
import random
import sys
from PIL import Image
import struct

def load_font_meta(file_path):
    """텍스트 파일에서 문자 코드 및 좌표 정보를 읽어옵니다."""
    meta_list = []
    if not os.path.exists(file_path):
        return meta_list
    
    with open(file_path, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            # 공백, 쉼표, 탭 등 다양한 구분자 대응
            parts = line.replace(',', ' ').split()
            if len(parts) >= 5:
                char_code = int(parts[0])
                coords = [int(p) for p in parts[1:5]] # x0, y0, x1, y1
                meta_list.append([char_code] + coords)
    return meta_list

def merge_bitmap_fonts(img_a_path, txt_a_path, img_b_path, txt_b_path, output_img_path, output_txt_path):
    # 1. 메타데이터 및 이미지 로드
    meta_a = load_font_meta(txt_a_path)
    meta_b = load_font_meta(txt_b_path)
    
    img_a = Image.open(img_a_path).convert('RGB')
    img_b = Image.open(img_b_path).convert('RGB')
    
    # A, B 순서대로 병합 (문자코드 중복 없음)
    combined_meta = meta_a + meta_b
    
    codeIdxPair = []
    for idx, meta in enumerate(combined_meta):
        codeIdxPair.append((meta[0], idx))
    sorted_pair = sorted(codeIdxPair, key=lambda x: x[0], reverse=True)
    
    # 2. 개별 문자 정보 정리 및 총 면적 계산
    chars_info = []
    total_area = 0
    font_height = None
    
    for item in combined_meta:
        char_code, x0, y0, x1, y1 = item
        w = x1 - x0
        h = y1 - y0
        
        if font_height is None:
            font_height = h
            
        # 소스 이미지 선택
        # 여기서는 안전하게 문자 인덱스로 소스를 구분합니다 (A: 앞쪽 len(meta_a)개, B: 나머지)
        is_a = len(chars_info) < len(meta_a)
        src_img = img_a if is_a else img_b
        
        chars_info.append({
            'code': char_code,
            'w': w,
            'h': h,
            'src_img': src_img,
            'box': (x0, y0, x1, y1)
        })
        total_area += w * font_height

    if not chars_info:
        print("병합할 문자 데이터가 없습니다.")
        return

    # 3. 최적의 캔버스 가로 크기(2의 지수승) 계산
    # 가로세로가 유사하면서 세로가 더 길도록 타겟 가로 비율 설정 (예: 가로 0.8 : 세로 1.2 정도 비율 타겟)
    # 가로 크기 W ≒ sqrt(total_area * 0.8)
    target_width = math.sqrt(total_area * 0.8)
    
    # 2의 거듭제곱으로 올림 (최소 크기는 가장 넓은 글자 너비 이상이어야 함)
    max_char_width = max(c['w'] for c in chars_info)
    pot_width = 2 ** math.ceil(math.log2(max(target_width, max_char_width)))
    
    # 4. 배치 레이아웃 계산 (줄바꿈 처리)
    current_x = 0
    current_y = 0
    
    for c in chars_info:
        # 가로 크기를 넘어가면 다음 줄로 이동
        if current_x + c['w'] > pot_width:
            current_x = 0
            current_y += font_height
            
        c['dest_x'] = current_x
        c['dest_y'] = current_y
        current_x += c['w']
        
    # 최종 필요한 세로 크기 계산
    canvas_height = current_y + font_height

    print(f"최종 이미지 크기: 가로 {pot_width} x 세로 {canvas_height}")
    
    # 5. 새 투명 이미지 생성 및 폰트 굽기(Blit), 텍스트 저장
    new_image = Image.new('RGB', (pot_width, canvas_height), (0, 0, 0, 0))
    
    with open(output_txt_path, 'w', encoding='utf-8') as out_f:
        for c in chars_info:
            # 폰트 이미지 크롭 후 새 이미지에 붙여넣기
            char_crop = c['src_img'].crop(c['box'])
            dx0, dy0 = c['dest_x'], c['dest_y']
            dx1, dy1 = dx0 + c['w'], dy0 + c['h']
            
            new_image.paste(char_crop, (dx0, dy0))
            
            c['new_bbox'] = (dx0, dy0, dx1, dy1)
            # 새 좌표 파일 쓰기
            out_f.write(f"{c['code']} {dx0} {dy0} {dx1} {dy1}\n")
    
    # 6. 리틀 엔디언 4바이트 정수로 바이너리 파일 작성
    # '<i' 포맷 명세서: < (리틀엔디언), i (4바이트 정수)
    out_path, _ = os.path.splitext(output_img_path)
    dff_NAME = f"{out_path}.dff" 
    with open(dff_NAME, 'wb') as out_b:
        # 헤더: [폰트 갯수, 이미지 가로 크기, 이미지 세로 크기]
        font_count = len(chars_info)
        out_b.write(struct.pack('<iii', font_count, pot_width, canvas_height))
        
        # 데이터 1: [code, x0, y0, x1, y1] 순서대로 기록
        for c in chars_info:
            nx0, ny0, nx1, ny1 = c['new_bbox']
            out_b.write(struct.pack('<iiiii', c['code'], nx0, ny0, nx1, ny1))
            
        # 데이터 2: [코드 갯수] 작성 후 [code, 0-base index] 순서대로 기록
        out_b.write(struct.pack('<i', font_count))
        for c, idx in sorted_pair:
            out_b.write(struct.pack('<ii', c, idx))
    
    # 이미지 저장
    new_image.save(output_img_path)
    print(f"성공적으로 병합되었습니다! -> {output_img_path}, {output_txt_path}")

def generate_random_mixed_sheet(img_a_path, txt_a_path, img_b_path, txt_b_path,
                                output_img_path, output_txt_path, grid_size=64):
    """
    img_a(ASCII 영역)의 알파벳 대소문자와 img_b(한글 영역)의 글자를
    랜덤하게 섞어 grid_size x grid_size (기본 64x64) 글자가 한 장에 들어가도록
    이미지와 TXT 메타데이터를 생성합니다.

    각 글자는 자신의 RECT(바운딩 박스) 크기대로 좌측 정렬로 연속 배치됩니다.
    영문자는 좁은 폭으로, 한글은 넓은 폭으로 자연스럽게 연결됩니다.
    """
    # 1. 메타데이터 및 이미지 로드
    meta_a = load_font_meta(txt_a_path)
    meta_b = load_font_meta(txt_b_path)

    img_a = Image.open(img_a_path).convert('RGB')
    img_b = Image.open(img_b_path).convert('RGB')

    # 2. ASCII 알파벳 대소문자 (A-Z, a-z) 글리프 추출
    ascii_chars = []
    for meta in meta_a:
        char_code = meta[0]
        # ASCII 알파벳 대소문자: 'A'(65) ~ 'Z'(90), 'a'(97) ~ 'z'(122)
        if (65 <= char_code <= 90) or (97 <= char_code <= 122):
            x0, y0, x1, y1 = meta[1], meta[2], meta[3], meta[4]
            w = x1 - x0
            h = y1 - y0
            if w > 0 and h > 0:
                ascii_chars.append({
                    'code': char_code,
                    'w': w,
                    'h': h,
                    'src_img': img_a,
                    'box': (x0, y0, x1, y1)
                })

    # 3. 한글 글리프 추출 (CP949 코드가 0x8000 이상인 문자)
    korean_chars = []
    for meta in meta_b:
        char_code = meta[0]
        # CP949 한글 코드는 0x8000 이상 (한글 완성형 영역)
        if char_code >= 0x8000:
            x0, y0, x1, y1 = meta[1], meta[2], meta[3], meta[4]
            w = x1 - x0
            h = y1 - y0
            if w > 0 and h > 0:
                korean_chars.append({
                    'code': char_code,
                    'w': w,
                    'h': h,
                    'src_img': img_b,
                    'box': (x0, y0, x1, y1)
                })

    if not ascii_chars:
        print("ASCII 알파벳 글리프를 찾을 수 없습니다.")
        return
    if not korean_chars:
        print("한글 글리프를 찾을 수 없습니다.")
        return

    print(f"ASCII 알파벳 글리프: {len(ascii_chars)}개")
    print(f"한글 글리프: {len(korean_chars)}개")

    # 4. 랜덤하게 섞어서 grid_size x grid_size = 4096개 선택
    total_slots = grid_size * grid_size
    mixed_chars = []

    for i in range(total_slots):
        # ASCII와 한글을 랜덤하게 선택 (50:50 비율)
        if random.random() < 0.5:
            char = random.choice(ascii_chars)
        else:
            char = random.choice(korean_chars)
        mixed_chars.append(char)

    # 5. 각 글리프의 최대 높이 계산 (행 높이 결정)
    max_h = max(c['h'] for c in mixed_chars)

    # 행 높이: 최대 글리프 높이 + 1px 여백
    row_h = max_h + 1

    # 6. 각 행의 폭 계산 (해당 행의 글리프 폭 합 + 글자 간 1px 간격)
    row_widths = []
    for row in range(grid_size):
        row_chars = mixed_chars[row * grid_size:(row + 1) * grid_size]
        row_w = sum(c['w'] for c in row_chars) + (grid_size - 1)  # 글자 간 1px 간격
        row_widths.append(row_w)

    # 캔버스 크기: 가장 넓은 행 기준
    canvas_width = max(row_widths)
    canvas_height = row_h * grid_size

    print(f"그리드: {grid_size}x{grid_size} = {total_slots}개")
    print(f"행 높이: {row_h}px")
    print(f"캔버스 크기: {canvas_width}x{canvas_height}")

    # 7. 새 이미지 생성 및 글리프 배치 (각 글자를 자신의 RECT 크기대로 좌측 정렬 연속 배치)
    new_image = Image.new('RGB', (canvas_width, canvas_height), (0, 0, 0))

    with open(output_txt_path, 'w', encoding='utf-8') as out_f:
        for idx, char in enumerate(mixed_chars):
            row = idx // grid_size
            col = idx % grid_size

            # 행 내에서 좌측부터 연속 배치 (각 글자의 실제 폭만큼 이동)
            # 행 시작 x 좌표 계산: 이전 행들의 최대 폭 기준 중앙 정렬
            row_start_x = (canvas_width - row_widths[row]) // 2

            # 현재 열까지의 누적 x 좌표
            dest_x = row_start_x
            for prev_col in range(col):
                dest_x += mixed_chars[row * grid_size + prev_col]['w'] + 1  # 1px 간격

            # y 좌표: 행의 상단 (각 글자의 높이에 맞게 배치)
            dest_y = row * row_h

            # 글리프 크롭 후 배치
            char_crop = char['src_img'].crop(char['box'])
            new_image.paste(char_crop, (dest_x, dest_y))

            # TXT 메타데이터 작성
            dx1 = dest_x + char['w']
            dy1 = dest_y + char['h']
            out_f.write(f"{char['code']} {dest_x} {dest_y} {dx1} {dy1}\n")

    # 8. 이미지 저장
    new_image.save(output_img_path)
    print(f"랜덤 혼합 시트 생성 완료! -> {output_img_path}")
    print(f"메타데이터: {output_txt_path}")


# --- 실행 가이드 ---
# 폰트별 설정 테이블
# 각 항목: (원본 폰트 이미지, 원본 폰트 TXT, 한글 폰트 PNG, 한글 폰트 TXT)
# 원본 폰트 이미지는 워크스페이스 루트의 fontX.png 또는 게임 설치 경로의 fontX.bmp 사용 가능
FONT_CONFIGS = [
    {
        "name": "font0",
        "img_a": "E:/Games/Pool of Radiance/data/fonts/font0.bmp",
        "txt_a": "font/font0.dff.txt",
        "img_b": "font/Umdot14.ttf_18.png",
        "txt_b": "font/Umdot14.ttf_18.txt",
    },
    {
        "name": "font1",
        "img_a": "E:/Games/Pool of Radiance/data/fonts/font1.bmp",
        "txt_a": "font/font1.dff.txt",
        "img_b": "font/Umdot12.ttf_16.png",
        "txt_b": "font/Umdot12.ttf_16.txt",
    },
    {
        "name": "font2",
        "img_a": "E:/Games/Pool of Radiance/data/fonts/font2.bmp",
        "txt_a": "font/font2.dff.txt",
        "img_b": "font/Umdot20-Bold.ttf_28.png",
        "txt_b": "font/Umdot20-Bold.ttf_28.txt",
    },
    {
        "name": "font3",
        "img_a": "E:/Games/Pool of Radiance/data/fonts/font3.bmp",
        "txt_a": "font/font3.dff.txt",
        "img_b": "font/Galmuri9.ttf_13.png",
        "txt_b": "font/Galmuri9.ttf_13.txt",
    },
    {
        "name": "font4",
        "img_a": "E:/Games/Pool of Radiance/data/fonts/font4.bmp",
        "txt_a": "font/font4.dff.txt",
        "img_b": "font/Galmuri11.ttf_15.png",
        "txt_b": "font/Galmuri11.ttf_15.txt",
    },
]


def process_all_fonts(configs=None, output_dir="font/font_kr"):
    """모든 폰트를 일괄 병합합니다."""
    if configs is None:
        configs = FONT_CONFIGS

    # 출력 디렉토리 생성
    if output_dir and not os.path.exists(output_dir):
        os.makedirs(output_dir, exist_ok=True)

    success_count = 0
    for cfg in configs:
        name = cfg["name"]
        output_img = os.path.join(output_dir, f"{name}.bmp")
        output_txt = os.path.join(output_dir, f"{name}.dff.txt")

        print(f"\n=== Processing {name} ===")
        try:
            merge_bitmap_fonts(
                img_a_path=cfg["img_a"],
                txt_a_path=cfg["txt_a"],
                img_b_path=cfg["img_b"],
                txt_b_path=cfg["txt_b"],
                output_img_path=output_img,
                output_txt_path=output_txt
            )
            success_count += 1
        except Exception as e:
            print(f"Error processing {name}: {e}")

    print(f"\n=== Batch merge complete: {success_count}/{len(configs)} fonts processed ===")
    return success_count


def print_usage():
    """사용법을 출력합니다."""
    print("Usage:")
    print("  python merge_font.py [--output-dir <path>]              # 모든 폰트 일괄 병합 (기본 동작)")
    print("  python merge_font.py --random-sheet <font_index>        # 랜덤 혼합 시트 생성 (0-4)")
    print("  python merge_font.py --random-sheet <font_index> --grid <N>  # 그리드 크기 지정 (기본 64)")
    print("  python merge_font.py --help                             # 도움말 표시")


if __name__ == "__main__":
    # 명령줄 인자 처리
    output_dir = "font/font_kr"
    random_sheet_index = None
    grid_size = 64

    args = sys.argv[1:]
    i = 0
    while i < len(args):
        if args[i] in ("--output-dir", "-o") and i + 1 < len(args):
            output_dir = args[i + 1]
            i += 2
        elif args[i] == "--random-sheet" and i + 1 < len(args):
            random_sheet_index = int(args[i + 1])
            i += 2
        elif args[i] == "--grid" and i + 1 < len(args):
            grid_size = int(args[i + 1])
            i += 2
        elif args[i] in ("--help", "-h"):
            print_usage()
            sys.exit(0)
        else:
            print(f"Unknown argument: {args[i]}")
            print_usage()
            sys.exit(1)

    if random_sheet_index is not None:
        # 랜덤 혼합 시트 생성
        if 0 <= random_sheet_index < len(FONT_CONFIGS):
            cfg = FONT_CONFIGS[random_sheet_index]
            name = cfg["name"]
            output_img = os.path.join(output_dir, f"{name}_random_sheet.png")
            output_txt = os.path.join(output_dir, f"{name}_random_sheet.txt")

            print(f"\n=== Generating random mixed sheet for {name} ===")
            generate_random_mixed_sheet(
                img_a_path=cfg["img_a"],
                txt_a_path=cfg["txt_a"],
                img_b_path=cfg["img_b"],
                txt_b_path=cfg["txt_b"],
                output_img_path=output_img,
                output_txt_path=output_txt,
                grid_size=grid_size
            )
        else:
            print(f"Error: font index must be 0-4, got {random_sheet_index}")
            sys.exit(1)
    else:
        process_all_fonts(output_dir=output_dir)
