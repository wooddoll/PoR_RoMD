#include <windows.h>
#include <vector>
#include "MinHook.h"

// -------------------------------------------------------------
// [구조체 정의]
// -------------------------------------------------------------
struct FontGlyph {
    int  nCode;   // 문자 코드 (ASCII or 한글 빅엔디안)
    RECT rcSrc;   // left, top, right, bottom
    int  reserved;
};

// cFont 클래스 메모리 레이아웃
class cFont {
public:
    void** vftable;       // +0x00
    int          m_nGlyphCount; // +0x04
    FontGlyph** m_ppGlyphs;    // +0x08
    void* m_pCharMap;    // +0x0C
    // ...
};

// -------------------------------------------------------------
// [함수 타입 정의 (thiscall)]
// -------------------------------------------------------------
// cFont::GetCharHeight (0x00435e00)
typedef int(__thiscall* tGetCharHeight)(cFont* pThis, int charCode);
tGetCharHeight fpOriginalGetCharHeight = NULL;

// cFont::GetCharWidth (0x00435e30)
typedef int(__thiscall* tGetCharWidth)(cFont* pThis, int charCode);
tGetCharWidth fpOriginalGetCharWidth = NULL;

// 내부 매핑 탐색 함수: CFontCharMap::FindGlyphIndex (0x004355c0)
// cFontCharMap 포인터(ECX)와 문자 코드(int)를 받아 글리프 배열 인덱스를 반환
typedef int(__thiscall* tFindGlyphIndex)(void* pCharMap, int charCode);
tFindGlyphIndex CFontCharMap_FindGlyphIndex = (tFindGlyphIndex)0x004355c0;


// -------------------------------------------------------------
// [후킹 함수 구현]
// -------------------------------------------------------------

// 1. GetCharHeight 후킹 (0x00435e00)
// 원본은 char(1바이트)로 잘라먹었지만, 후킹 함수에서는 온전한 32비트 int로 처리!
int __fastcall Hooked_GetCharHeight(cFont* pThis, void* /*edx*/, int charCode)
{
    // 글리프 인덱스 검색 (32비트 한글 코드 그대로 전달)
    int glyphIdx = CFontCharMap_FindGlyphIndex(pThis->m_pCharMap, charCode);

    if (glyphIdx != -1 && pThis->m_ppGlyphs && pThis->m_ppGlyphs[glyphIdx])
    {
        FontGlyph* pGlyph = pThis->m_ppGlyphs[glyphIdx];
        return pGlyph->rcSrc.bottom - pGlyph->rcSrc.top; // height = bottom - top
    }

    // 매핑 테이블에 없는 글자일 경우 기본값(또는 0) 반환
    return 0;
}

// 2. GetCharWidth 후킹 (0x00435e30)
int __fastcall Hooked_GetCharWidth(cFont* pThis, void* /*edx*/, int charCode)
{
    // 글리프 인덱스 검색
    int glyphIdx = CFontCharMap_FindGlyphIndex(pThis->m_pCharMap, charCode);

    if (glyphIdx != -1 && pThis->m_ppGlyphs && pThis->m_ppGlyphs[glyphIdx])
    {
        FontGlyph* pGlyph = pThis->m_ppGlyphs[glyphIdx];
        return pGlyph->rcSrc.right - pGlyph->rcSrc.left; // width = right - left
    }

    // 공백 문자(' ')이거나 못 찾은 경우 기본 폭(예: 8) 또는 0 반환
    return 0;
}


// -------------------------------------------------------------
// [기존 WordWrap 함수 및 CTextLayout 구조체]
// -------------------------------------------------------------
struct CTextLayout {
    void* vftable;             // +0x00
    char   pad_00[0x120];
    cFont* m_pFont;             // +0x124
    char* m_pszText;           // +0x128
    char   pad_12C[4];
    int    m_nUnknown130;       // +0x130
    char   pad_134[4];
    int    m_nLineCount;        // +0x138
    int* m_pLineEndIndices;   // +0x13C
    int* m_pLineWidths;       // +0x140
    int    m_nMaxLineWidth;     // +0x144
    int    m_nTotalHeight;      // +0x148

    int GetMaxWidth() {
        typedef int(__thiscall* tGetMaxWidth)(void*);
        return ((tGetMaxWidth)(*(void***)this)[0x48 / 4])(this);
    }
};

typedef BOOL(__fastcall* tWordWrap)(CTextLayout*);
tWordWrap fpOriginalWordWrap = NULL;

typedef void(__cdecl* tEngineDelete)(void*);
tEngineDelete EngineDelete = (tEngineDelete)0x005ea8f2;

