#include <windows.h>
#include <vector>
#include "MinHook.h"

// -------------------------------------------------------------
// [1. 공통 폰트 인터페이스]
// -------------------------------------------------------------
class cFont {
public:
    virtual void  Dummy00() = 0;
    virtual void  Dummy04() = 0;
    virtual void  Dummy08() = 0;
    virtual void  Dummy0C() = 0;
    virtual void  Dummy10() = 0;
    virtual void  Dummy14() = 0;
    virtual void  Dummy18() = 0;
    virtual int   GetCharHeight(int code) = 0; // +0x1C (메모리 패치로 32비트 완벽 지원)
    virtual int   GetCharWidth(int code) = 0; // +0x20 (메모리 패치로 32비트 완벽 지원)
    virtual RECT* GetCharRect(int code) = 0; // +0x24
    virtual void* GetSurface() = 0; // +0x28
};

typedef void(__cdecl* tEngineDelete)(void*);
tEngineDelete EngineDelete = (tEngineDelete)0x005ea8f2;

// -------------------------------------------------------------
// [2. 복잡한 로직 MinHook 후킹: CalculateTextExtents (0x0043bf40)]
// -------------------------------------------------------------
struct CTextBox {
    void* vftable;             // +0x00
    char   pad_00[0x138];
    int    m_nLineCount;        // +0x13C
    int    m_rcBounds_left;     // +0x140
    int    m_nTotalWidth;       // +0x144
    int    m_rcBounds_top;      // +0x148
    char   pad_14C[0x40];
    char** m_ppLines;           // +0x18C 또는 +0x198
    char   pad_19C[4];
    cFont* m_pFont;             // +0x1A0
};

typedef BOOL(__thiscall* tCalculateTextExtents)(CTextBox*);
tCalculateTextExtents fpOriginalCalculateTextExtents = NULL;

BOOL __fastcall Hooked_CalculateTextExtents(CTextBox* pThis, void* /*edx*/)
{
    int maxBoxWidth = 0;
    int maxLineHeight = 0;

    char** ppLines = *(char***)((char*)pThis + 0x198);
    cFont* pFont = *(cFont**)((char*)pThis + 0x1A0);

    if (pThis->m_nLineCount > 0 && ppLines && pFont)
    {
        for (int lineIdx = 0; lineIdx < pThis->m_nLineCount; ++lineIdx)
        {
            const char* szLine = ppLines[lineIdx];
            if (!szLine) continue;

            int currentLineWidth = 10;

            for (int i = 0; szLine[i] != '\0'; )
            {
                unsigned char c = (unsigned char)szLine[i];
                int code = 0;
                int charBytes = 1;

                if (c >= 0x80) { // 한글 2바이트 (빅엔디안)
                    code = (c << 8) | (unsigned char)szLine[i + 1];
                    charBytes = 2;
                }
                else {         // ASCII
                    code = c;
                    charBytes = 1;
                }

                // 메모리 패치된 원본 폰트 가상 함수를 직접 호출!
                int h = pFont->GetCharHeight(code);
                if (maxLineHeight < h) maxLineHeight = h;

                int w = pFont->GetCharWidth(code);
                currentLineWidth += w;

                if (maxBoxWidth < currentLineWidth + 20) {
                    maxBoxWidth = currentLineWidth + 20;
                }

                i += charBytes;
            }
        }
    }

    pThis->m_rcBounds_left = 0;
    pThis->m_nTotalWidth = maxBoxWidth;
    pThis->m_rcBounds_top = 0;

    return TRUE;
}

