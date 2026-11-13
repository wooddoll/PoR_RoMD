"""sneg_ui_strings.dat (SUIS) 읽기/쓰기.

포맷: "SUIS" u32 version=1, u16 len + 언어 코드, u32 개수,
      항목마다 u16 키길이, 키, u32 코드페이지, u16 값길이, 값
"""
import struct
import sys


def load(path):
    b = open(path, 'rb').read()
    o = 0
    magic, ver = struct.unpack_from('<4sI', b, o); o += 8
    assert magic == b'SUIS' and ver == 1, (magic, ver)
    n = struct.unpack_from('<H', b, o)[0]; o += 2
    lang = b[o:o + n].decode('ascii'); o += n
    cnt = struct.unpack_from('<I', b, o)[0]; o += 4
    items = []
    for _ in range(cnt):
        kl = struct.unpack_from('<H', b, o)[0]; o += 2
        k = b[o:o + kl].decode('ascii'); o += kl
        cp = struct.unpack_from('<I', b, o)[0]; o += 4
        vl = struct.unpack_from('<H', b, o)[0]; o += 2
        v = b[o:o + vl]; o += vl
        items.append([k, cp, v])
    assert o == len(b), (o, len(b))
    return lang, items


def save(path, lang, items):
    out = bytearray(b'SUIS' + struct.pack('<I', 1))
    lb = lang.encode('ascii')
    out += struct.pack('<H', len(lb)) + lb
    out += struct.pack('<I', len(items))
    for k, cp, v in items:
        kb = k.encode('ascii')
        out += struct.pack('<H', len(kb)) + kb + struct.pack('<I', cp) + struct.pack('<H', len(v)) + v
    open(path, 'wb').write(out)


def text(cp, v):
    return v.decode('utf-8' if cp == 65001 else f'cp{cp}', 'replace')


if __name__ == '__main__':
    lang, items = load(sys.argv[1])
    print(lang, len(items), sorted(set(c for _, c, _ in items)))
    for k, cp, v in items:
        if len(sys.argv) < 3 or sys.argv[2] in k:
            print(k, cp, text(cp, v))
