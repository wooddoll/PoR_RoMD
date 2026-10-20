#pragma once

#include <intrin.h>
#include <cstdint>
#define WIN32_LEAN_AND_MEAN // 불필요한 Windows 헤더 제외 (선택 사항)
#include <windows.h>
#include <ddraw.h>          // IDirectDrawSurface7 및 DirectDraw 핵심 헤더



#define _LOG_   0
#if _LOG_
void GetCurrentCall(const int charCode, const uintptr_t caller);
#define CHECK(charcode) do { GetCurrentCall(charcode, reinterpret_cast<uintptr_t>(_ReturnAddress())); } while(0)
#else
#define CHECK(charcode) ((void)0)
#endif
/* ------------------------------------------------------------------ */
/*  내부 함수 (원본 CRT/할당자를 그대로 써야 한다)                      */
/* ------------------------------------------------------------------ */
typedef void* (__cdecl* t_malloc)(unsigned int size);
typedef void(__cdecl* t_free)(void* p);
#define _malloc     ((t_malloc) 0x005EA8FD)   // malloc
#define _free       ((t_free)   0x005EA8F2)   // free

/* FUN_00436180(0x29, 0x9f, 0xdf) : 호출 직후 AX 를 16bit 픽셀로 사용.
   cdecl, 인자 3개. 이름은 임시 (RGB → 현재 픽셀 포맷 변환으로 추정). */
typedef WORD(__cdecl* tMakeColor16)(int a, int b, int c);
#define game_MakeColor16 ((tMakeColor16)0x00436180)


//=========================================================

inline bool IsSpaceCh(const int charCode) {
  if (((charCode < 9) || (0xd < charCode)) && (charCode != 0x20)) {
    return false;
  }
  return true;
}

inline bool IsHangul(const uint8_t c1, const uint8_t c2) {     // EUC-KR
    return (c1 >= 0xA4 && c1 <= 0xC8 && c2 >= 0xA1 && c2 <= 0xFE);
}

inline int getCharCode(const uint8_t c1, const uint8_t c2) {     // EUC-KR
    // 새로운 완성형 한글 음절 시작 (0xA4A1 ~ 0xC8FE)
    if (IsHangul(c1, c2))
        return (c1 << 8) | c2;
    return static_cast<char>(c1);
}

inline int getCharCode(const int charCode) {     // EUC-KR
    uint8_t c1 = static_cast<uint8_t>(charCode & 0xFF);        // 현재 바이트
    uint8_t c2 = static_cast<uint8_t>((charCode >> 8) & 0xFF); // 다음 바이트 (4바이트 읽기)
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


struct cCharCodeMap {
    int count;                 // +0x00
    int** keys;                // +0x04
    int** values;              // +0x08

    int FindGlyphIndex(int chCode) {    // 못찾으면 -1 리턴
        typedef int(__thiscall* tFn)(cCharCodeMap*, int);
        return reinterpret_cast<tFn>(0x004355C0)(this, chCode);
    }
};

struct cGlyph {
    int charCode;                   // +0x00
	RECT rect;                      // +0x04: x0, +0x08: y0, +0x0C: x1, +0x10: y1
    IDirectDrawSurface7* pSurface;  // +0x14,    IDirectDrawSurface*
};

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

    int GetCharHeight(int chCode) {
        CHECK(chCode);
        int idx = this->pCharMap->FindGlyphIndex(chCode);
        if (idx < 0)
            return 0;
        RECT& rect = this->pGlyphs[idx]->rect; 
        return rect.bottom - rect.top;
    }
    int GetCharWidth(int chCode) {
        CHECK(chCode);
        int idx = this->pCharMap->FindGlyphIndex(chCode);
        if (idx < 0)
            return 0;
        RECT& rect = this->pGlyphs[idx]->rect; 
        return rect.right - rect.left;
    }
    RECT* GetCharRect(int chCode) {
        CHECK(chCode);
        int idx = this->pCharMap->FindGlyphIndex(chCode);
        if (idx < 0)
            return &g_dummyZeroRect;
        return &(this->pGlyphs[idx]->rect);
    }
    IDirectDrawSurface7* GetSurface() {
        return this->pSurface;
    }
};



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

    int GetMaxWidth() {             // vtable +0x48
        typedef int(__thiscall* tGetMaxWidth)(void*);
        return ((tGetMaxWidth)(*(void***)this)[0x48 / 4])(this);
    }
    IDirectDrawSurface7* GetTargetSurface() {   /* vtable +0x54 : 그려 넣을 대상 서피스 반환 (포인터 전체 32bit) */
        typedef IDirectDrawSurface7* (__thiscall* tFn)(void*);
        return ((tFn)(*(void***)this)[0x54 / 4])(this);
    }
};

struct cSpriteBase {            // 폭/높이를 가진 계열 (이름 임시)
    void* vftable;              // +0x00  vtable +0x54 : 서피스 반환
    char  pad_004[0x124];       // +0x04
    int   m_nWidth;             // +0x128
    int   m_nHeight;            // +0x12C
    /* ... */
    IDirectDrawSurface7* GetTargetSurface() {   /* vtable +0x54 : 그려 넣을 대상 서피스 반환 (포인터 전체 32bit) */
        typedef IDirectDrawSurface7* (__thiscall* tFn)(void*);
        return ((tFn)(*(void***)this)[0x54 / 4])(this);
    }
};

struct cTextManager {
    void*  vftable;             // +0x000
    char   pad_004[0x124];      // +0x004
    int    m_nWidth;            // +0x128
    int    m_nHeight;           // +0x12C
    char   pad_130[0x0C];       // +0x130
    int    m_nLineCountB;       // +0x13C   to Calculate
    int    m_nLineHeight;       // +0x140  가장 큰 글자 높이        (출력)
    int    m_nBoxWidth;         // +0x144  가장 긴 줄 폭 + 여백     (출력)
    int    m_nTotalHeight;      // +0x148  m_nLineCount * 줄 높이   (출력)
    int    unknown_14C[16];     // +0x14C ~ +0x18B
    char** m_ppLinesA;          // +0x18C  to Draw
    int    m_nLineCountA;       // +0x190  to Draw
    int    unknown_194;         // +0x194
    char** m_ppLinesB;          // +0x198  to Calculate
    int    unknown_19C;         // +0x19C 
    cFont* m_pFont;             // +0x1A0
    char   pad_1A4[0x10];       // +0x1A4
    cSpriteBase* m_pPart[3];    // +0x1B4, +0x1B8, +0x1BC
    cSpriteBase* m_pTarget;     // +0x1C0
};

