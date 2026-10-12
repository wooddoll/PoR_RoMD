import math
import os
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
        w = x1 - x0 + 1
        h = y1 - y0 + 1
        
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
            
            c['new_bbox'] = (dx0, dy0, dx1-1, dy1-1)
            # 새 좌표 파일 쓰기
            out_f.write(f"{c['code']} {dx0} {dy0} {dx1-1} {dy1-1}\n")
    
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

# --- 실행 가이드 ---
if __name__ == "__main__":
    # 파일 경로를 본인의 환경에 맞게 수정하세요.
    merge_bitmap_fonts(
        img_a_path="E:/Games/Pool of Radiance/Data/Fonts_/font0.bmp",
        txt_a_path='font/font0.dff.txt',
        img_b_path='font/Umdot14.ttf_19.png',
        txt_b_path='font/Umdot14.ttf_19.txt',
        output_img_path='font/font_kr/font0.bmp',
        output_txt_path='font/font_kr/font0.dff.txt'
    )
    
    merge_bitmap_fonts(
        img_a_path="E:/Games/Pool of Radiance/Data/Fonts_/font1.bmp",
        txt_a_path='font/font1.dff.txt',
        img_b_path='font/Umdot12.ttf_17.png',
        txt_b_path='font/Umdot12.ttf_17.txt',
        output_img_path='font/font_kr/font1.bmp',
        output_txt_path='font/font_kr/font1.dff.txt'
    )
    
    merge_bitmap_fonts(
        img_a_path="E:/Games/Pool of Radiance/Data/Fonts_/font2.bmp",
        txt_a_path='font/font2.dff.txt',
        img_b_path='font/Umdot20-Bold.ttf_29.png',
        txt_b_path='font/Umdot20-Bold.ttf_29.txt',
        output_img_path='font/font_kr/font2.bmp',
        output_txt_path='font/font_kr/font2.dff.txt'
    )
    
    merge_bitmap_fonts(
        img_a_path="E:/Games/Pool of Radiance/Data/Fonts_/font3.bmp",
        txt_a_path='font/font3.dff.txt',
        img_b_path='font/Galmuri9.ttf_14.png',
        txt_b_path='font/Galmuri9.ttf_14.txt',
        output_img_path='font/font_kr/font3.bmp',
        output_txt_path='font/font_kr/font3.dff.txt'
    )
    
    merge_bitmap_fonts(
        img_a_path="E:/Games/Pool of Radiance/Data/Fonts_/font4.bmp",
        txt_a_path='font/font4.dff.txt',
        img_b_path='font/Galmuri11.ttf_16.png',
        txt_b_path='font/Galmuri11.ttf_16.txt',
        output_img_path='font/font_kr/font4.bmp',
        output_txt_path='font/font_kr/font4.dff.txt'
    )
