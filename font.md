# 폰트 파일 분석 (read_font.py)

## 1. 파일 구성

폰트 파일 폴더에는 2쌍의 파일이 있다.

```
[fontX.bmp, fontX.dff] 5쌍
```

- **file path**: `\Pool of Radiance\data\fonts\`
- **파일 목록**: font0 ~ font4 (총 5쌍)

| 파일 | BMP 크기 | DFF 크기 |
|------|----------|----------|
| font0 | 565x80 | 7,184 Byte |
| font1 | 520x72 | 7,184 Byte |
| font2 | 811x120 | 7,184 Byte |
| font3 | 544x60 | 7,184 Byte |
| font4 | 600x68 | 7,184 Byte |

---

## 2. BMP 파일 구조 (fontX.bmp)

- 흑백 이미지 파일이며, 비트맵 폰트가 포함되어 있음
- **4줄로 구분**되어 있으며, 아스키코드 순서로 배치됨

| 줄 | 아스키코드 범위 |
|----|----------------|
| 1  | 0 ~ 63 |
| 2  | 64 ~ 127 |
| 3  | 128 ~ 191 |
| 4  | 192 ~ 255 |

---

## 3. DFF 파일 구조 (fontX.dff)

- **4바이트 단위 정수**, **리틀 엔디언** (little-endian)
- 총 7,184 Byte = 1,796개의 4바이트 정수

### 3.1 헤더 (12 Byte, 3개 정수)

```
256, 565, 80,    // [폰트 개수, 이미지 크기 가로, 세로]
```

| 오프셋 | 값 | 설명 |
|--------|-----|------|
| +0x00 | 256 | 폰트 개수 (glyph count) |
| +0x04 | 565 | 이미지 가로 크기 (width) |
| +0x08 | 80  | 이미지 세로 크기 (height) |

### 3.2 array_5x256 (256줄 × 5개 정수)

```
index, x0, y0, x1, y1
```

- **index**: 0부터 시작해서 255까지 증가 (모든 파일에서 동일)
  - 128~255는 `0x80FFFFFF` ~ `0xFFFFFFFF`로 저장됨
  - 256은 `0x00010000`으로 저장됨
  - **음수 보정**: index가 -1 ~ -128 범위이면 +256을 더해 양수로 변환
- **x0, y0**: 글리프 바운딩 박스 좌상단 좌표
- **x1, y1**: 글리프 바운딩 박스 우하단 좌표

### 3.3 fair_count (1개 정수)

```
256,    // fair count
```

- 아마도 index의 값이 음수와 양수를 오가기 때문에 보정하려는 의도로 보임

### 3.4 array_2x256 (256쌍 × 2개 정수)

```
[127, 127], [126, 126] ... [0, 0]                    // 128쌍, 일정하게 감소
[0xFFFFFFFF, 0xFF000000], [0xFEFFFFFF, 0xFE000000] ... [0x80FFFFFF, 0x80000000]  // 128쌍
```

- **첫 128쌍**: `[127,127]`부터 `[0,0]`까지 일정하게 감소
- **다음 128쌍**: 음수로 나오는 index에 대한 실제 양수 값 index 매핑

---

## 4. read_font.py 동작 분석

### 4.1 read_binary_as_int32(file_path)

DFF 바이너리 파일을 4바이트 리틀 엔디언 정수로 읽어 `DffData` 구조체로 반환

```
DffData {
    header: [count, width, height]          // 3개 정수
    array_5x256: [[index, x0, y0, x1, y1]]  // 256줄
    fair_count: int                          // 1개 정수
    array_2x256: [[v1, v2]]                  // 256쌍
}
```

### 4.2 analyFontH(dff)

`array_5x256`의 y0, y1 좌표를 이용해 폰트 높이 집합 계산

```
font heights: {y1 - y0 + 1}  // 각 글리프의 높이 집합
```

### 4.3 draw_boxes_on_bmp(dff, dff_path)

1. DFF 파일과 동일한 이름의 BMP 파일을 찾음
2. BMP 크기와 헤더의 (width, height) 일치 여부 확인
3. `array_5x256`의 (x0, y0, x1, y1) 좌표로 **반투명 핑크색 사각형 테두리**를 그림
4. 결과를 PNG 파일로 저장 (현재 폴더)

### 4.4 get_font_baseline_offset(image, bbox, is_bg_white)

'A' 글자의 바운딩 박스를 분석하여 실제 글자 픽셀 경계를 탐색

- **글자 픽셀 판정 기준**:
  - 흰 배경(`is_bg_white=True`): R < 127 → 글자 픽셀
  - 검은 배경(`is_bg_white=False`): R > 127 → 글자 픽셀
- **계산 결과**:
  - 실제 글자 순수 폭(Width), 순수 높이(Height)
  - 상단/우측/하단 패딩
  - 상단 시작점으로부터 베이스라인까지의 거리

---

## 5. 폰트별 분석 결과

### font0 — 가능한 한글 폰트 13px 이하

```
font heights: {19}
BMP size: 565x80
Header expected: 565x80
BMP dimensions match header.
```

**A 글자 분석 결과**:

| 항목 | 값 |
|------|-----|
| bbox 크기 | (10, 19) |
| left | 0px |
| right | 7px |
| upper | 3px |
| bottom | 13px |
| 실제 글자 폭 | 8 px |
| 실제 글자 높이 | 11 px |
| upper padding | 3 px |
| right padding | 2 px |
| bottom padding | 5 px |
| 베이스라인 거리 | 14 px |

### font1 — 가능한 한글 폰트 12px 이하

```
font heights: {17}
BMP size: 520x72
Header expected: 520x72
BMP dimensions match header.
```

**A 글자 분석 결과**:

| 항목 | 값 |
|------|-----|
| bbox 크기 | (9, 17) |
| left | 0px |
| right | 6px |
| upper | 4px |
| bottom | 12px |
| 실제 글자 폭 | 7 px |
| 실제 글자 높이 | 9 px |
| upper padding | 4 px |
| right padding | 2 px |
| bottom padding | 4 px |
| 베이스라인 거리 | 13 px |

### font2 — 가능한 한글 폰트 21px 이하

```
font heights: {29}
BMP size: 811x120
Header expected: 811x120
BMP dimensions match header.
```

**A 글자 분석 결과**:

| 항목 | 값 |
|------|-----|
| bbox 크기 | (14, 29) |
| left | 0px |
| right | 11px |
| upper | 5px |
| bottom | 21px |
| 실제 글자 폭 | 12 px |
| 실제 글자 높이 | 17 px |
| upper padding | 5 px |
| right padding | 2 px |
| bottom padding | 7 px |
| 베이스라인 거리 | 22 px |

### font3 — 가능한 한글 폰트 9px 이하

```
File size: 7184 bytes
Number of 4-byte integers: 1796
font heights: {14}
BMP size: 544x60
Header expected: 544x60
BMP dimensions match header.
```

**A 글자 분석 결과**:

| 항목 | 값 |
|------|-----|
| bbox 크기 | (8, 14) |
| left | 0px |
| right | 6px |
| upper | 3px |
| bottom | 9px |
| 실제 글자 폭 | 7 px |
| 실제 글자 높이 | 7 px |
| upper padding | 3 px |
| right padding | 1 px |
| bottom padding | 4 px |
| 베이스라인 거리 | 10 px |

### font4 — 가능한 한글 폰트 11px 이하

```
font heights: {16}
BMP size: 600x68
Header expected: 600x68
BMP dimensions match header.
```

**A 글자 분석 결과**:

| 항목 | 값 |
|------|-----|
| bbox 크기 | (8, 16) |
| left | 0px |
| right | 6px |
| upper | 3px |
| bottom | 11px |
| 실제 글자 폭 | 7 px |
| 실제 글자 높이 | 9 px |
| upper padding | 3 px |
| right padding | 1 px |
| bottom padding | 4 px |
| 베이스라인 거리 | 12 px |

---

## 6. 폰트 스타일 분류

| 폰트 | 스타일 | 크기 | 비고 |
|------|--------|------|------|
| font0 | 장식적, 판타지 느낌 | 13px | Umdot font |
| font1 | 장식적, 판타지 느낌 | 12px | Umdot font |
| font2 | 장식적, 판타지 느낌 | 21px | Umdot font |
| font3 | 딱딱하고 포멀한, 시스템용 | 9px | 굴림체? |
| font4 | 딱딱하고 포멀한, 시스템용 | 11px | 굴림체? |

---

## 7. 게임 내부 구조 (cFont)

### struct cFont

```c
struct cFont {
    void *vtable;              // +0x00

    int glyphCount;            // +0x04
    Glyph **glyphs;            // +0x08
    CharCodeMap *charMap;      // +0x0c

    int unknown_10;            // +0x10

    IDirectDrawSurface *surface; // +0x14

    HDC hdc;                   // +0x18
    HBITMAP dibBitmap;         // +0x1c
    void *dibBits;             // +0x20

    int atlasWidth;            // +0x24
    int atlasHeight;           // +0x28
};
```

### struct Glyph

```c
struct Glyph {
    int charCode;              // +0x00
    int left;                  // +0x04
    int top;                   // +0x08
    int right;                 // +0x0c
    int bottom;                // +0x10
    void *surface;             // +0x14
};
```

### struct CharCodeMap

```c
struct CharCodeMap {
    int count;                 // +0x00
    int **keys;                // +0x04
    int **values;              // +0x08
};
