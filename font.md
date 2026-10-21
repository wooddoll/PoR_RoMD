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

### 3.2 array_5xN (N줄 × 5개 정수)

```
code, x0, y0, x1, y1
```

- **code**: 0부터 시작해서 127, 이후 -128 ~ -1 (모든 파일에서 동일)
  - 128~255는 `0x80FFFFFF` ~ `0xFFFFFFFF`로 저장됨
  - 256은 `0x00010000`으로 저장됨
  - index가 아니라 code임. signed char로 ASCII코드를 읽었을 때의 값
- **x0, y0**: 글리프 바운딩 박스 좌상단 좌표 (inclusive)
- **x1, y1**: 글리프 바운딩 박스 우하단 좌표 (Exclusive)

### 3.3 fair_count (1개 정수)

```
256,    // fair count
```

- 폰트의 코드를 실제 index로 바꾸기 위한 table

### 3.4 array_2xM (M쌍 × 2개 정수)

```
[127, 127], [126, 126] ... [0, 0]                    // 128쌍, 일정하게 감소
[0xFFFFFFFF, 0xFF000000], [0xFEFFFFFF, 0xFE000000] ... [0x80FFFFFF, 0x80000000]  // 128쌍
```

- **첫 128쌍**: `[127,127]`부터 `[0,0]`
- **다음 128쌍**: `[-128,128]`부터 `[-1,255]`
- 순서는 이상하지만, signed char로 ASCII코드를 읽었을 때의 값을 index로 변환하는 테이블


---

## 4. read_font.py 동작 분석

### 4.1 read_binary_as_int32(file_path)

DFF 바이너리 파일을 4바이트 리틀 엔디언 정수로 읽어 `DffData` 구조체로 반환

```
DffData {
    header: [count, width, height]          // 3개 정수
    array_5xN: [[code, x0, y0, x1, y1]]  // N줄
    fair_count: int                          // 1개 정수
    array_2xM: [[v1, v2]]                  // M쌍
}
```

### 4.2 analyFontH(dff)

`array_5xN`의 y0, y1 좌표를 이용해 폰트 높이 집합 계산

```
font heights: {y1 - y0}  // 각 글리프의 높이 집합
```

### 4.3 draw_boxes_on_bmp(dff, dff_path)

1. DFF 파일과 동일한 이름의 BMP 파일을 찾음
2. BMP 크기와 헤더의 (width, height) 일치 여부 확인
3. `array_5xN`의 (x0, y0, x1, y1) 좌표로 **반투명 핑크색 사각형 테두리**를 그림
4. 결과를 PNG 파일로 저장 (현재 폴더)

> **참고**: `get_font_baseline_offset()` 호출을 포함하지 않는다. 베이스라인 분석은 `--baseline` 옵션으로 별도 실행한다.

### 4.4 get_font_baseline_offset(image, bbox, is_bg_white)

'A' 글자의 바운딩 박스를 분석하여 실제 글자 픽셀 경계를 탐색

- **글자 픽셀 판정 기준**:
  - 흰 배경(`is_bg_white=True`): R < 127 → 글자 픽셀
  - 검은 배경(`is_bg_white=False`): R > 127 → 글자 픽셀
- **계산 결과**:
  - 실제 글자 순수 폭(Width), 순수 높이(Height)
  - 좌측/우측/상단/하단 패딩
  - 상단 시작점으로부터 베이스라인까지의 거리

**출력 형식**:

```
=== 글자 크기 분석 결과 ===
  bbox 크기        : 10 x 19 px
  실제 글자 폭     : 8 px
  실제 글자 높이   : 11 px
  좌측 패딩        : 0 px
  우측 패딩        : 2 px
  상단 패딩        : 3 px
  하단 패딩        : 5 px
  베이스라인 좌표  : 13 px (상단 기준)
===========================
```

### 4.5 CLI 실행 옵션

`read_font.py`는 명령줄 옵션을 통해 각 기능을 개별적으로 실행할 수 있다.

#### 4.5.1 사용법

