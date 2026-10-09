#include <windows.h>
#include <vector>
#include "MinHook.h"

// -------------------------------------------------------------
// [1. 공통 폰트 인터페이스 및 헬퍼]
// -------------------------------------------------------------
struct FontGlyph {
    int  nCode;   // 문자 코드 (ASCII 또는 한글 빅엔디안 0xB0A1)
    RECT rcSrc;   // left, top, right, bottom
    int  reserved;
};

class cFont {
public:
    void** vftable;       // +0x00
    int          m_nGlyphCount; // +0x04
    FontGlyph** m_ppGlyphs;    // +0x08
    void* m_pCharMap;    // +0x0C
    int          pad10;         // +0x10
    void* m_pSurface;    // +0x14 (DirectDraw 서피스)
};

// CFontCharMap::FindGlyphIndex (0x004355c0)
typedef int(__thiscall* tFindGlyphIndex)(void* pCharMap, int charCode);
tFindGlyphIndex CFontCharMap_FindGlyphIndex = (tFindGlyphIndex)0x004355c0;

// cFont::GetCharRect (0x00435e60)
typedef RECT* (__thiscall* tGetCharRect)(cFont* pThis, int charCode);
tGetCharRect cFont_GetCharRect = (tGetCharRect)0x00435e60;

// 엔진 힙 메모리 해제 함수 (0x005ea8f2)
typedef void(__cdecl* tEngineDelete)(void*);
tEngineDelete EngineDelete = (tEngineDelete)0x005ea8f2;

// -------------------------------------------------------------
// [2. 폰트 메트릭 후킹: GetCharHeight & GetCharWidth]
// -------------------------------------------------------------
typedef int(__thiscall* tGetCharHeight)(cFont* pThis, int charCode);
tGetCharHeight fpOriginalGetCharHeight = NULL;

typedef int(__thiscall* tGetCharWidth)(cFont* pThis, int charCode);
tGetCharWidth fpOriginalGetCharWidth = NULL;

// 0x00435e00 : GetCharHeight
int __fastcall Hooked_GetCharHeight(cFont* pThis, void* /*edx*/, int charCode)
{
    int glyphIdx = CFontCharMap_FindGlyphIndex(pThis->m_pCharMap, charCode);
    if (glyphIdx != -1 && pThis->m_ppGlyphs && pThis->m_ppGlyphs[glyphIdx]) {
        FontGlyph* pGlyph = pThis->m_ppGlyphs[glyphIdx];
        return pGlyph->rcSrc.bottom - pGlyph->rcSrc.top;
    }
    return 0;
}

// 0x00435e30 : GetCharWidth
int __fastcall Hooked_GetCharWidth(cFont* pThis, void* /*edx*/, int charCode)
{
    int glyphIdx = CFontCharMap_FindGlyphIndex(pThis->m_pCharMap, charCode);
    if (glyphIdx != -1 && pThis->m_ppGlyphs && pThis->m_ppGlyphs[glyphIdx]) {
        FontGlyph* pGlyph = pThis->m_ppGlyphs[glyphIdx];
        return pGlyph->rcSrc.right - pGlyph->rcSrc.left;
    }
    return 0;
}

// -------------------------------------------------------------
// [3. CTextBox::CalculateTextExtents 후킹 (0x0043bf40)]
// (기존 Code Cave 3, 4를 완벽하게 대체)
// -------------------------------------------------------------
struct CTextBox {
    void* vftable;             // +0x00
    char   pad_00[0x138];
    int    m_nLineCount;        // +0x13C
    int    m_rcBounds_left;     // +0x140
    int    m_nTotalWidth;       // +0x144 (측정된 가로폭)
    int    m_rcBounds_top;      // +0x148
    char   pad_14C[0x40];
    char** m_ppLines;           // +0x18C 또는 +0x198 (문자열 라인 포인터 배열)
    char   pad_19C[4];
    cFont* m_pFont;             // +0x1A0 (폰트 객체)
};

typedef BOOL(__thiscall* tCalculateTextExtents)(CTextBox*);
tCalculateTextExtents fpOriginalCalculateTextExtents = NULL;

BOOL __fastcall Hooked_CalculateTextExtents(CTextBox* pThis, void* /*edx*/)
{
    int maxBoxWidth = 0;
    int maxLineHeight = 0;

    char** ppLines = *(char***)((char*)pThis + 0x198);
    cFont* pFont = *(cFont**)((char*)pThis + 0x1A0);

    if (pThis->m_nLineCount > 0 && ppLines)
    {
        for (int lineIdx = 0; lineIdx < pThis->m_nLineCount; ++lineIdx)
        {
            const char* szLine = ppLines[lineIdx];
            if (!szLine) continue;

            int currentLineWidth = 10; // 왼쪽 마진 10px

            for (int i = 0; szLine[i] != '\0'; )
            {
                unsigned char c = (unsigned char)szLine[i];
                int code = 0;
                int charBytes = 1;

                if (c >= 0x80) { // 한글 2바이트
                    code = (c << 8) | (unsigned char)szLine[i + 1];
                    charBytes = 2;
                }
                else {         // ASCII
                    code = c;
                    charBytes = 1;
                }

                int h = Hooked_GetCharHeight(pFont, NULL, code);
                if (maxLineHeight < h) maxLineHeight = h;

                int w = Hooked_GetCharWidth(pFont, NULL, code);
                currentLineWidth += w;

                if (maxBoxWidth < currentLineWidth + 20) {
                    maxBoxWidth = currentLineWidth + 20;
                }

                i += charBytes; // 한글은 +2, ASCII는 +1
            }
        }
    }

    pThis->m_rcBounds_left = 0;
    pThis->m_nTotalWidth = maxBoxWidth;
    pThis->m_rcBounds_top = 0;

    return TRUE;
}

