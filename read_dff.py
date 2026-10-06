import struct
import sys
import os
from dataclasses import dataclass, field
from typing import List

from PIL import Image, ImageDraw


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
    print(f"File size: {len(data)} bytes")
    print(f"Number of 4-byte integers: {num_ints}")

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

    # Warn if there are leftover bytes
    remainder = len(data) % 4
    if remainder > 0:
        print(f"Warning: {remainder} leftover byte(s) at the end of the file.")

    return result


def print_dff_data(dff: DffData):
    """Print the DffData structure in a readable format."""
    print("\n=== Header ===")
    print(" ".join(f"{v:10d}" for v in dff.header))

    print("\n=== array_5x256 (index, x0, y0, x1, y1) x 256 lines ===")
    for i, line in enumerate(dff.array_5x256):
        print(f"[{i:4d}] " + " ".join(f"{v:10d}" for v in line))

    print(f"\n=== fair_count ===")
    print(f"{dff.fair_count}")

    print("\n=== array_2x256 (256 pairs) ===")
    for i, line in enumerate(dff.array_2x256):
        print(f"[{i:4d}] " + " ".join(f"{v:10d}" for v in line))


def draw_boxes_on_bmp(dff_path):
    """
    Reads the DFF file, finds the corresponding BMP file (same name, .bmp extension),
    verifies the BMP dimensions match the header (width, height),
    draws semi-transparent pink hollow boxes using array_5x256 data,
    and saves the result as a PNG file in the current folder.
    """
    # Read DFF data
    dff = read_binary_as_int32(dff_path)
    if dff is None:
        return

    # Determine BMP file path (same name, .bmp extension) and PNG path (current folder)
    base_name = os.path.splitext(dff_path)[0]
    bmp_path = base_name + ".bmp"
    png_name = os.path.basename(base_name) + ".png"
    png_path = os.path.join(os.getcwd(), png_name)

    # Check if BMP file exists
    if not os.path.exists(bmp_path):
        print(f"Error: BMP file '{bmp_path}' not found.")
        return

    # Open BMP image
    try:
        img = Image.open(bmp_path)
        img = img.convert("RGBA")
    except Exception as e:
        print(f"Error opening BMP file: {e}")
        return

    # Verify dimensions match header
    expected_width = dff.header[1]
    expected_height = dff.header[2]
    actual_width, actual_height = img.size

    print(f"BMP size: {actual_width}x{actual_height}")
    print(f"Header expected: {expected_width}x{expected_height}")

    if actual_width != expected_width or actual_height != expected_height:
        print(f"Warning: BMP dimensions ({actual_width}x{actual_height}) "
              f"do not match header ({expected_width}x{expected_height}).")
    else:
        print("BMP dimensions match header.")

    # Create overlay for semi-transparent pink boxes
    overlay = Image.new("RGBA", img.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(overlay)

    # Pink color with semi-transparency (alpha=128)
    pink = (255, 105, 180, 128)

    # Draw hollow boxes using array_5x256 data: (index, x0, y0, x1, y1)
    for i, box in enumerate(dff.array_5x256):
        if len(box) < 5:
            continue
        index, x0, y0, x1, y1 = box[0], box[1], box[2], box[3], box[4]
        # Draw hollow rectangle (outline only) from (x0, y0) to (x1, y1)
        draw.rectangle([x0, y0, x1, y1], outline=pink, width=1)

    # Composite overlay onto original image
    result = Image.alpha_composite(img, overlay)

    # Save as PNG
    result.save(png_path, "PNG")
    print(f"Result saved to: {png_path}")


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python read_dff.py <dff_file_path>")
        sys.exit(1)

    dff_path = sys.argv[1]
    draw_boxes_on_bmp(dff_path)