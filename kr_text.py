"""번역문을 게임 파일(EUC-KR)로 쓰기 전에 공통으로 거치는 정규화.

- 전각 영숫자/기호 U+FF01~U+FF5E (EUC-KR A3A1~A3FD, A3DC 제외) -> 반각 ASCII.
  영숫자가 2바이트가 되면 exe 의 바이트 기준 계산(m_nColumnMaxBytes 등)과 어긋나고,
  고해상도에서는 ASCII 와 같은 글꼴로 그리기 위해 반각으로 통일한다 (steam_text_hooks.md 16절).
  ￦(U+FFE6, A3DC), ￣(U+FFE3, A3FE)는 ASCII 대응이 없거나 다르므로 그대로 둔다.
"""


def normalize_fullwidth(text):
    if not text:
        return text
    return ''.join(chr(ord(c) - 0xFEE0) if 0xFF01 <= ord(c) <= 0xFF5E else c for c in text)


def encode_kr(text, errors='strict'):
    """normalize_fullwidth 후 EUC-KR 로 인코딩."""
    return normalize_fullwidth(text).encode('euc-kr', errors=errors)


if __name__ == '__main__':
    assert normalize_fullwidth('ＡＢＣ１２３！？　') == 'ABC123!?　'
    assert normalize_fullwidth('￦￣가') == '￦￣가'
    assert encode_kr('Ｈｐ ３') == b'Hp 3'
    print('ok')
