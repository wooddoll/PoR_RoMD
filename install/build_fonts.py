#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
build_fonts.py - Pool of Radiance Korean font batch generation script

Usage:
    python build_fonts.py <Pool of Radiance path>

Input:
    <Pool of Radiance path> - Game installation path (e.g., ../Pool of Radiance)

Output:
    <Pool of Radiance path>/data/font_kr/font0.bmp ~ font4.bmp
    <Pool of Radiance path>/data/font_kr/font0.dff ~ font4.dff
    (TXT files are not generated)
"""

import math
import os
import struct
import sys
from dataclasses import dataclass, field
from typing import List

from PIL import Image


# ============================================================
# DFF binary reading (from read_font.py)
# ============================================================

@dataclass
class DffData:
    """Data structure for parsed DFF binary file."""
    header: List[int] = field(default_factory=list)          # 3 values: count, width, height
    array_5x256: List[List[int]] = field(default_factory=list)  # 256 lines of (index, x0, y0, x1, y1)
    fair_count: int = 0                                      # fair count value
    array_2x256: List[List[int]] = field(default_factory=list)  # 256 pairs, actual index values


def read_binary_as_int32(file_path):
    """
    Reads a binary file and returns a DffData structure.
    - header: 3 values (count, width, height)
    - array_5x256: 256 lines of (index, x0, y0, x1, y1), index starts at 0
    - fair_count: 1 value
    - array_2x256: 256 pairs, actual index values
    """
    try:
        with open(file_path, 'rb') as f:
            data = f.read()
    except FileNotFoundError:
        print(f"Error: File '{file_path}' not found.")
        return None
    except Exception as e:
        print(f"Error reading file: {e}")
        return None

    # Calculate how many complete 4-byte integers can be read
    num_ints = len(data) // 4

    # Read all 4-byte integers
    values = []
    for i in range(num_ints):
        offset = i * 4
        value = struct.unpack('<i', data[offset:offset + 4])[0]
        values.append(value)

    result = DffData()

    # Header: first 3 values (count, width, height)
    result.header = values[:3]

    # array_5x256: next 256 lines, 5 values per line (index, x0, y0, x1, y1)
    pos = 3
    for _ in range(256):
        if pos >= num_ints:
            break
        end = min(pos + 5, num_ints)
        ixy01_5 = values[pos:end]
        index = ixy01_5[0]
        # If index is between -1 and -128, add 256
        if -128 <= index <= -1:
            ixy01_5[0] += 256
        result.array_5x256.append(ixy01_5)
        pos = end

    # fair_count: next 1 value
    if pos < num_ints:
        result.fair_count = values[pos]
        pos += 1

    # array_2x256: next 256 pairs, actual index values
    for _ in range(256):
        if pos >= num_ints:
            break
        end = min(pos + 2, num_ints)
        result.array_2x256.append(values[pos:end])
        pos = end

    return result


def dff_to_meta_list(dff):
    """Convert DFF data to metadata list. (Processed in memory, no TXT file)"""
    meta_list = []
    for idx, rect in enumerate(dff.array_5x256):
        asciicode = idx if idx < 128 else idx - 256
        x0 = rect[1]
        y0 = rect[2]
        x1 = rect[3]
        y1 = rect[4]
        meta_list.append([asciicode, x0, y0, x1, y1])
    return meta_list


# ============================================================
# Font metadata loading (from merge_font.py)
# ============================================================

def load_font_meta(file_path):
    """Read character code and coordinate info from text file."""
    meta_list = []
    if not os.path.exists(file_path):
        return meta_list

    with open(file_path, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            # Handle various delimiters: space, comma, tab
            parts = line.replace(',', ' ').split()
            if len(parts) >= 5:
                char_code = int(parts[0])
                coords = [int(p) for p in parts[1:5]]  # x0, y0, x1, y1
                meta_list.append([char_code] + coords)
    return meta_list


# ============================================================
# Font merging (from merge_font.py, TXT output removed)
# ============================================================

def merge_bitmap_fonts(img_a_path, meta_a, img_b_path, txt_b_path, output_img_path, output_dff_path):
    """
    Merge original ASCII font image with Korean font.
    - img_a_path: Original font image (BMP/PNG)
    - meta_a: Original font metadata (list read from DFF)
    - img_b_path: Korean font image (PNG)
    - txt_b_path: Korean font metadata (TXT)
    - output_img_path: Output image path (BMP)
    - output_dff_path: Output DFF binary path
    """
    # 1. Load metadata and images
    meta_b = load_font_meta(txt_b_path)

    img_a = Image.open(img_a_path).convert('RGB')
    img_b = Image.open(img_b_path).convert('RGB')

    # Merge A, B in order (no duplicate char codes)
    combined_meta = meta_a + meta_b

    codeIdxPair = []
    for idx, meta in enumerate(combined_meta):
        codeIdxPair.append((meta[0], idx))
    sorted_pair = sorted(codeIdxPair, key=lambda x: x[0], reverse=True)

    # 2. Organize individual char info and calculate total area
    chars_info = []
    total_area = 0
    font_height = None

    for item in combined_meta:
        char_code, x0, y0, x1, y1 = item
        w = x1 - x0 + 1
        h = y1 - y0 + 1

        if font_height is None:
            font_height = h

        # Select source image
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
        print("No character data to merge.")
        return False

    # 3. Calculate optimal canvas width (power of 2)
    target_width = math.sqrt(total_area * 0.8)

    # Round up to power of 2 (minimum size must be >= widest char width)
    max_char_width = max(c['w'] for c in chars_info)
    pot_width = 2 ** math.ceil(math.log2(max(target_width, max_char_width)))

    # 4. Calculate layout (line wrapping)
    current_x = 0
    current_y = 0

    for c in chars_info:
        # Move to next line if exceeds width
        if current_x + c['w'] > pot_width:
            current_x = 0
            current_y += font_height

        c['dest_x'] = current_x
        c['dest_y'] = current_y
        current_x += c['w']

    # Calculate final required height
    canvas_height = current_y + font_height

    print(f"  Final image size: {pot_width} x {canvas_height}")

    # 5. Create new image and blit fonts
    new_image = Image.new('RGB', (pot_width, canvas_height), (0, 0, 0, 0))

    for c in chars_info:
        # Crop font image and paste to new image
        char_crop = c['src_img'].crop(c['box'])
        dx0, dy0 = c['dest_x'], c['dest_y']
        dx1, dy1 = dx0 + c['w'], dy0 + c['h']

        new_image.paste(char_crop, (dx0, dy0))

        c['new_bbox'] = (dx0, dy0, dx1 - 1, dy1 - 1)

    # 6. Write binary file as little-endian 4-byte integers
    # '<i' format: < (little-endian), i (4-byte integer)
    with open(output_dff_path, 'wb') as out_b:
        # Header: [font count, image width, image height]
        font_count = len(chars_info)
        out_b.write(struct.pack('<iii', font_count, pot_width, canvas_height))

        # Data 1: [code, x0, y0, x1, y1] in order
        for c in chars_info:
            nx0, ny0, nx1, ny1 = c['new_bbox']
            out_b.write(struct.pack('<iiiii', c['code'], nx0, ny0, nx1, ny1))

        # Data 2: [code count] then [code, 0-base index] in order
        out_b.write(struct.pack('<i', font_count))
        for c, idx in sorted_pair:
            out_b.write(struct.pack('<ii', c, idx))

    # Save image
    new_image.save(output_img_path)
    print(f"  Generated: {output_img_path}, {output_dff_path}")
    return True


# ============================================================
# Font configuration table
# ============================================================

def get_font_configs(game_dir):
    """Generate font configuration table based on game installation path."""
    fonts_dir = os.path.join(game_dir, "data", "fonts")
    return [
        {
            "name": "font0",
            "dff": os.path.join(fonts_dir, "font0.dff"),
            "img_a": os.path.join(fonts_dir, "font0.bmp"),
            "img_b": "font/Umdot14.ttf_18.png",
            "txt_b": "font/Umdot14.ttf_18.txt",
        },
        {
            "name": "font1",
            "dff": os.path.join(fonts_dir, "font1.dff"),
            "img_a": os.path.join(fonts_dir, "font1.bmp"),
            "img_b": "font/Umdot12.ttf_16.png",
            "txt_b": "font/Umdot12.ttf_16.txt",
        },
        {
            "name": "font2",
            "dff": os.path.join(fonts_dir, "font2.dff"),
            "img_a": os.path.join(fonts_dir, "font2.bmp"),
            "img_b": "font/Umdot20-Bold.ttf_28.png",
            "txt_b": "font/Umdot20-Bold.ttf_28.txt",
        },
        {
            "name": "font3",
            "dff": os.path.join(fonts_dir, "font3.dff"),
            "img_a": os.path.join(fonts_dir, "font3.bmp"),
            "img_b": "font/Galmuri9.ttf_13.png",
            "txt_b": "font/Galmuri9.ttf_13.txt",
        },
        {
            "name": "font4",
            "dff": os.path.join(fonts_dir, "font4.dff"),
            "img_a": os.path.join(fonts_dir, "font4.bmp"),
            "img_b": "font/Galmuri11.ttf_15.png",
            "txt_b": "font/Galmuri11.ttf_15.txt",
        },
    ]


# ============================================================
# Main execution
# ============================================================
def build_font(name, src_path, ext_img, ext_txt, dst_path):

    if not os.path.exists(dst_path):
        os.makedirs(dst_path, exist_ok=True)

    # 1. Read original DFF file
    src_dff = os.path.join(src_path, f"{name}.dff")
    dff = read_binary_as_int32(src_dff)
    if dff is None:
        print(f"  Error: Cannot read DFF file: {src_dff}")
        return False
    
    # 2. Convert DFF data to metadata list
    meta_a = dff_to_meta_list(dff)
    
    # 3. Set output paths
    output_img = os.path.join(dst_path, f"{name}.bmp")
    output_dff = os.path.join(dst_path, f"{name}.dff")
    
    # 4. Execute merge
    src_bmp = os.path.join(src_path, f"{name}.bmp")
    return merge_bitmap_fonts(
            img_a_path=src_bmp,
            meta_a=meta_a,
            img_b_path=ext_img,
            txt_b_path=ext_txt,
            output_img_path=output_img,
            output_dff_path=output_dff
        )

def build_all_fonts(game_dir):
    """Generate all fonts in batch."""
    # Normalize path
    game_dir = os.path.normpath(game_dir)

    # Output directory: <game_dir>/data/font_kr
    output_dir = os.path.join(game_dir, "Data_kr/Fonts")
    if not os.path.exists(output_dir):
        os.makedirs(output_dir, exist_ok=True)

    # Get font configurations
    configs = get_font_configs(game_dir)

    success_count = 0
    for cfg in configs:
        name = cfg["name"]
        print(f"\n=== Processing {name} ===")

        # 1. Read original DFF file
        dff = read_binary_as_int32(cfg["dff"])
        if dff is None:
            print(f"  Error: Cannot read DFF file: {cfg['dff']}")
            continue

        # 2. Convert DFF data to metadata list
        meta_a = dff_to_meta_list(dff)

        # 3. Set output paths
        output_img = os.path.join(output_dir, f"{name}.bmp")
        output_dff = os.path.join(output_dir, f"{name}.dff")

        # 4. Execute merge
        try:
            if merge_bitmap_fonts(
                img_a_path=cfg["img_a"],
                meta_a=meta_a,
                img_b_path=cfg["img_b"],
                txt_b_path=cfg["txt_b"],
                output_img_path=output_img,
                output_dff_path=output_dff
            ):
                success_count += 1
        except Exception as e:
            print(f"  Error processing {name}: {e}")

    print(f"\n=== Build complete: {success_count}/{len(configs)} fonts generated ===")
    print(f"Output directory: {output_dir}")
    return success_count


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python build_fonts.py <Pool of Radiance path>")
        print("Example: python build_fonts.py ../Pool of Radiance")
        sys.exit(1)

    game_path = sys.argv[1]
    build_all_fonts(game_path)