import struct
import png

# ----------------------------------------
# 1. 이미지 로더 (BMP / PNG)
# ----------------------------------------

def read_bmp(file_path):
    """24비트 BMP를 읽어 (가로, 세로, 2차원 RGB 픽셀 리스트) 형태로 리턴합니다."""
    with open(file_path, 'rb') as f:
        header = f.read(54)
        if header[:2] != b'BM':
            raise ValueError("올바른 BMP 파일이 아닙니다.")
        
        width, height = struct.unpack('<ii', header[18:26])
        pixel_offset = struct.unpack('<i', header[10:14])[0]
        
        # BMP 줄바꿈 패딩 계산
        row_padded_width = (width * 3 + 3) & ~3
        padding_size = row_padded_width - (width * 3)
        
        f.seek(pixel_offset)
        raw_rows = []
        for _ in range(height):
            row_data = f.read(width * 3)
            f.read(padding_size)
            raw_rows.append(row_data)
            
        # BMP 파일 구조(하단 시작)를 일반 좌표계(상단 시작)로 뒤집기
        raw_rows.reverse()
        
        # 바이트 데이터를 표준 [R, G, B, R, G, B...] 형태로 변환
        pixel_matrix = []
        for row in raw_rows:
            rgb_row = []
            for i in range(0, len(row), 3):
                b, g, r = row[i:i+3]  # BMP는 BGR 순서
                rgb_row.extend([r, g, b])
            pixel_matrix.append(rgb_row)
            
        return width, height, pixel_matrix


def read_png(file_path):
    """PNG를 읽어 (가로, 세로, 2차원 RGB 픽셀 리스트) 형태로 리턴합니다."""
    with open(file_path, 'rb') as f:
        reader = png.Reader(file=f)
        width, height, rows, info = reader.read()
        
        # 알파 채널 여부 확인 (RGB=3, RGBA=4)
        channels = 4 if info['alpha'] else 3
        pixel_matrix = []
        
        for row in rows:
            rgb_row = []
            for i in range(0, len(row), channels):
                # 알파 채널이 있더라도 RGB 3채널만 추출하여 통일
                r, g, b = row[i:i+3]
                rgb_row.extend([r, g, b])
            pixel_matrix.append(rgb_row)
            
        return width, height, pixel_matrix

def read_image(file_path: str):
    if file_path.lower().endswith('.bmp'):
        return read_bmp(file_path)
    if file_path.lower().endswith('.png'):
        return read_png(file_path)
    else:
        print('unsupported image format, use .bmp and .png')


# ----------------------------------------
# 2. 이미지 병합 및 배치 (Blit) 헬퍼
# ----------------------------------------

def blit_region(dest_canvas, src_canvas, dest_x, dest_y, src_x, src_y, w, h):
    """
    도화지 행의 길이(3072)를 절대 변화시키지 않고, 
    정확히 지정한 픽셀 수만큼만 안전하게 덮어쓰는 개선된 Blit 함수
    """
    for dy in range(h):
        curr_dest_y = dest_y + dy
        curr_src_y = src_y + dy
        
        # 도화지나 소스 이미지의 세로 경계를 벗어나면 안전하게 스킵
        if curr_dest_y >= len(dest_canvas) or curr_src_y >= len(src_canvas):
            continue
            
        # 픽셀당 3바이트(RGB) 단위 오프셋 계산
        src_start = src_x * 3
        src_end = (src_x + w) * 3
        dest_start = dest_x * 3
        dest_end = (dest_x + w) * 3
        
        # [핵심 수정] 복사해올 소스 조각 데이터 추출
        patch_data = src_canvas[curr_src_y][src_start:src_end]
        
        # 잘라온 소스 조각이 실제 요청한 크기(w * 3)와 맞는지 한 번 더 검증
        expected_bytes = w * 3
        if len(patch_data) < expected_bytes:
            patch_data = patch_data + [0] * (expected_bytes - len(patch_data))
        elif len(patch_data) > expected_bytes:
            patch_data = patch_data[:expected_bytes]
            
        # 도화지의 가로 범위를 벗어나지 않도록 대입할 최종 크기 조절
        if dest_start + len(patch_data) > len(dest_canvas[curr_dest_y]):
            # 도화지 오른쪽 경계를 넘어가는 경우 잘라냄
            allowed_bytes = len(dest_canvas[curr_dest_y]) - dest_start
            patch_data = patch_data[:allowed_bytes]
            
        # 최종 대입 (슬라이싱 범위를 덮어쓸 데이터 크기와 정확히 일치시킴)
        actual_dest_end = dest_start + len(patch_data)
        dest_canvas[curr_dest_y][dest_start:actual_dest_end] = patch_data

