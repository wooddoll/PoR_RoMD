#pragma once

#include <intrin.h>
#include <cstdint>
#include <stddef.h>

#define WIN32_LEAN_AND_MEAN // 불필요한 Windows 헤더 제외 (선택 사항)
#include <windows.h>
#include <ddraw.h>          // IDirectDrawSurface7 및 DirectDraw 핵심 헤더

inline constexpr uintptr_t ptr_malloc = 0x005EA8FD;
inline constexpr uintptr_t ptr_free = 0x005EA8F2;

inline constexpr uintptr_t ptrKeyInput = 0x00406d60;
inline constexpr uintptr_t ptr_FindGlyphIndex = 0x004355C0;

inline constexpr uintptr_t ptrcheck_FUN_00438830 = 0x00438830;
inline constexpr uintptr_t ptrcheck_FUN_00438830_end = 0x00438b0f;

inline constexpr uintptr_t ptrKeyInputDraw = 0x0043a4d0;
inline constexpr uintptr_t ptrKeyInputDraw_end = 0x0043ab99;

inline constexpr uintptr_t ptrDrawMenu = 0x0043b4f0;
inline constexpr uintptr_t ptrDrawMenu_end = 0x0043b896;

inline constexpr uintptr_t ptrCalculateTextExtents = 0x0043BF40;
inline constexpr uintptr_t ptrCalculateTextExtents_end = 0x0043C00F;
inline constexpr uintptr_t ptrDrawTextLines = 0x0043BC80;
inline constexpr uintptr_t ptrDrawTextLines_end = 0x0043bef3;

inline constexpr uintptr_t ptrCalcExtentTextLabel = 0x004428B0;
inline constexpr uintptr_t ptrCalcExtentTextLabel_end = 0x00442980;
inline constexpr uintptr_t ptrDrawTextTextLabel = 0x00442590;
inline constexpr uintptr_t ptrDrawTextTextLabel_end = 0x00442876;

inline constexpr uintptr_t ptrWordWrap = 0x00444890;
inline constexpr uintptr_t ptrWordWrap_end = 0x00444CAB;
inline constexpr uintptr_t ptrRenderShadow = 0x00444D40;
inline constexpr uintptr_t ptrRenderShadow_end = 0x00445039;

inline constexpr uintptr_t ptrSprintfMeasure = 0x00476820;
inline constexpr uintptr_t ptrSprintfMeasure_end = 0x00476adc;




/* ------------------------------------------------------------------ */
/*  내부 함수 (원본 CRT/할당자를 그대로 써야 한다)                      */
/* ------------------------------------------------------------------ */
typedef void* (__cdecl* t_malloc)(unsigned int size);
typedef void(__cdecl* t_free)(void* p);
#define _malloc     ((t_malloc) ptr_malloc)   // malloc
#define _free       ((t_free)   ptr_free)   // free

//=========================================================

inline bool IsSpaceCh(const int charCode) {
    return (charCode >= 9 && charCode <= 0x0D) || charCode == 0x20;
}

inline bool IsHangul(const uint8_t c1, const uint8_t c2) {     // EUC-KR
    return (0xA1 <= c1 && c1 <= 0xC8 && 0xA1 <= c2 && c2 <= 0xFE);
}

inline int getCharCode(const uint8_t c1, const uint8_t c2) {     // EUC-KR
    // 새로운 완성형 한글 음절 시작 (0xA4A1 ~ 0xC8FE)
    if (IsHangul(c1, c2))
        return (c1 << 8) | c2;
    return static_cast<char>(c1);
}

inline int getCharCode(const int charCode) {     // EUC-KR
    uint8_t c1 = static_cast<uint8_t>(charCode & 0xFF);        // 현재 바이트
    uint8_t c2 = static_cast<uint8_t>((charCode >> 8) & 0xFF); // 다음 바이트
    return getCharCode(c1, c2);
}

/* ------------------------------------------------------------------ */
/*  charIdx 위치의 문자 코드를 반환 (2바이트 한글이면 합쳐서, 아니면 1바이트) */
/* ------------------------------------------------------------------ */
inline int getCharCode(const char* text, int charIdx)
{
    uint8_t c1 = static_cast<uint8_t>(text[charIdx]);
    uint8_t c2 = static_cast<uint8_t>(text[charIdx + 1]);
    return getCharCode(c1, c2);
}




