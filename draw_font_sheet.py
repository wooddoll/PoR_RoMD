import os
import sys
from PIL import Image, ImageFont, ImageDraw


def load_char_list(file_path, encoding="euc-kr"):
    """CP949 인코딩 텍스트 파일에서 문자 목록을 읽어옵니다."""
    with open(file_path, "r", encoding=encoding) as f:
        content = f.read()
    # 중복 제거 및 순서 유지
    unique_chars = []
    for c in content:
        if c not in unique_chars:
            unique_chars.append(c)
    return unique_chars


def load_font(font_path, font_size):
    """TTF 폰트를 로드합니다. 실패 시 None 반환."""
    try:
        return ImageFont.truetype(font_path, font_size)
    except IOError:
        print(f"[오류] 폰트 파일을 찾을 수 없거나 불러올 수 없습니다: {font_path}")
        return None


def measure_chars(font, char_list, fixed_height, baseline_below, padding_left, padding_right):
    """각 문자의 가로 슬롯 크기를 측정합니다."""
    baseline_above = fixed_height - baseline_below
    char_data = []

    for c in char_list:
        # 가변폭(Advance Width) 계산
        advance_w = int(font.getlength(c))

        # 실제 픽셀이 찍히는 영역 계산
        bbox = font.getbbox(c)
        ink_w = advance_w if bbox is None else bbox[2] - bbox[0]

        # 가변폭과 실제 픽셀폭 중 큰 값에 좌우 패딩을 더해 가로 슬롯 크기 결정
        actual_w = max(advance_w, ink_w)
        slot_w = padding_left + actual_w + padding_right

        char_data.append({
            "char": c,
            "slot_w": slot_w,
            "slot_h": fixed_height,
        })

    return char_data, baseline_above


def pack_chars(char_data, fixed_height, max_atlas_width, gap=1):
    """가로줄 단위 패킹 알고리즘으로 문자들을 아틀라스에 배치합니다.
    각 슬롯 사이에 gap(기본 1px) 간격을 둡니다."""
    current_x = 0
    current_y = 0
    packed_chars = []

    for data in char_data:
        # 최대 가로폭을 넘어가면 개행 (간격 포함)
        if current_x + data["slot_w"] > max_atlas_width:
            current_x = 0
            current_y += fixed_height + gap

        data["x"] = current_x
        data["y"] = current_y
        packed_chars.append(data)

        current_x += data["slot_w"] + gap

    atlas_width = max_atlas_width if current_y > 0 else current_x
    atlas_height = current_y + fixed_height if current_x > 0 else current_y

    return packed_chars, atlas_width, atlas_height


def render_atlas(font, packed_chars, atlas_width, atlas_height,
                 baseline_above, padding_left, bg_color, fg_color,
                 draw_boxes=False):
    """문자들을 아틀라스 이미지에 렌더링하고 좌표 메타데이터를 생성합니다."""
    atlas_img = Image.new("RGBA", (atlas_width, atlas_height), bg_color)
    draw = ImageDraw.Draw(atlas_img)
    draw.fontmode = "1"  # 안티앨리어싱 제거

    # 폰트 자체의 내장 메트릭(ascent)을 가져와 오프셋 보정 계산
    try:
        ascent, _ = font.getmetrics()
    except AttributeError:
        ascent = font.size  # 하위 버전 호환용

    # 박스 그리기 옵션: 반투명 핑크색 overlay 생성
    if draw_boxes:
        overlay = Image.new("RGBA", atlas_img.size, (0, 0, 0, 0))
        box_draw = ImageDraw.Draw(overlay)

    txt_lines = []

    for data in packed_chars:
        c = data["char"]

        # 슬롯 내부에서의 렌더링 시작 좌표 계산
        draw_x = data["x"] + padding_left
        # Y축: 지정한 baseline_above 위치에 폰트의 실제 baseline이 정렬되도록 오프셋 가감
        draw_y = data["y"] + baseline_above - ascent

        # 글자 그리기
        draw.text((draw_x, draw_y), c, fill=fg_color, font=font)

        # 좌표 데이터 계산 (x0, y0, x1, y1)
        x0 = data["x"]
        y0 = data["y"]
        x1 = data["x"] + data["slot_w"] - 1
        y1 = data["y"] + data["slot_h"] - 1

        # 박스 그리기 옵션: 슬롯 경계에 반투명 핑크색 사각형 테두리
        if draw_boxes:
            pink = (255, 105, 180, 128)
            box_draw.rectangle([x0, y0, x1, y1], outline=pink, width=1)

        # CP949 코드와 좌표를 탭으로 구분해 저장
        cp949char = c.encode("cp949")
        cp949code = int.from_bytes(cp949char, byteorder="big")
        txt_lines.append(f"{cp949code}\t{x0}\t{y0}\t{x1+1}\t{y1+1}")

    # 반투명 박스 overlay를 원본 이미지에 합성
    if draw_boxes:
        atlas_img = Image.alpha_composite(atlas_img, overlay)

    return atlas_img, txt_lines


def ensure_parent_dir(file_path):
    """파일이 위치할 부모 폴더가 없으면 생성합니다."""
    folder_path = os.path.dirname(file_path)
    if folder_path and not os.path.exists(folder_path):
        os.makedirs(folder_path, exist_ok=True)


