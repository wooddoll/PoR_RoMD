#pragma once

#include <windows.h>

struct cCharCodeMap {
    int count;                 // +0x00
    int** keys;                // +0x04
    int** values;              // +0x08
};

struct cGlyph {
    int charCode;              // +0x00
	RECT rect;                 // +0x04: x0, +0x08: y0, +0x0C: x1, +0x10: y1
    void* surface;             // +0x14,    IDirectDrawSurface*
};

class cFont_vtable {
public:
	virtual HFONT CreateFontDIB(LPCSTR pszFaceName, DWORD dwCharSet, int nHeight) = 0;  // +0x00
	virtual bool  RasterizeGlyphs() = 0;                                                // +0x04
	virtual bool  UploadDIBToDirectDrawSurface() = 0;                                   // +0x08
	virtual bool  CreateSurface(void* pDD) = 0;                                         // +0x0C, pDD: IDirectDraw*
	virtual bool  Reset() = 0;                                                          // +0x10
	virtual bool  AddGlyph(RECT* pRect, int charCode) = 0;                              // +0x14, pRect: RECT*, charCode: int
	virtual void  SetSurface(void* pSurface) = 0;                                       // +0x18, pSurface: IDirectDrawSurface*
	virtual int   GetCharHeight(int charCode) = 0;                                      // +0x1C
	virtual int   GetCharWidth(int charCode) = 0;                                       // +0x20
	virtual RECT* GetCharRect(int charCode) = 0;                                        // +0x24
	virtual void* GetSurface() = 0;                                                     // +0x28, return IDirectDrawSurface*
};

struct cFont {
	cFont_vtable* vtable;       // +0x00
	int glyphCount;             // +0x04
	cGlyph** pGlyphs;           // +0x08
	cCharCodeMap* pCharMap;     // +0x0c
	int unknown_10;             // +0x10
	void* pSurface;             // +0x14, IDirectDrawSurface*
	HDC hdc;                    // +0x18
	HBITMAP dibBitmap;          // +0x1c
	void* dibBits;              // +0x20
	int atlasWidth;             // +0x24
	int atlasHeight;            // +0x28
};

struct cTextBox {
    void* vftable;              // +0x00
    char   pad_00[0x138];       // +0x04 ~ +0x13C
    int    m_nLineCount;        // +0x13C
    int    m_rcBounds_left;     // +0x140
    int    m_nTotalWidth;       // +0x144
    int    m_rcBounds_top;      // +0x148
    char   pad_14C[0x40];       // +0x14C ~ +0x18C
    char** m_ppLines;           // +0x18C
    char   pad_190[0x10];       // +0x190 ~ +0x1A0
    cFont* m_pFont;             // +0x1A0
};


struct cTextLayout {
    void* vftable;              // +0x00
    char   pad_00[0x120];       // +0x04 ~ +0x124
    cFont* m_pFont;             // +0x124
    char* m_pszText;            // +0x128
    char   pad_12C[4];          // +0x12C
    int    m_nUnknown130;       // +0x130
    char   pad_134[4];          // +0x134
    int    m_nLineCount;        // +0x138
    int* m_pLineEndIndices;     // +0x13C
    int* m_pLineWidths;         // +0x140
    int    m_nMaxLineWidth;     // +0x144
    int    m_nTotalHeight;      // +0x148

    int GetMaxWidth() {
        typedef int(__thiscall* tGetMaxWidth)(void*);
        return ((tGetMaxWidth)(*(void***)this)[0x12])(this);
    }
};