```
python read_font.py <dff_path>                    # DFF 파일을 TXT로 변환 (기존 동작)
python read_font.py <dff_dir>                     # 디렉토리의 모든 fontX.dff 일괄 처리
python read_font.py --baseline <dff_path> [char]  # 베이스라인 오프셋 분석 (기본 문자: 'A')
python read_font.py --draw <dff_path>             # BMP에 바운딩 박스 그리기 (PNG 저장)
python read_font.py --help                        # 도움말 표시
```

#### 4.5.2 `--baseline` 옵션 — 베이스라인 오프셋 분석

`get_font_baseline_offset()`을 단독으로 실행한다.

```bash
python read_font.py --baseline font0.dff          # 'A' 문자 기준 분석
python read_font.py --baseline font0.dff B        # 'B' 문자 기준 분석
```

- DFF 파일과 **동일한 이름의 BMP 파일**을 자동으로 찾아 읽는다.
- 지정된 문자(기본값: `'A'`)의 바운딩 박스를 `array_5xN`에서 검색한다.
- `get_font_baseline_offset()`을 호출하여 실제 글자 크기, 패딩, 베이스라인 거리를 출력한다.
- 선택적 `char` 인자로 분석할 문자를 지정할 수 있다.

#### 4.5.3 `--draw` 옵션 — 바운딩 박스 그리기

`draw_boxes_on_bmp()`을 단독으로 실행한다.

```bash
python read_font.py --draw font0.dff
```

- DFF 파일과 **동일한 이름의 BMP 파일**을 자동으로 찾아 읽는다.
- `array_5xN`의 (x0, y0, x1, y1) 좌표로 **반투명 핑크색 사각형 테두리**를 그린다.
- 결과를 **현재 폴더**에 `[파일명].png`로 저장한다.

#### 4.5.4 `--help` 옵션 — 도움말

```bash
python read_font.py --help
```

- 사용 가능한 모든 CLI 옵션과 사용법을 출력한다.

---

## 5. 폰트별 분석 결과

`--baseline` 옵션으로 각 폰트의 'A' 글자를 분석한 결과이다.

### font0 — 가능한 한글 폰트 13px 이하

```
=== 글자 크기 분석 결과 ===
  bbox 크기        : 9 x 18 px
  실제 글자 폭     : 8 px
  실제 글자 높이   : 11 px
  좌측 패딩        : 0 px
  우측 패딩        : 1 px
  상단 패딩        : 3 px
  하단 패딩        : 4 px
  베이스라인 좌표  : 13 px (상단 기준)
===========================
```

### font1 — 가능한 한글 폰트 12px 이하

```
=== 글자 크기 분석 결과 ===
  bbox 크기        : 8 x 16 px
  실제 글자 폭     : 7 px
  실제 글자 높이   : 9 px
  좌측 패딩        : 0 px
  우측 패딩        : 1 px
  상단 패딩        : 4 px
  하단 패딩        : 3 px
  베이스라인 좌표  : 12 px (상단 기준)
===========================
```

### font2 — 가능한 한글 폰트 21px 이하

```
=== 글자 크기 분석 결과 ===
  bbox 크기        : 13 x 28 px
  실제 글자 폭     : 12 px
  실제 글자 높이   : 17 px
  좌측 패딩        : 0 px
  우측 패딩        : 1 px
  상단 패딩        : 5 px
  하단 패딩        : 6 px
  베이스라인 좌표  : 21 px (상단 기준)
===========================
```

### font3 — 가능한 한글 폰트 9px 이하

```
=== 글자 크기 분석 결과 ===
  bbox 크기        : 7 x 13 px
  실제 글자 폭     : 7 px
  실제 글자 높이   : 7 px
  좌측 패딩        : 0 px
  우측 패딩        : 0 px
  상단 패딩        : 3 px
  하단 패딩        : 3 px
  베이스라인 좌표  : 9 px (상단 기준)
===========================
```

### font4 — 가능한 한글 폰트 11px 이하

```
=== 글자 크기 분석 결과 ===
  bbox 크기        : 7 x 15 px
  실제 글자 폭     : 7 px
  실제 글자 높이   : 9 px
  좌측 패딩        : 0 px
  우측 패딩        : 0 px
  상단 패딩        : 3 px
  하단 패딩        : 3 px
  베이스라인 좌표  : 11 px (상단 기준)
===========================
```

