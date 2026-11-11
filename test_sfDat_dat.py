import argparse
import struct
import os
import json

import struct

def load_json(json_path):
    with open(json_path, 'rb') as f:
        raw_data = f.read()
        data = json.loads(raw_data.decode('utf-8'))
        return data
    return {}

def save_json(json_path, json_data):
    with open(json_path, 'w', encoding='utf-8') as f:
        json.dump(json_data, f, ensure_ascii=False, indent=2)

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
        strings[i] = {}
        strings[i]['en'] = text

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

        kr = text.get('kr', None)
        if kr:
            try:
                encoded = cp949_to_wide(kr, byte_pos)
            except UnicodeEncodeError as e:
                raise ValueError(f"문자열 {i}번에 CP949로 표현할 수 없는 문자가 있습니다: {e}")
        else:
            encoded = text.get('en', '').encode("utf-16le")

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



def remerge():
    srcA_Path = "D:/workspace/PoR_RoMD/sfDat_backup.json"
    srcB_Path = "D:/workspace/PoR_RoMD/translation/sfDat.json"

    with open(srcA_Path, "r", encoding="utf-8") as in_file:
        jsonA_strings = json.load(in_file)

    with open(srcB_Path, "r", encoding="utf-8") as in_file:
        jsonB_strings = json.load(in_file)

    # A의 원문을 B의 번역으로 덮어쓰기
    for key in jsonB_strings:
        if key.endswith("_kr"):
            Bidx = int(key[:-3])
            Aidx = Bidx if Bidx < 900 else Bidx + 1
            
            if f'{Aidx}' in jsonA_strings and jsonB_strings[f'{Bidx}_kr']:
                jsonA_strings[f'{Aidx}_kr'] = jsonB_strings[f'{Bidx}_kr']   

    output_path = "D:/workspace/PoR_RoMD/translation/sfDat_merged.json"
    with open(output_path, "w", encoding="utf-8") as out_file:
        json.dump(jsonA_strings, out_file, ensure_ascii=False, indent=2)

#INPUT_PATH = "translation/sfDat.json"
#sfDat_data = load_json(INPUT_PATH)
def translation_sfDat(sfDat: dict, batch_size: int = 10):
    strings = {}
    for idx, value in sfDat.items():
        if idx.endswith("_kr"):
            i = int(idx[:-3])
            if strings.get(i, None) is None:
                strings[i] = {}
            strings[i]['kr'] = value
        else:
            i = int(idx)
            if strings.get(i, None) is None:
                strings[i] = {}
            strings[i]['en'] = value
    
    pending_idx = []
    pending_items = []
    for idx, value in strings.items():
        str_en = value.get('en', '')
        str_kr = value.get('kr', '')
        if not str_kr and len(str_en) > 2:
            pending_idx.append(idx)
            pending_items.append(str_en)

    total_pending = len(pending_items)
    print(f"총 번역 대상 수: {total_pending}개 (배치 크기: {batch_size})")
    processed_count = 0

    result = {}
    def sub_proc(idx_list, text_list):
        nonlocal processed_count  # 바깥 함수의 변수임을 명시하여 UnboundLocalError 방지
        # 2. batch_size 단위로 끊어서 번역 진행
        for i in range(0, len(text_list), batch_size):
            idx_to_send = idx_list[i : i + batch_size]
            words_to_send = text_list[i : i + batch_size]

            time.sleep(2)  # Rate Limit 완화
            kor_words = _request_gemini_translation(words_to_send)

            # 번역 결과 매핑 및 갱신
            if kor_words:
                for idx, trans_result in zip(idx_to_send, kor_words):
                    result[idx] = trans_result
                    processed_count += 1

                print(f"[{processed_count}/{total_pending}] 번역 완료 (진행률: {processed_count/total_pending*100:.1f}%)")
                # 매 배치 완료 후 중간 저장
                save_json(OUTPUT_PATH, result)
            else:
                print(f"배치 번역 실패 ({i} ~ {i + len(words_to_send)} 항목). 다음 배치를 진행합니다.")
          
            break

    sub_proc(pending_items, "아이템명")
    print("모든 번역 작업이 완료되었습니다.")

    return result
#result = translation_sfDat(sfDat_data, 10)

def convert():
    src_Path = "translation/sfDat_old.json"
    dst_Path = "translation/sfDat_new.json"

    with open(src_Path, "r", encoding="utf-8") as in_file:
        json_strings = json.load(in_file)

    result = {}
    for key, value in json_strings.items():
        if key.endswith("_kr"):
            idx = int(key[:-3])
            result[idx]['kr'] = value
        else:
            idx = int(key)
            result[idx] = { 'en': value }
            
    with open(dst_Path, "w", encoding="utf-8") as out_file:
        json.dump(result, out_file, ensure_ascii=False, indent=2)

if __name__ == "__main__":
    main()
    #convert()