// -------------------------------------------------------------
// [3. 복잡한 로직 MinHook 후킹: WordWrap (0x00444890)]
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

        if (c >= 0x80) {
            code = (c << 8) | (unsigned char)szText[i + 1];
            charBytes = 2;
        }
        else {
            code = c;
            charBytes = 1;
        }

        if (code == ' ') {
            lastSpaceIdx = i;
            widthAtLastSpace = currentLineWidth;
        }

        int charW = pThis->m_pFont->GetCharWidth(code);

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
// [4. 전체 단순 메모리 패치 (Memory Patching)]
// -------------------------------------------------------------
void ApplyMemoryPatches()
{
    DWORD oldProtect;

    // A. cFont::GetCharHeight (0x00435e00) -> 8B 44 24 04 90 (32비트화)
    VirtualProtect((void*)0x00435e00, 5, PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy((void*)0x00435e00, "\x8B\x44\x24\x04\x90", 5);
    VirtualProtect((void*)0x00435e00, 5, oldProtect, &oldProtect);

    // B. cFont::GetCharWidth (0x00435e30) -> 8B 44 24 04 90 (32비트화)
    VirtualProtect((void*)0x00435e30, 5, PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy((void*)0x00435e30, "\x8B\x44\x24\x04\x90", 5);
    VirtualProtect((void*)0x00435e30, 5, oldProtect, &oldProtect);

    // C. CTextBox::DrawTextLines (0x0043bc80) 내부 점프 패치
    VirtualProtect((void*)0x0043bd9b, 0x100, PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy((void*)0x0043bd9b, "\xE9\x60\x77\x1D\x00\x90", 6);                               // Cave 1 점프
    memcpy((void*)0x0043be72, "\xE9\x89\x77\x1D\x00\x90\x90\x90\x90\x90\x90\x90\x90\x90", 14); // Cave 2 점프
    VirtualProtect((void*)0x0043bd9b, 0x100, oldProtect, &oldProtect);

    // D. Code Cave 본체 쓰기 (0x00613500)
    VirtualProtect((void*)0x00613500, 0x200, PAGE_EXECUTE_READWRITE, &oldProtect);

    const unsigned char cave1[] = {
        0x0F, 0xB6, 0x14, 0x18, 0x80, 0xFA, 0x80, 0x72, 0x0F, 0x0F, 0xB6, 0x44, 0x18, 0x01,
        0xC1, 0xE2, 0x08, 0x0B, 0xD0, 0x8B, 0xC2, 0xEB, 0x03, 0x90, 0x0F, 0xBE, 0xC2, 0x8B,
        0x11, 0xE9, 0x7F, 0x88, 0xE2, 0xFF
    };
    memcpy((void*)0x00613500, cave1, sizeof(cave1));

    const unsigned char cave2[] = {
        0x51, 0x0F, 0xB6, 0x14, 0x18, 0x80, 0xFA, 0x80, 0x72, 0x16, 0x0F, 0xB6, 0x44, 0x18,
        0x01, 0xC1, 0xE2, 0x08, 0x0B, 0xD0, 0x52, 0x83, 0xC3, 0x02, 0x8B, 0x8E, 0xA0, 0x01,
        0x00, 0x00, 0xEB, 0x0D, 0x0F, 0xBE, 0xC2, 0x50, 0x43, 0x8B, 0x8E, 0xA0, 0x01, 0x00,
        0x00, 0xEB, 0x00, 0x8B, 0x11, 0xFF, 0x52, 0x20, 0x59, 0x8B, 0x4C, 0x24, 0x30, 0x03,
        0xC8, 0xE9, 0x42, 0x88, 0xE2, 0xFF
    };
    memcpy((void*)0x00613600, cave2, sizeof(cave2));
    VirtualProtect((void*)0x00613500, 0x200, oldProtect, &oldProtect);
}

// -------------------------------------------------------------
// [5. 최종 초기화 진입점]
// -------------------------------------------------------------
void InstallHooks()
{
    // 단순 메모리 패치 적용
    ApplyMemoryPatches();

    // 복잡한 텍스트 레이아웃만 MinHook으로 후킹
    if (MH_Initialize() == MH_OK)
    {
        MH_CreateHook((LPVOID)0x0043bf40, &Hooked_CalculateTextExtents, (LPVOID*)&fpOriginalCalculateTextExtents);
        MH_CreateHook((LPVOID)0x00444890, &Hooked_WordWrap, (LPVOID*)&fpOriginalWordWrap);
        MH_EnableHook(MH_ALL_HOOKS);
    }
}