struct cGlyphResult {
    unsigned int charCode;   // 0x00: 렌더링할 문자 코드
    unsigned int byteLength; // 0x04: 원본 문자열에서 읽어들인 바이트 수
    int width;               // 0x08: 글리프 너비 (Right - Left)
    int height;              // 0x0C: 글리프 높이 (Bottom - Top)
    char originalText[4];    // 0x10: 원본 문자 데이터 (널 종료 문자열 형태)
};
static_assert(offsetof(cGlyphResult, charCode) == 0x0, "");
static_assert(offsetof(cGlyphResult, byteLength) == 0x4, "");
static_assert(offsetof(cGlyphResult, width) == 0x8, "");
static_assert(offsetof(cGlyphResult, height) == 0xC, "");
static_assert(offsetof(cGlyphResult, originalText) == 0x10, "");


struct cCharCodeMap {
    int count;                 // +0x00
    int** keys;                // +0x04
    int** values;              // +0x08

    int FindGlyphIndex(int chCode) {    // 못찾으면 -1 리턴
        typedef int(__thiscall* tFn)(cCharCodeMap*, int);
        return reinterpret_cast<tFn>(ptr_FindGlyphIndex)(this, chCode);
    }
};
static_assert(offsetof(cCharCodeMap, count) == 0x0, "");
static_assert(offsetof(cCharCodeMap, keys) == 0x4, "");
static_assert(offsetof(cCharCodeMap, values) == 0x8, "");

struct cGlyph {
    int charCode;                   // +0x00
	RECT rect;                      // +0x04: x0, +0x08: y0, +0x0C: x1, +0x10: y1
    IDirectDrawSurface7* pSurface;  // +0x14,    IDirectDrawSurface*
};
static_assert(offsetof(cGlyph, charCode) == 0x0, "");
static_assert(offsetof(cGlyph, rect) == 0x4, "");
static_assert(offsetof(cGlyph, pSurface) == 0x14, "");


struct cFont_vtable {
	virtual HFONT CreateFontDIB(LPCSTR pszFaceName, DWORD dwCharSet, int nHeight) = 0;  // +0x00
	virtual bool  RasterizeGlyphs() = 0;                                                // +0x04
	virtual bool  UploadDIBToDirectDrawSurface() = 0;                                   // +0x08
	virtual bool  CreateSurface(IDirectDraw* pDD) = 0;                                  // +0x0C, pDD: IDirectDraw*
	virtual bool  Reset() = 0;                                                          // +0x10
	virtual bool  AddGlyph(RECT* pRect, int charCode) = 0;                              // +0x14, pRect: RECT*, charCode: int
	virtual void  SetSurface(IDirectDrawSurface7* pSurface) = 0;                        // +0x18, pSurface: IDirectDrawSurface*
	virtual int   GetCharHeight(int charCode) = 0;                                      // +0x1C
	virtual int   GetCharWidth(int charCode) = 0;                                       // +0x20
	virtual RECT* GetCharRect(int charCode) = 0;                                        // +0x24
	virtual IDirectDrawSurface7* GetSurface() = 0;                                      // +0x28, return IDirectDrawSurface*
};



extern RECT g_dummyZeroRect;
struct cFont {
	cFont_vtable* vtable;           // +0x00
	int glyphCount;                 // +0x04
	cGlyph** pGlyphs;               // +0x08
	cCharCodeMap* pCharMap;         // +0x0c
	int unknown_10;                 // +0x10
	IDirectDrawSurface7* pSurface;  // +0x14, IDirectDrawSurface*
	HDC hdc;                        // +0x18
	HBITMAP dibBitmap;              // +0x1c
	void* dibBits;                  // +0x20
	int atlasWidth;                 // +0x24
	int atlasHeight;                // +0x28

    __forceinline int GetCharHeight(int chCode) {
        int idx = this->pCharMap->FindGlyphIndex(chCode);
        if (idx < 0)
            return 0;
        RECT& rect = this->pGlyphs[idx]->rect; 
        return rect.bottom - rect.top;
    }
    __forceinline int GetCharWidth(int chCode) {
        int idx = this->pCharMap->FindGlyphIndex(chCode);
        if (idx < 0)
            return 0;
        RECT& rect = this->pGlyphs[idx]->rect; 
        return rect.right - rect.left;
    }
    __forceinline RECT* GetCharRect(int chCode) {
        int idx = this->pCharMap->FindGlyphIndex(chCode);
        if (idx < 0)
            return &g_dummyZeroRect;
        return &(this->pGlyphs[idx]->rect);
    }
    __forceinline IDirectDrawSurface7* GetSurface() {
        return this->pSurface;
    }
};
static_assert(offsetof(cFont, vtable) == 0x0, "");
static_assert(offsetof(cFont, glyphCount) == 0x4, "");
static_assert(offsetof(cFont, pGlyphs) == 0x8, "");
static_assert(offsetof(cFont, pCharMap) == 0xC, "");
static_assert(offsetof(cFont, pSurface) == 0x14, "");
static_assert(offsetof(cFont, hdc) == 0x18, "");
static_assert(offsetof(cFont, dibBitmap) == 0x1C, "");
static_assert(offsetof(cFont, dibBits) == 0x20, "");
static_assert(offsetof(cFont, atlasWidth) == 0x24, "");
static_assert(offsetof(cFont, atlasHeight) == 0x28, "");