---

## 6. 폰트 스타일 분류

| 폰트 | 스타일 | 크기 | 비고 |
|------|--------|------|------|
| font0 | 장식적, 판타지 느낌 | 13px | Umdot14 |
| font1 | 장식적, 판타지 느낌 | 12px | Umdot12 |
| font2 | 장식적, 판타지 느낌 | 21px | Umdot20 |
| font3 | 딱딱하고 포멀한, 시스템용 | 9px | 갈무리9 |
| font4 | 딱딱하고 포멀한, 시스템용 | 11px | 갈무리11 |
* 영문과 같은 baseline을 쓰면 한글은 글자가 살짝 뜬 것처럼 보이므로 baseline-1 사용
---

## 8. draw_font_sheet.py — 비트맵 폰트 시트 생성

TTF 폰트를 받아 안티앨리어싱이 없는 단색 비트맵 폰트 시트(PNG + TXT)를 생성하는 스크립트.

### 8.1 CLI 실행 옵션

```
python draw_font_sheet.py                          # 모든 폰트 일괄 생성 (기본 동작)
python draw_font_sheet.py --draw-boxes             # 각 슬롯에 반투명 핑크색 박스 그리기
python draw_font_sheet.py --help                   # 도움말 표시
```

### 8.2 `--draw-boxes` 옵션 — 슬롯 박스 그리기

`read_font.py`의 `draw_boxes_on_bmp()`와 동일한 방식으로, 생성된 비트맵 폰트 시트의 각 슬롯 경계에 **반투명 핑크색 사각형 테두리**를 그린다.

```bash
python draw_font_sheet.py --draw-boxes
```

- 각 문자의 슬롯 경계(x0, y0, x1, y1)에 `(255, 105, 180, 128)` 색상의 반투명 핑크색 박스를 그린다.
- 박스는 슬롯의 좌표 데이터(TXT 파일)와 동일한 경계를 사용한다.
- 결과물은 기존과 동일한 경로(`./font/`)에 저장된다.

---

## 9. merge_font.py — 폰트 병합 및 랜덤 혼합 시트 생성

ASCII 폰트(`img_a`)와 한글 폰트(`img_b`)를 병합하거나, 랜덤 혼합 시트를 생성하는 스크립트.

### 9.1 CLI 실행 옵션

```
python merge_font.py [--output-dir <path>]              # 모든 폰트 일괄 병합 (기본 동작)
python merge_font.py --random-sheet <font_index>        # 랜덤 혼합 시트 생성 (0-4)
python merge_font.py --random-sheet <font_index> --grid <N>  # 그리드 크기 지정 (기본 64)
python merge_font.py --help                             # 도움말 표시
```

### 9.2 `--random-sheet` 옵션 — 랜덤 혼합 시트 생성

`img_a`(ASCII 영역)의 알파벳 대소문자(A-Z, a-z)와 `img_b`(한글 영역)의 글자를 랜덤하게 섞어 `grid_size x grid_size`(기본 64x64 = 4096개) 글자가 한 장에 들어가도록 이미지와 TXT 메타데이터를 생성한다.

```bash
python merge_font.py --random-sheet 0                  # font0 기준 64x64 그리드 생성
python merge_font.py --random-sheet 0 --grid 8         # 8x8 그리드로 테스트
```

- **글리프 추출 기준**:
  - ASCII: 코드 65~90(A-Z), 97~122(a-z)
  - 한글: CP949 코드 0x8000 이상
- **배치 방식**: 각 글자를 **자신의 RECT(바운딩 박스) 크기대로 좌측 정렬로 연속 배치**
  - 영문자는 좁은 폭(3~9px), 한글은 넓은 폭(13px)으로 자연스럽게 연결
  - 글자 간 1px 간격
  - 행 높이는 최대 글리프 높이 + 1px
- **출력 파일**: `font/font_kr/[font_name]_random_sheet.png` + `.txt`
