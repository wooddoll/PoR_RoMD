import struct
import os
import json

import struct

def decode_sfdat(path):
    with open(path, "rb") as f:
        entry_count = struct.unpack("<I", f.read(4))[0]
        total_chars = struct.unpack("<I", f.read(4))[0]

        offsets = list(struct.unpack("<" + "I" * entry_count,
                                     f.read(entry_count * 4)))

        data = bytearray(f.read(total_chars * 2))

    lengths = [0] * entry_count
    accumulated = 0

    for i in range(entry_count - 1):
        start = offsets[i]
        next_start = offsets[i + 1]
        length = next_start - start

        lengths[i] = length

        key_low = (i + 0x55) & 0xFF
        key_high = (i - 0x45) & 0xFF

        for j in range(length):
            pos = start + j
            data[pos * 2] ^= key_low
            data[pos * 2 + 1] ^= key_high

        accumulated += length

    i = entry_count - 1
    length = total_chars - accumulated
    lengths[i] = length

    key_low = (i + 0x55) & 0xFF
    key_high = (i - 0x45) & 0xFF

    start = offsets[i]

    for j in range(length):
        pos = start + j
        data[pos * 2] ^= key_low
        data[pos * 2 + 1] ^= key_high

    strings = {}

    for i in range(entry_count):
        start_byte = offsets[i] * 2
        end_byte = start_byte + lengths[i] * 2
        raw16 = data[start_byte:end_byte]

        text = raw16.decode("utf-16le").rstrip("\x00")

        strings[i] = text

    return strings


import struct


def encode_sfdat(strings, out_path, encoding="utf-16le"):
    entry_count = len(strings)

    offsets = []
    plain_data = bytearray()

    current_char_pos = 0

    for i in range(entry_count):
        text = strings[i]

        raw = text.encode(encoding)

        if len(raw) % 2 != 0:
            raise ValueError(f"문자열 {i}번의 인코딩 결과가 2바이트 단위가 아닙니다.")

        offsets.append(current_char_pos)
        plain_data.extend(raw)

        current_char_pos += len(raw) // 2

    total_chars = current_char_pos

    encrypted_data = bytearray(plain_data)

    accumulated = 0
    lengths = [0] * entry_count

    for i in range(entry_count - 1):
        start = offsets[i]
        next_start = offsets[i + 1]
        length = next_start - start

        lengths[i] = length

        key_low = (i + 0x55) & 0xFF
        key_high = (i - 0x45) & 0xFF

        for j in range(length):
            pos = start + j
            encrypted_data[pos * 2] ^= key_low
            encrypted_data[pos * 2 + 1] ^= key_high

        accumulated += length

    i = entry_count - 1
    length = total_chars - accumulated
    lengths[i] = length

    key_low = (i + 0x55) & 0xFF
    key_high = (i - 0x45) & 0xFF

    start = offsets[i]

    for j in range(length):
        pos = start + j
        encrypted_data[pos * 2] ^= key_low
        encrypted_data[pos * 2 + 1] ^= key_high

    with open(out_path, "wb") as f:
        f.write(struct.pack("<I", entry_count))
        f.write(struct.pack("<I", total_chars))

        for off in offsets:
            f.write(struct.pack("<I", off))

        f.write(encrypted_data)

# --- 실행 코드 ---
if __name__ == "__main__":
    lines = decode_sfdat("D:/workspace/Pool of Radiance/sfDat.dat")

    output_txt_path = 'sfDat.json'
    with open(output_txt_path, 'w', encoding='utf-8') as out_f:
        json.dump(lines, out_f, ensure_ascii=False, indent=2)
    
    encode_sfdat(lines, 'sfDat_dump.bin')
