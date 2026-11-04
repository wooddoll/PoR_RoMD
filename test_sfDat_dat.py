import argparse
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
        strings[f'{i}_kr'] = ''

    return strings

def encode_sfdat(strings, out_path, encoding="utf-16le"):
    entry_count = len(strings)

    offsets = []
    plain_data = bytearray()

    current_char_pos = 0

    for i in range(entry_count):
        text = strings.get(f'{i}', None)
        if text is None:
            entry_count = i
            break

        str = text.encode(encoding)
        str_kor = strings.get(f'{i}_kr', None)
        if str_kor is not None and str_kor:
            str = str_kor.encode(encoding)

        if len(str) % 2 != 0:
            raise ValueError(f"문자열 {i}번의 인코딩 결과가 2바이트 단위가 아닙니다.")

        offsets.append(current_char_pos)
        plain_data.extend(str)

        current_char_pos += len(str) // 2

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

def _xor_entries(plain_data, offsets, total_chars):
    """엔트리별 키로 XOR (decode/encode 공통, XOR은 대칭)."""
    data = bytearray(plain_data)
    entry_count = len(offsets)
    for i in range(entry_count):
        start = offsets[i]
        end = offsets[i + 1] if i + 1 < entry_count else total_chars
        key_low = (i + 0x55) & 0xFF
        key_high = (i - 0x45) & 0xFF
        for pos in range(start, end):
            data[pos * 2] ^= key_low
            data[pos * 2 + 1] ^= key_high
    return data


def cp949_to_wide(text, byte_pos="low"):
    """
    CP949 문자열을 '1바이트 = 1 UTF-16 유닛'으로 펼친다.
    byte_pos="low" : [b, 0x00] (ASCII의 UTF-16LE와 동일한 배치, 기본값)
    byte_pos="high": [0x00, b]
    """
    raw = text.encode("cp949")  # cp949로 표현 불가능한 문자가 있으면 UnicodeEncodeError
    out = bytearray()
    for b in raw:
        if byte_pos == "low":
            out.append(b)
            out.append(0)
        else:
            out.append(0)
            out.append(b)
    return bytes(out)


def encode_sfdat_cp949(strings, out_path, byte_pos="low"):
    """
    '{i}_kr' 이 있으면 CP949로 인코딩하여 바이트당 2바이트(1 유닛)로 배분,
    없으면 기존 방식(UTF-16LE)으로 원문을 그대로 인코딩한다.
    """
    offsets = []
    plain_data = bytearray()
    current_char_pos = 0

    i = 0
    while True:
        text = strings.get(f'{i}', None)
        if text is None:
            break

        kr = strings.get(f'{i}_kr', None)
        if kr:
            try:
                encoded = cp949_to_wide(kr, byte_pos)
            except UnicodeEncodeError as e:
                raise ValueError(f"문자열 {i}번에 CP949로 표현할 수 없는 문자가 있습니다: {e}")
        else:
            encoded = text.encode("utf-16le")

        offsets.append(current_char_pos)
        plain_data.extend(encoded)
        current_char_pos += len(encoded) // 2
        i += 1

    entry_count = len(offsets)
    total_chars = current_char_pos
    encrypted = _xor_entries(plain_data, offsets, total_chars)

    with open(out_path, "wb") as f:
        f.write(struct.pack("<I", entry_count))
        f.write(struct.pack("<I", total_chars))
        for off in offsets:
            f.write(struct.pack("<I", off))
        f.write(encrypted)


def verify_cp949(strings, path, byte_pos="low"):
    """생성한 파일을 decode_sfdat으로 다시 읽어, 각 엔트리가 의도한 CP949 바이트와 같은지 검사."""
    decoded = decode_sfdat(path)   # 키 i -> 문자열(UTF-16LE 디코드)
    # 디코드된 문자열에서 1유닛=1바이트를 복원
    bad = []
    i = 0
    while strings.get(str(i)) is not None:
        kr = strings.get(f'{i}_kr')
        expected = kr.encode("cp949") if kr else None
        raw = decoded[i].encode("utf-16le")          # 엔트리의 원시 바이트
        got = raw[0::2] if byte_pos == "low" else raw[1::2]
        if expected is not None and got != expected:
            bad.append(i)
        i += 1
    print("mismatch:", bad[:20], "총", len(bad))


def main():
    parser = argparse.ArgumentParser(
        description="Decode sfDat.dat to JSON or encode JSON back to sfDat.dat."
    )
    parser.add_argument(
        "-m", "--mode",
        required=True,
        choices=("decode", "encode", "encode_kr"),
        help="operation to perform",
    )
    parser.add_argument("input_path", help="path to the input file")
    parser.add_argument("output_path", help="path to the output file")
    args = parser.parse_args()

    if args.mode == "decode":
        strings = decode_sfdat(args.input_path)
        with open(args.output_path, "w", encoding="utf-8") as out_file:
            json.dump(strings, out_file, ensure_ascii=False, indent=2)
    else:
        with open(args.input_path, "r", encoding="utf-8") as in_file:
            json_strings = json.load(in_file)
        if not isinstance(json_strings, dict):
            parser.error("The input JSON must have an object at the top level.")

        if args.mode == "encode":
            encode_sfdat(json_strings, args.output_path)
        else:
            encode_sfdat_cp949(json_strings, args.output_path)

def test():
        # 오프셋/크기 분포도 확인
    import struct
    with open("sfDat_kr.dat", "rb") as f:
        n, total = struct.unpack("<II", f.read(8))
        offs = struct.unpack("<" + "I"*n, f.read(4*n))
    print("entries:", n, "total units:", total, "max offset:", max(offs))
    print("파일 크기:", 8 + 4*n + total*2)


if __name__ == "__main__":
    main()