BOOL __fastcall Hooked_WordWrap(CTextLayout* pThis)
{
    int nBoxMaxWidth = pThis->GetMaxWidth();
    const char* szText = pThis->m_pszText;

    pThis->m_nTotalHeight = 0;
    pThis->m_nMaxLineWidth = 0;

    if (!szText || szText[0] == '\0') {
        pThis->m_nLineCount = 0;
        return TRUE;
    }

    std::vector<int> lineEndIndices;
    std::vector<int> lineWidths;

    int currentLineWidth = 0;
    int maxLineWidth = 0;
    int lineStartIdx = 0;
    int lastSpaceIdx = -1;
    int widthAtLastSpace = 0;

    int i = 0;
    while (szText[i] != '\0')
    {
        if (szText[i] == '\n')
        {
            lineEndIndices.push_back(i - 1);
            lineWidths.push_back(currentLineWidth);
            if (maxLineWidth < currentLineWidth) maxLineWidth = currentLineWidth;

            currentLineWidth = 0;
            lastSpaceIdx = -1;
            i++;
            lineStartIdx = i;
            continue;
        }

        unsigned char c = (unsigned char)szText[i];
        int code = 0;
        int charBytes = 1;

        if (c >= 0x80) { // 한글 2바이트 (빅엔디안)
            code = (c << 8) | (unsigned char)szText[i + 1];
            charBytes = 2;
        }
        else {         // ASCII
            code = c;
            charBytes = 1;
        }

        if (code == ' ') {
            lastSpaceIdx = i;
            widthAtLastSpace = currentLineWidth;
        }

        // 우리가 후킹한 GetCharWidth 함수 호출 (또는 직접 호출)
        int charW = Hooked_GetCharWidth(pThis->m_pFont, NULL, code);

        if (currentLineWidth + charW > nBoxMaxWidth && i > lineStartIdx)
        {
            if (lastSpaceIdx != -1 && lastSpaceIdx > lineStartIdx) {
                lineEndIndices.push_back(lastSpaceIdx);
                lineWidths.push_back(widthAtLastSpace);
                if (maxLineWidth < widthAtLastSpace) maxLineWidth = widthAtLastSpace;
                i = lastSpaceIdx + 1;
            }
            else {
                lineEndIndices.push_back(i - 1);
                lineWidths.push_back(currentLineWidth);
                if (maxLineWidth < currentLineWidth) maxLineWidth = currentLineWidth;
            }

            currentLineWidth = 0;
            lastSpaceIdx = -1;
            lineStartIdx = i;
            continue;
        }

        currentLineWidth += charW;
        i += charBytes;
    }

    lineEndIndices.push_back(i - 1);
    lineWidths.push_back(currentLineWidth);
    if (maxLineWidth < currentLineWidth) maxLineWidth = currentLineWidth;

    int lineCount = (int)lineEndIndices.size();
    pThis->m_nLineCount = lineCount;
    pThis->m_nMaxLineWidth = maxLineWidth;

    int* pNewEndIndices = new int[lineCount];
    int* pNewWidths = new int[lineCount];

    memcpy(pNewEndIndices, lineEndIndices.data(), lineCount * sizeof(int));
    memcpy(pNewWidths, lineWidths.data(), lineCount * sizeof(int));

    if (pThis->m_pLineEndIndices) EngineDelete(pThis->m_pLineEndIndices);
    if (pThis->m_pLineWidths)     EngineDelete(pThis->m_pLineWidths);

    pThis->m_pLineEndIndices = pNewEndIndices;
    pThis->m_pLineWidths = pNewWidths;

    return TRUE;
}


// -------------------------------------------------------------
// [후킹 설치 진입점]
// -------------------------------------------------------------
void InstallHooks()
{
    if (MH_Initialize() == MH_OK)
    {
        // 1. GetCharHeight 후킹 (0x00435e00)
        MH_CreateHook((LPVOID)0x00435e00, &Hooked_GetCharHeight, (LPVOID*)&fpOriginalGetCharHeight);

        // 2. GetCharWidth 후킹 (0x00435e30)
        MH_CreateHook((LPVOID)0x00435e30, &Hooked_GetCharWidth, (LPVOID*)&fpOriginalGetCharWidth);

        // 3. WordWrap 후킹 (0x00444890)
        MH_CreateHook((LPVOID)0x00444890, &Hooked_WordWrap, (LPVOID*)&fpOriginalWordWrap);

        // 모든 후킹 활성화
        MH_EnableHook(MH_ALL_HOOKS);
    }
}