struct cTextLayout {
    void* vftable;                 // +0x000
    char   pad_00[0x120];           // +0x004
    cFont* m_pFont;                 // +0x124
    char* m_pszText;               // +0x128
    char   pad_12C[4];              // +0x12C
    int    m_nAlign;                // +0x130  (==1 이면 최종 폭을 GetMaxWidth()로 강제 // 또는 ==1 이면 줄 가운데 정렬)
    char   pad_134[4];              // +0x134
    int    m_nLineCount;            // +0x138
    int* m_pLineEndIndices;       // +0x13C
    int* m_pLineWidths;           // +0x140
    int    m_nMaxLineWidth;         // +0x144
    int    m_nTotalHeight;          // +0x148
    int    m_nLineHeight;           // +0x14C
    WORD   m_wTextColor;            // +0x150  원본 픽셀이 0xFFFF 일 때 대체할 색
    WORD   unknown_152;              // +0x152
    WORD   m_wShadowColor;          // +0x154
    char   pad_156[0x0E];           // +0x156 ~ +0x163
    BYTE   m_bShadow;               // +0x164  그림자 사용 여부

    __forceinline int GetMaxWidth() {             // vtable +0x48
        typedef int(__thiscall* tGetMaxWidth)(void*);
        return ((tGetMaxWidth)(*(void***)this)[0x48 / 4])(this);
    }
    __forceinline IDirectDrawSurface7* GetTargetSurface() {   /* vtable +0x54 : 그려 넣을 대상 서피스 반환 (포인터 전체 32bit) */
        typedef IDirectDrawSurface7* (__thiscall* tFn)(void*);
        return ((tFn)(*(void***)this)[0x54 / 4])(this);
    }
};
static_assert(offsetof(cTextLayout, m_pFont) == 0x124, "");
static_assert(offsetof(cTextLayout, m_pszText) == 0x128, "");
static_assert(offsetof(cTextLayout, m_nAlign) == 0x130, "");
static_assert(offsetof(cTextLayout, m_nLineCount) == 0x138, "");
static_assert(offsetof(cTextLayout, m_pLineEndIndices) == 0x13C, "");
static_assert(offsetof(cTextLayout, m_pLineWidths) == 0x140, "");
static_assert(offsetof(cTextLayout, m_nMaxLineWidth) == 0x144, "");
static_assert(offsetof(cTextLayout, m_nTotalHeight) == 0x148, "");
static_assert(offsetof(cTextLayout, m_nLineHeight) == 0x14C, "");
static_assert(offsetof(cTextLayout, m_wTextColor) == 0x150, "");
static_assert(offsetof(cTextLayout, m_wShadowColor) == 0x154, "");
static_assert(offsetof(cTextLayout, m_bShadow) == 0x164, "");



struct cRenderTarget {            // 폭/높이를 가진 계열 (이름 임시)
    void* vftable;              // +0x00  vtable +0x54 : 서피스 반환
    char  pad_004[0x124];       // +0x04
    int   m_nWidth;             // +0x128
    int   m_nHeight;            // +0x12C
    /* ... */
    __forceinline IDirectDrawSurface7* GetTargetSurface() {   /* vtable +0x54 : 그려 넣을 대상 서피스 반환 (포인터 전체 32bit) */
        typedef IDirectDrawSurface7* (__thiscall* tFn)(void*);
        return ((tFn)(*(void***)this)[0x54 / 4])(this);
    }
};
static_assert(offsetof(cRenderTarget, m_nWidth) == 0x128, "");
static_assert(offsetof(cRenderTarget, m_nHeight) == 0x12C, "");



