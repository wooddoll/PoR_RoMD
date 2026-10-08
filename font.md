폰트 파일 폴더에는 2쌍의 파일이 있다.
[fontX.bmp, fontX.dff] 5쌍

file path: \Pool of Radiance\data\fonts\

font0.bmp, font1.bmp, font2.bmp, font3.bmp, font4.bmp // font0.bmp(565x80x3B), 파일별로 크기가 다름
흑백 이미지 파일이며, 비트맵 폰트가 있음. 4줄로 구분, 아스키코드 순서
1: 0 ~ 63
2: 64 ~ 127
3: 128 ~ 191
4: 192 ~ 255

font0.dff, font1.dff, font2.dff, font3.dff, font4.dff // 동일하게 7,184 Byte
4바이트 단위 정수, 리틀 엔디언
256, 565, 80,       // header부분 12byte [폰트 개수, 이미지 크기 가로, 세로]
index, x0, y0, x1, y1      // index값은 0부터 시작해서 255까지 증가. 모든 파일에서 동일
                           // 128~255는 0x80FFFFFF ~ 0xFFFFFFFF이고 256은 0x00010000.
256,                    // fair count, 아마도 index의 값이 음수와 양수를 오가기 때문에 보정하려는 의도로 보임
[127, 127], [126, 126] ... [0, 0] // 128쌍, 일정하게 감소
[0xFFFFFFFF, 0xFF000000], [0xFEFFFFFF, 0xFE000000] ... [0x80FFFFFF, 0x80000000] // 128쌍, 음수로 나오는 index에 대한 실제 양수 값 index

font0--------------------------------- 가능한 한글 폰트 13px이하
font heights: {19}
BMP size: 565x80
Header expected: 565x80
BMP dimensions match header.
--- [실제 글자 크기 분석 결과] ---
   - bbox(10, 19)
■ 박스 내 상대 좌표 (0,0 기준)
   - left 0px, right: 7px
   - upper: 3px, bottom: 13px
---------------------------------
💡 실제 글자 순수 폭(Width)  : 8 px
💡 실제 글자 순수 높이(Height): 11 px
A 글자 기준: upper padding 3 px, right padding 2 px, bottom padding 5 px
💡 상단 시작점으로부터 베이스라인까지의 거리: 14 px

font1--------------------------------- 가능한 한글 폰트 12px이하
font heights: {17}
BMP size: 520x72
Header expected: 520x72
BMP dimensions match header.
--- [실제 글자 크기 분석 결과] ---
   - bbox(9, 17)
■ 박스 내 상대 좌표 (0,0 기준)
   - left 0px, right: 6px
   - upper: 4px, bottom: 12px
---------------------------------
💡 실제 글자 순수 폭(Width)  : 7 px
💡 실제 글자 순수 높이(Height): 9 px
A 글자 기준: upper padding 4 px, right padding 2 px, bottom padding 4 px
💡 상단 시작점으로부터 베이스라인까지의 거리: 13 px

font2--------------------------------- 가능한 한글 폰트 21px이하
font heights: {29}
BMP size: 811x120
Header expected: 811x120
BMP dimensions match header.
--- [실제 글자 크기 분석 결과] ---
   - bbox(14, 29)
■ 박스 내 상대 좌표 (0,0 기준)
   - left 0px, right: 11px
   - upper: 5px, bottom: 21px
---------------------------------
💡 실제 글자 순수 폭(Width)  : 12 px
💡 실제 글자 순수 높이(Height): 17 px
A 글자 기준: upper padding 5 px, right padding 2 px, bottom padding 7 px
💡 상단 시작점으로부터 베이스라인까지의 거리: 22 px

font3--------------------------------- 가능한 한글 폰트 9px이하
File size: 7184 bytes
Number of 4-byte integers: 1796
font heights: {14}
BMP size: 544x60
Header expected: 544x60
BMP dimensions match header.
--- [실제 글자 크기 분석 결과] ---
   - bbox(8, 14)
■ 박스 내 상대 좌표 (0,0 기준)
   - left 0px, right: 6px
   - upper: 3px, bottom: 9px
---------------------------------
💡 실제 글자 순수 폭(Width)  : 7 px
💡 실제 글자 순수 높이(Height): 7 px
A 글자 기준: upper padding 3 px, right padding 1 px, bottom padding 4 px
💡 상단 시작점으로부터 베이스라인까지의 거리: 10 px

font4--------------------------------- 가능한 한글 폰트 11px이하
font heights: {16}
BMP size: 600x68
Header expected: 600x68
BMP dimensions match header.
--- [실제 글자 크기 분석 결과] ---
   - bbox(8, 16)
■ 박스 내 상대 좌표 (0,0 기준)
   - left 0px, right: 6px
   - upper: 3px, bottom: 11px
---------------------------------
💡 실제 글자 순수 폭(Width)  : 7 px
💡 실제 글자 순수 높이(Height): 9 px
A 글자 기준: upper padding 3 px, right padding 1 px, bottom padding 4 px
💡 상단 시작점으로부터 베이스라인까지의 거리: 12 px
//////////
0, 1, 2 같은 폰트, 약간 장식적이고 판타지 느낌; 13px, 12px, 21px; Umdot font
3, 4 같은 폰트, 딱딱하고 포멀한, 시스템용 폰트; 9px, 11px; 굴림체?
////////////
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

struct Glyph {
    int charCode;              // +0x00
    int left;                  // +0x04
    int top;                   // +0x08
    int right;                 // +0x0c
    int bottom;                // +0x10
    void *surface;             // +0x14
};

struct CharCodeMap {
    int count;                 // +0x00
    int **keys;                // +0x04
    int **values;              // +0x08
};

