import argparse
#from html import parser
import struct
import json
import os
from kr_text import encode_kr, normalize_fullwidth

# ------------------------------------------------------------
# Helper utilities
# ------------------------------------------------------------

def load_json(json_path):
    with open(json_path, "rb") as f:
        raw_data = f.read()
        return json.loads(raw_data.decode("utf-8"))

def save_json(json_path, json_data):
    with open(json_path, "w", encoding="utf-8") as f:
        json.dump(json_data, f, ensure_ascii=False, indent=2)

# ------------------------------------------------------------
# Decoding (UTF‑16LE) – unchanged from original
# ------------------------------------------------------------

def _decode_utf16le_entry(data, start, length, byte_pos="low"):
    """Decode a slice that was stored as UTF‑16LE (plain text).
    ``byte_pos`` is kept for API compatibility – it is ignored because
    UTF‑16LE always uses the low‑byte first.
    """
    start_byte = start * 2
    end_byte = start_byte + length * 2
    raw16 = data[start_byte:end_byte]
    return raw16.decode("utf-16le").rstrip("\x00")

# ------------------------------------------------------------
# CP949 handling – new functionality
# ------------------------------------------------------------

def _decode_cp949_entry(data, start, length, byte_pos="low"):
    """Recover a CP949‑encoded string.
    The original encoder expanded each CP949 byte into a UTF‑16LE unit
    where either the low or the high byte held the original byte and the
    other byte was zero.  To retrieve the original Korean text we must:
      1. Extract the raw 2‑byte units.
      2. Collapse them back to a bytearray (taking low or high bytes).
      3. Decode the resulting byte stream with ``cp949``.
    """
    start_byte = start * 2
    end_byte = start_byte + length * 2
    raw16 = data[start_byte:end_byte]
    # ``raw16`` is a ``bytes`` object of length ``length*2``.
    # ``bytearray`` will hold the original CP949 bytes.
    if byte_pos == "low":
        # Keep every even byte (0,2,4,...)
        cp_bytes = raw16[0::2]
    else:
        # Keep every odd byte (1,3,5,...)
        cp_bytes = raw16[1::2]
    return cp_bytes.decode("cp949")

# ------------------------------------------------------------
# Core decode routine (shared for both modes)
# ------------------------------------------------------------

def decode_sfdat(path, cp949_mode=False, byte_pos="low"):
    """Decode ``sfDat`` files.
    Parameters
    ----------
    path: str
        Path to the binary ``sfDat`` file.
    cp949_mode: bool, optional
        If ``True`` treat *all* entries as CP949‑encoded.  This is useful when
        the file was produced with ``encode_kr``.
    byte_pos: "low" | "high"
        Which byte of the UTF‑16LE unit contains the original CP949 byte.
    Returns
    -------
    dict[int, dict]
        Mapping ``entry_id → {"en": …, "kr": …}``.  When ``cp949_mode`` is
        ``True`` the Korean text is placed under the ``"kr"`` key; the
        original UTF‑16LE text (if any) is still accessible via ``"en"``.
    """
    with open(path, "rb") as f:
        entry_count = struct.unpack("<I", f.read(4))[0]
        total_chars = struct.unpack("<I", f.read(4))[0]
        offsets = list(struct.unpack("<" + "I" * entry_count, f.read(entry_count * 4)))
        encrypted = bytearray(f.read(total_chars * 2))

    # De‑xor (the operation is symmetric)
    data = bytearray(encrypted)
    for i in range(entry_count):
        start = offsets[i]
        end = offsets[i + 1] if i + 1 < entry_count else total_chars
        key_low = (i + 0x55) & 0xFF
        key_high = (i - 0x45) & 0xFF
        for pos in range(start, end):
            data[pos * 2] ^= key_low
            data[pos * 2 + 1] ^= key_high

    result = {}
    for i in range(entry_count):
        start = offsets[i]
        end = offsets[i + 1] if i + 1 < entry_count else total_chars
        length = end - start
        entry = {}
        # Always decode the UTF‑16LE representation – it gives the original
        # English string when the entry was not a CP949 entry.
        entry["en"] = _decode_utf16le_entry(data, start, length)
        if cp949_mode:
            # Overwrite/augment with the Korean version.
            entry["kr"] = _decode_cp949_entry(data, start, length, byte_pos)
        result[i] = entry
    return result

# ------------------------------------------------------------
# Encoding helpers (unchanged except for slight refactor)
# ------------------------------------------------------------

def _xor_entries(plain_data, offsets, total_chars):
    """XOR each entry with its per‑index key – symmetric for encode/decode."""
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
    """Expand CP949 bytes to a UTF‑16LE unit.
    ``byte_pos`` decides whether the original byte lands in the low or high
    byte of the unit.
    """
    raw = normalize_fullwidth(text).encode("cp949")
    out = bytearray()
    for b in raw:
        if byte_pos == "low":
            out.append(b)
            out.append(0)
        else:
            out.append(0)
            out.append(b)
    return bytes(out)

