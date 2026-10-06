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



//////////

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