// -------------------------------------------------------------
// [4. CTextLayout::WordWrap 후킹 (0x00444890)]
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

        if (c >= 0x80) { // 한글 2바이트
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
// [5. CTextBox::DrawTextLines (0x0043bc80) 인라인 마이크로 패치]
// (Cave 1, 2 대신 DLL 로드 시 런타임 메모리 패치로 한 방에 해결)
// -------------------------------------------------------------
void ApplyDrawTextInlinePatch()
{
    // 점프 대상: 0x00613500 (Cave 1), 0x00613600 (Cave 2)
    // 메모리 보호 해제 후 우리가 앞서 검증한 Cave 바이트를 DLL이 직접 써넣습니다!
    DWORD oldProtect;
    VirtualProtect((void*)0x0043bd9b, 0x100, PAGE_EXECUTE_READWRITE, &oldProtect);

    // Cave 1 점프 설치 (0x0043bd9b)
    memcpy((void*)0x0043bd9b, "\xE9\x60\x77\x1D\x00\x90", 6);

    // Cave 2 점프 설치 (0x0043be72)
    memcpy((void*)0x0043be72, "\xE9\x89\x77\x1D\x00\x90\x90\x90\x90\x90\x90\x90\x90\x90", 14);

    // .text 섹션 끝 빈 공간에 Cave 본체 쓰기
    VirtualProtect((void*)0x00613500, 0x200, PAGE_EXECUTE_READWRITE, &oldProtect);

    // Cave 1 코드
    const unsigned char cave1[] = {
        0x0F, 0xB6, 0x14, 0x18, 0x80, 0xFA, 0x80, 0x72, 0x0F, 0x0F, 0xB6, 0x44, 0x18, 0x01,
        0xC1, 0xE2, 0x08, 0x0B, 0xD0, 0x8B, 0xC2, 0xEB, 0x03, 0x90, 0x0F, 0xBE, 0xC2, 0x8B,
        0x11, 0xE9, 0x7F, 0x88, 0xE2, 0xFF
    };
    memcpy((void*)0x00613500, cave1, sizeof(cave1));

    // Cave 2 코드
    const unsigned char cave2[] = {
        0x51, 0x0F, 0xB6, 0x14, 0x18, 0x80, 0xFA, 0x80, 0x72, 0x16, 0x0F, 0xB6, 0x44, 0x18,
        0x01, 0xC1, 0xE2, 0x08, 0x0B, 0xD0, 0x52, 0x83, 0xC3, 0x02, 0x8B, 0x8E, 0xA0, 0x01,
        0x00, 0x00, 0xEB, 0x0D, 0x0F, 0xBE, 0xC2, 0x50, 0x43, 0x8B, 0x8E, 0xA0, 0x01, 0x00,
        0x00, 0xEB, 0x00, 0x8B, 0x11, 0xFF, 0x52, 0x20, 0x59, 0x8B, 0x4C, 0x24, 0x30, 0x03,
        0xC8, 0xE9, 0x42, 0x88, 0xE2, 0xFF
    };
    memcpy((void*)0x00613600, cave2, sizeof(cave2));
}

// -------------------------------------------------------------
// [6. 통합 후킹 설치 진입점]
// -------------------------------------------------------------
void InstallHooks()
{
    // 1. MinHook 초기화
    if (MH_Initialize() == MH_OK)
    {
        // 폰트 메트릭 후킹
        MH_CreateHook((LPVOID)0x00435e00, &Hooked_GetCharHeight, (LPVOID*)&fpOriginalGetCharHeight);
        MH_CreateHook((LPVOID)0x00435e30, &Hooked_GetCharWidth, (LPVOID*)&fpOriginalGetCharWidth);

        // 텍스트 박스 크기 측정 후킹 (Cave 3, 4 대체)
        MH_CreateHook((LPVOID)0x0043bf40, &Hooked_CalculateTextExtents, (LPVOID*)&fpOriginalCalculateTextExtents);

        // 단어 자동 줄바꿈 후킹
        MH_CreateHook((LPVOID)0x00444890, &Hooked_WordWrap, (LPVOID*)&fpOriginalWordWrap);

        // 모든 MinHook 활성화
        MH_EnableHook(MH_ALL_HOOKS);
    }

    // 2. 화면 그리기 루프 인라인 패치 적용 (Cave 1, 2 메모리 자동 주입)
    ApplyDrawTextInlinePatch();
}