# ----------------------------------------
# 3. 최종 대형 BMP 파일 저장
# ----------------------------------------

def save_as_bmp(file_path, width, height, pixel_matrix):
    """2차원 RGB 픽셀 리스트를 24비트 BMP 파일로 저장합니다."""
    row_padded_width = (width * 3 + 3) & ~3
    padding_size = row_padded_width - (width * 3)
    pixel_data_size = row_padded_width * height
    file_size = 54 + pixel_data_size

    # 24비트 표준 BMP 헤더 패킹
    bmp_header = struct.pack(
        '<2sIHHIIiiHHIIiiII',
        b'BM', file_size, 0, 0, 54, 
        40, width, height, 1, 24, 0, pixel_data_size,
        2835, 2835, 0, 0
    )

    # BMP 표준에 맞추기 위해 상하 반전 후 파일 쓰기
    output_rows = list(pixel_matrix)
    output_rows.reverse()

    with open(file_path, 'wb') as f_out:
        f_out.write(bmp_header)
        for row in output_rows:
            bmp_row = bytearray()
            # RGB -> BGR 순서로 변환
            for i in range(0, len(row), 3):
                r, g, b = row[i:i+3]
                bmp_row.extend([b, g, r])
            # 4바이트 정렬을 위한 패딩 추가
            bmp_row.extend(b'\x00' * padding_size)
            f_out.write(bmp_row)


# ----------------------------------------
# 4. 실전 활용 시나리오 메인 함수
# ----------------------------------------

def assemble_font_sheet():
    # ① 대형 도화지 크기 지정 (예: 가로 2000, 세로 2000 검은색 도화지)
    canvas_w, canvas_h = 2000, 2000
    # 모든 픽셀을 검은색(R=0, G=0, B=0)으로 초기화한 2차원 배열 생성
    canvas = [[0] * (canvas_w * 3) for _ in range(canvas_h)]
    
    print("1. 소스 이미지 로드 중...")
    # ② 기존 BMP 및 PNG 폰트 시트 파일 읽기
    # 각각 가로, 세로, 픽셀 매트릭스를 반환받음
    bg_bmp_w, bg_bmp_h, bg_bmp_pixels = read_bmp('background.bmp')
    font_png_w, font_png_h, font_png_pixels = read_png('font_sheet.png')
    
    print("2. 글자 영역 복사 및 큰 BMP에 배치 중...")
    
    # 예시 1) 기존 BMP 이미지 전체를 대형 도화지의 (0, 0) 위치에 깔기
    blit_region(canvas, bg_bmp_pixels, dest_x=0, dest_y=0, src_x=0, src_y=0, w=bg_bmp_w, h=bg_bmp_h)
    
    # 예시 2) PNG 폰트 시트에서 특정 글자(예: 가로 32x세로 32 규격) 조각들을 잘라서 원하는 위치에 배치
    # 폰트 시트의 (64, 32) 영역에 있는 글자를 도화지의 (100, 500) 위치로 복사
    blit_region(canvas, font_png_pixels, dest_x=100, dest_y=500, src_x=64, src_y=32, w=32, h=32)
    
    # 폰트 시트의 (96, 32) 영역에 있는 다음 글자를 도화지의 (132, 500) 위치로 복사
    blit_region(canvas, font_png_pixels, dest_x=132, dest_y=500, src_x=96, src_y=32, w=32, h=32)
    
    print("3. 대형 완성본 BMP 파일 저장 중...")
    # ③ 최종 대형 BMP 파일로 출력
    save_as_bmp('large_font_master.bmp', canvas_w, canvas_h, canvas)
    print("완료되었습니다! 'large_font_master.bmp' 확인해 보세요.")

if __name__ == '__main__':
    assemble_font_sheet()