struct cTextManager {
    void*  vftable;             // +0x000
    char   pad_004[0x2C];       // +0x004
    int    m_nWidth30;          // +0x030  '>' 우측 정렬 기준 폭 (※ +0x128 과 다름, 확인 필요)
    char   pad_034[0xF4];       // +0x034
    int    m_nWidth;            // +0x128
    int    m_nHeight;           // +0x12C
    char   pad_130[0x0C];       // +0x130
    int    m_nLineCountB;       // +0x13C   to Calculate
    int    m_nLineHeight;       // +0x140  가장 큰 글자 높이        (출력)
    int    m_nBoxWidth;         // +0x144  가장 긴 줄 폭 + 여백     (출력)
    int    m_nTotalHeight;      // +0x148  m_nLineCount * 줄 높이   (출력)
    int    unknown_14C[0xB];    // +0x14C
    WORD   m_wNormalColor;      // +0x178
    WORD   m_wSelColor;         // +0x17A
    char   pad_17C[0x10];       // +0x17C
    char** m_ppLinesA;          // +0x18C  to Draw
    int    m_nLineCountA;       // +0x190  to Draw
    int    unknown_194;         // +0x194
    char** m_ppLinesB;          // +0x198  to Calculate
    int    unknown_19C;         // +0x19C 
    cFont* m_pFont;             // +0x1A0
    BYTE*  m_pTruncFlags;       // +0x1A4  줄별 "잘렸음" 플래그
    char   pad_1A8[0xC];        // +0x1A8
    cRenderTarget* m_pPart[2];    // +0x1B4, +0x1B8
	cRenderTarget* m_pPartBottom; // +0x1BC
    cRenderTarget* m_pTarget;   // +0x1C0
    char   pad_1C4[8];          // +0x1C4
    BYTE*  m_pSelFlags;         // +0x1CC  줄별 "선택됨" 플래그
};
static_assert(offsetof(cTextManager, m_nWidth30) == 0x30, "");
static_assert(offsetof(cTextManager, m_nWidth) == 0x128, "");
static_assert(offsetof(cTextManager, m_nHeight) == 0x12C, "");
static_assert(offsetof(cTextManager, m_nLineCountB) == 0x13C, "");
static_assert(offsetof(cTextManager, m_nLineHeight) == 0x140, "");
static_assert(offsetof(cTextManager, m_nBoxWidth) == 0x144, "");
static_assert(offsetof(cTextManager, m_nTotalHeight) == 0x148, "");
static_assert(offsetof(cTextManager, m_wNormalColor) == 0x178, "");
static_assert(offsetof(cTextManager, m_wSelColor) == 0x17A, "");
static_assert(offsetof(cTextManager, m_ppLinesA) == 0x18C, "");
static_assert(offsetof(cTextManager, m_nLineCountA) == 0x190, "");
static_assert(offsetof(cTextManager, m_ppLinesB) == 0x198, "");
static_assert(offsetof(cTextManager, m_pFont) == 0x1A0, "");
static_assert(offsetof(cTextManager, m_pTruncFlags) == 0x1A4, "");
static_assert(offsetof(cTextManager, m_pPart) == 0x1B4, "");
static_assert(offsetof(cTextManager, m_pPartBottom) == 0x1BC, "");
static_assert(offsetof(cTextManager, m_pTarget) == 0x1C0, "");
static_assert(offsetof(cTextManager, m_pSelFlags) == 0x1CC, "");



struct CWidgetBase {
    void* vftable;                  // +0x000
    char   pad_004[0x34];            // +0x004
    int    m_nX;                     // +0x038 위젯 화면 x
    int    m_nY;                     // +0x03C 위젯 화면 y
    char   pad_040[0x7C];            // +0x040 ~ +0x0BB
    RECT   m_rcScreen;               // +0x0BC (FUN_004366e0)
    RECT   m_rcFrame;                // +0x0CC
    RECT   m_rcClip;                 // +0x0DC
    void** m_pImage;                 // +0x0EC 배경 이미지 객체 (vtable+0x1C = Draw)
    int    m_nImageArg;              // +0x0F0
    BYTE   m_bVisible;               // +0x0F4
    char   pad_0F5[0x2F];            // +0x0F5 ~ +0x123

    __forceinline int GetOffsetX() { // vtable +0x84
        typedef int(__thiscall* tGetOffsetX)(CWidgetBase*);
        return ((tGetOffsetX)(*(void***)this)[0x84 / 4])(this);
    }
    __forceinline int GetOffsetY() { // vtable +0x88
        typedef int(__thiscall* tGetOffsetY)(CWidgetBase*);
        return ((tGetOffsetY)(*(void***)this)[0x88 / 4])(this);
    }
};
static_assert(offsetof(CWidgetBase, m_nX) == 0x38, "");
static_assert(offsetof(CWidgetBase, m_nY) == 0x3C, "");
static_assert(offsetof(CWidgetBase, m_rcScreen) == 0xBC, "");
static_assert(offsetof(CWidgetBase, m_rcFrame) == 0xCC, "");
static_assert(offsetof(CWidgetBase, m_rcClip) == 0xDC, "");
static_assert(offsetof(CWidgetBase, m_pImage) == 0xEC, "");
static_assert(offsetof(CWidgetBase, m_nImageArg) == 0xF0, "");
static_assert(offsetof(CWidgetBase, m_bVisible) == 0xF4, "");