def save_outputs(atlas_img, txt_lines, output_image_path, output_txt_path):
    """이미지와 텍스트 메타데이터 파일을 저장합니다."""
    ensure_parent_dir(output_image_path)
    atlas_img.save(output_image_path)

    ensure_parent_dir(output_txt_path)
    with open(output_txt_path, "w", encoding="utf-8") as f:
        f.write("\n".join(txt_lines))


def generate_custom_bitmap_font(font_path, font_size, char_list, output_image_path, output_txt_path,
                                padding_left=0, padding_right=1,
                                fixed_height=16, baseline_below=4,
                                bg_color=(0, 0, 0, 255), fg_color=(255, 255, 255, 255), max_atlas_width=2048,
                                draw_boxes=False, gap=1):
    """
    TTF 폰트를 받아 안티앨리어싱이 없는 단색 비트맵 폰트를 생성합니다.
    세로 크기는 (baseline_above + baseline_below)로 고정되며, 가로 크기는 가변폭으로 대응합니다.
    결과물로 이미지 파일과 단순 좌표 텍스트 파일을 생성합니다.
    draw_boxes=True 시 각 슬롯 경계에 반투명 핑크색 박스를 그립니다.
    gap: 슬롯 사이 간격 (기본 1px)
    """
    font = load_font(font_path, font_size)
    if font is None:
        return

    char_data, baseline_above = measure_chars(
        font, char_list, fixed_height, baseline_below, padding_left, padding_right
    )

    packed_chars, atlas_width, atlas_height = pack_chars(
        char_data, fixed_height, max_atlas_width, gap=gap
    )

    atlas_img, txt_lines = render_atlas(
        font, packed_chars, atlas_width, atlas_height,
        baseline_above, padding_left, bg_color, fg_color,
        draw_boxes=draw_boxes
    )

    save_outputs(atlas_img, txt_lines, output_image_path, output_txt_path)

    print("비트맵 폰트 생성 완료!")
    print(f" - 이미지: {output_image_path} ({atlas_width}x{atlas_height})")
    print(f" - 텍스트 메타데이터: {output_txt_path}")
    if draw_boxes:
        print(" - 박스 그리기: 활성화")


# --- 사용 예시 ---
def generate_all_fonts(char_list_file="./cp949_kor2350.txt", output_dir="./font", draw_boxes=False, gap=1):
    """모든 한글 폰트를 일괄 생성합니다."""
    # 한글 완성형 2,350자 (CP949 인코딩 파일에서 로드)
    hangul_2350 = load_char_list(char_list_file)

    # 폰트 설정: [경로, 폰트 크기, 고정 높이, baseline 아래 여백]
    FONTS = {
        "font0": ["../font/Umdot-ttf/Proportional/Regular/Umdot14.ttf", 13, 18, 3],
        "font1": ["../font/Umdot-ttf/Proportional/Regular/Umdot12.ttf", 12, 16, 2],
        "font2": ["../font/Umdot-ttf/Proportional/Bold/Umdot20-Bold.ttf", 20, 28, 5],
 
        "font3": ["../font/Galmuri-v2.40.4/Galmuri9.ttf", 10, 13, 2],
        "font4": ["../font/Galmuri-v2.40.4/Galmuri11.ttf", 12, 15, 2],
    }

    # 좌우 여백 설정 (세로는 baseline 옵션으로 고정되므로 좌우만 지정)
    PAD_LEFT = 0
    PAD_RIGHT = 0

    BACKGROUND_COLOR = (0, 0, 0, 255)    # 검은색 배경
    FONT_COLOR = (255, 255, 255, 255)    # 흰색 글씨

    # 출력 디렉토리 생성
    if output_dir and not os.path.exists(output_dir):
        os.makedirs(output_dir, exist_ok=True)

    success_count = 0
    for font_key, (font_file, font_size, fixed_height, baseline_below) in FONTS.items():
        print(f"\n=== Generating {font_key} ===")
        font_name = os.path.basename(font_file)
        out_name = f"{font_name}_{fixed_height}"

        try:
            generate_custom_bitmap_font(
                font_path=font_file,
                font_size=font_size,
                char_list=hangul_2350,
                output_image_path=f"{output_dir}/{out_name}.png",
                output_txt_path=f"{output_dir}/{out_name}.txt",
                padding_left=PAD_LEFT,
                padding_right=PAD_RIGHT,
                fixed_height=fixed_height,
                baseline_below=baseline_below,
                bg_color=BACKGROUND_COLOR,
                fg_color=FONT_COLOR,
                max_atlas_width=1024,
                draw_boxes=draw_boxes,
                gap=gap
            )
            success_count += 1
        except Exception as e:
            print(f"Error generating {font_key}: {e}")

    print(f"\n=== Batch font generation complete: {success_count}/{len(FONTS)} fonts processed ===")
    return success_count


def print_usage():
    """사용법을 출력합니다."""
    print("Usage:")
    print("  python draw_font_sheet.py                          # 모든 폰트 일괄 생성 (기본 동작)")
    print("  python draw_font_sheet.py --draw-boxes             # 각 슬롯에 반투명 핑크색 박스 그리기")
    print("  python draw_font_sheet.py --help                   # 도움말 표시")


if __name__ == "__main__":
    args = sys.argv[1:]

    if not args:
        generate_all_fonts()
    elif args[0] == '--help' or args[0] == '-h':
        print_usage()
    elif args[0] == '--draw-boxes':
        generate_all_fonts(draw_boxes=True)
    else:
        print(f"Error: Unknown option: {args[0]}")
        print_usage()