def encode_sfdat(strings, out_path, encoding="utf-16le"):
    # Original encoder – kept for completeness.
    entry_count = len(strings)
    offsets = []
    plain_data = bytearray()
    current_char_pos = 0
    for i in range(entry_count):
        text = strings.get(str(i), None)
        if text is None:
            entry_count = i
            break
        encoded = text.encode(encoding)
        if len(encoded) % 2 != 0:
            raise ValueError(f"String {i} does not align to 2‑byte units.")
        offsets.append(current_char_pos)
        plain_data.extend(encoded)
        current_char_pos += len(encoded) // 2
    total_chars = current_char_pos
    encrypted = _xor_entries(plain_data, offsets, total_chars)
    with open(out_path, "wb") as f:
        f.write(struct.pack("<I", entry_count))
        f.write(struct.pack("<I", total_chars))
        for off in offsets:
            f.write(struct.pack("<I", off))
        f.write(encrypted)

def encode_sfdat_cp949(strings, out_path, byte_pos="low"):
    """Encode a dict ``{idx: {"en": …, "kr": …}}`` where ``"kr"`` is the
    Korean translation.  Entries lacking a ``"kr"`` key fall back to the
    English text (UTF‑16LE).
    """
    offsets = []
    plain_data = bytearray()
    current_char_pos = 0
    for i in range(len(strings)):
        entry = strings.get(str(i))
        if entry is None:
            break
        if "kr" in entry and entry["kr"]:
            try:
                encoded = cp949_to_wide(entry["kr"], byte_pos)
            except UnicodeEncodeError as e:
                raise ValueError(f"Entry {i} contains characters not representable in CP949: {e}")
        else:
            encoded = entry.get("en", "").encode("utf-16le")
        if len(encoded) % 2 != 0:
            raise ValueError(f"Entry {i} does not align to 2‑byte units.")
        offsets.append(current_char_pos)
        plain_data.extend(encoded)
        current_char_pos += len(encoded) // 2
    entry_count = len(offsets)
    total_chars = current_char_pos
    encrypted = _xor_entries(plain_data, offsets, total_chars)
    with open(out_path, "wb") as f:
        f.write(struct.pack("<I", entry_count))
        f.write(struct.pack("<I", total_chars))
        for off in offsets:
            f.write(struct.pack("<I", off))
        f.write(encrypted)


def combine(en_Path: str, kr_Path: str, dst_Path: str):
    #en_Path = "translation/sfDat_en.json"
    #kr_Path = "translation/sfDat_kr.json"
    #dst_Path = "translation/sfDat_en_kr.json"

    with open(en_Path, "r", encoding="utf-8") as in_file:
        en_strings = json.load(in_file)

    with open(kr_Path, "r", encoding="utf-8") as in_file:
        kr_strings = json.load(in_file)

    result = {}
    for key, value in en_strings.items():
        result[key] = { 'en': value }
        if key in kr_strings:
            if value != kr_strings[key]:
                result[key]['kr'] = kr_strings[key]
            else:
                result[key]['kr'] = ''
            
    with open(dst_Path, "w", encoding="utf-8") as out_file:
        json.dump(result, out_file, ensure_ascii=False, indent=2)

# ------------------------------------------------------------
# CLI entry point
# ------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description="SF Dat Tool")
    parser.add_argument("-m", "--mode", required=True,
                    choices=("decode","decode_kr","encode","encode_kr","combine"),
                    help="Operation mode.")
    parser.add_argument("input_path", help="Path to the first input file (or the only input for other modes).")
    parser.add_argument("output_path", help="Path to the output file.")
    parser.add_argument("--kr-input", "-k", dest="kr_input_path",
                    help="Second input file required for combine mode.")
    parser.add_argument("--byte-pos", choices=("low", "high"), default="low",
                        help="When using CP949, which byte contains the original data (default: low).")
    args = parser.parse_args()

    if args.mode == "combine":
        if not args.kr_input_path:
            raise ValueError("Combine mode requires --kr-input/-k argument.")
        combine(args.input_path, args.kr_input_path, args.output_path)
    elif args.mode.startswith("decode"):
        cp949 = args.mode == "decode_kr"
        decoded = decode_sfdat(args.input_path, cp949_mode=cp949, byte_pos=args.byte_pos)
        # Write a compact JSON (only the language that makes sense) – when
        # ``decode_kr`` we output only the Korean strings.
        
        if cp949:
            # Strip the "en" field – keep only Korean.
            out_dict = {str(k): v.get("kr", "") for k, v in decoded.items()}
        else:
            out_dict = {str(k): v.get("en", "") for k, v in decoded.items()}
            
        save_json(args.output_path, out_dict)
    else:
        # Encode – load JSON that follows the {idx: {"en": …, "kr": …}} schema.
        with open(args.input_path, "r", encoding="utf-8") as f:
            json_strings = json.load(f)
        if args.mode == "encode":
            encode_sfdat(json_strings, args.output_path)
        else:
            encode_sfdat_cp949(json_strings, args.output_path, byte_pos=args.byte_pos)




if __name__ == "__main__":
    main()
    #combine()