struct CKeyInput {
    CWidgetBase base;
    int    m_nBgFlag;                // +0x124 0이 아니면 배경 그리기
    char   pad_128[0x0C];            // +0x128 ~ +0x133
    cFont* m_pFont;                  // +0x134
    char* m_pszText;                // +0x138
    int    m_nStopIdx;               // +0x13C 높이 초과로 그리기를 멈춘 인덱스 (-1 = 없음)
    int    m_nPadX;                  // +0x140
    int    m_nPadY;                  // +0x144
    int    m_nBoxW;                  // +0x148
    int    m_nBoxH;                  // +0x14C
    BYTE   m_bCursorOn;              // +0x150 커서 표시 여부
    char   pad_151[3];
    int    m_nUnknown154;            // +0x154 (이 함수에서 사용 안 함)
    DWORD  m_dwLastTick;             // +0x158
    int    m_aBreakCache[20];        // +0x15C ~ +0x1AB 줄바꿈 위치 캐시 (공백 인덱스, -1=없음)
    int    m_nCursorAlpha;           // +0x1AC 0~255
    BYTE   m_bCursorFalling;         // +0x1B0 1이면 알파 감소 중
    BYTE   m_bRightAlign;            // +0x1B1 1이면 넘칠 때 끝부분만 오른쪽 정렬
};
static_assert(offsetof(CKeyInput, m_nBgFlag) == 0x124, "");
static_assert(offsetof(CKeyInput, m_pFont) == 0x134, "");
static_assert(offsetof(CKeyInput, m_pszText) == 0x138, "");
static_assert(offsetof(CKeyInput, m_nStopIdx) == 0x13c, "");
static_assert(offsetof(CKeyInput, m_nPadX) == 0x140, "");
static_assert(offsetof(CKeyInput, m_nPadY) == 0x144, "");
static_assert(offsetof(CKeyInput, m_nBoxW) == 0x148, "");
static_assert(offsetof(CKeyInput, m_nBoxH) == 0x14C, "");
static_assert(offsetof(CKeyInput, m_bCursorOn) == 0x150, "");
static_assert(offsetof(CKeyInput, m_dwLastTick) == 0x158, "");
static_assert(offsetof(CKeyInput, m_aBreakCache) == 0x15C, "");
static_assert(offsetof(CKeyInput, m_nCursorAlpha) == 0x1AC, "");
static_assert(offsetof(CKeyInput, m_bCursorFalling) == 0x1B0, "");
static_assert(offsetof(CKeyInput, m_bRightAlign) == 0x1B1, "");


struct cTextLabel {                            // vtable 0x00623038 / 0x00623040 ...
    void*   vftable;                    // +0x000
    char    pad_004[0x140];             // +0x004
    int     m_nLineHeight;              // +0x144  (출력) 가장 큰 글자 높이
    int     m_nTotalHeight;             // +0x148  (출력) 줄 수 × 줄 높이
    int     m_nBoxWidth;                // +0x14C  (출력) 가장 긴 줄의 끝 x + 20
    char    pad_150[8];                 // +0x150 ~ +0x157
    int     m_nLineCount;               // +0x158
    char**  m_ppLines;                  // +0x15C
    char    pad_160[4];                 // +0x160
    cFont*  m_pFont;                    // +0x164
    WORD    m_wColor;                   // +0x168
    char    pad_16A[0x0A];              // +0x16A
    cRenderTarget*   m_pTarget;         // +0x174
    char    pad_178[0x30];              // +0x178
    int     m_nLeft;                    // +0x1A8
};

static_assert(offsetof(cTextLabel, m_nLineHeight) == 0x144, "");
static_assert(offsetof(cTextLabel, m_nTotalHeight) == 0x148, "");
static_assert(offsetof(cTextLabel, m_nBoxWidth) == 0x14C, "");
static_assert(offsetof(cTextLabel, m_nLineCount) == 0x158, "");
static_assert(offsetof(cTextLabel, m_ppLines) == 0x15C, "");
static_assert(offsetof(cTextLabel, m_pFont) == 0x164, "");
static_assert(offsetof(cTextLabel, m_wColor) == 0x168, "");
static_assert(offsetof(cTextLabel, m_pTarget) == 0x174, "");
static_assert(offsetof(cTextLabel, m_nLeft) == 0x1A8, "");

