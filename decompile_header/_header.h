struct CharCodeMap {
    int count;                 // +0x00
    int **keys;                // +0x04
    int **values;              // +0x08
};

struct Glyph {
    int charCode;              // +0x00
    int left;                  // +0x04
    int top;                   // +0x08
    int right;                 // +0x0c
    int bottom;                // +0x10
    IDirectDrawSurface *surface;             // +0x14
};

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

    int canvasWidth;            // +0x24
    int canvasHeight;           // +0x28
};

HFONT __thiscall cFont_CreateFontDIB_004356f0(cFont *this,LPCSTR pszFaceName,DWORD dwCharSet,int nHeight);	0
BOOL __thiscall cFont_RasterizeGlyphs_00435950(cFont *this);												4
BOOL __thiscall cFont_UploadDIBToDirectDrawSurface_004359b0(cFont *this)									8
BOOL __thiscall cFont_CreateSurface_00435c10(cFont *this,IDirectDraw *pDD)									c
BOOL __thiscall cFont_Reset_00435c70(cFont *this,LPCSTR pszFaceName,DWORD dwCharSet,int nHeight)			10
BOOL __thiscall cFont_AddGlyph_00435d00(cFont *this,RECT *pRect,int charCode)								14
void __thiscall cFont_SetSurface_00435dd0(cFont *this,IDirectDrawSurface *pSurface)							18
int __thiscall cFont_GetCharHeight_00435e00(cFont *this,char charCode)
int __thiscall cFont_GetCharWidth_00435e30(cFont *this,char charCode)
RECT * __thiscall cFont_GetCharRect_00435e60(cFont *this,int charCode)
IDirectDrawSurface * __thiscall cFont_GetSurface_005484e0(cFont *this)
