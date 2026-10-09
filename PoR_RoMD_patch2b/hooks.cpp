#include <windows.h>
#include <vector>
#include "MinHook.h"

// -------------------------------------------------------------
// cFont 가상 인터페이스 정의
// -------------------------------------------------------------
class cFont {
public:
    virtual void Dummy00() = 0; // +0x00
    virtual void Dummy04() = 0; // +0x04
    virtual void Dummy08() = 0; // +0x08
    virtual void Dummy0C() = 0; // +0x0C
    virtual void Dummy10() = 0; // +0x10
    virtual void Dummy14() = 0; // +0x14
    virtual void Dummy18() = 0; // +0x18
    virtual int   GetCharHeight(int code) = 0; // +0x1C
    virtual int   GetCharWidth(int code) = 0; // +0x20
    virtual RECT* GetCharRect(int code) = 0; // +0x24
    virtual void* GetSurface() = 0; // +0x28
};

// -------------------------------------------------------------
// [후킹 타겟 1] 0x00444890 : CTextLayout::WordWrap
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

// 원본 함수 포인터 (MinHook 트램펄린용)
typedef BOOL(__fastcall* tWordWrap)(CTextLayout*);
tWordWrap fpOriginalWordWrap = NULL;

// 게임 엔진의 메모리 해제 함수 (0x005ea8f2)
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

        // 2바이트 한글 디코딩 (빅엔디안)
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

        // Word Wrap 조건
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

    // 엔진의 힙 할당 방식과 동일하게 할당
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
// [후킹 설치 함수]
// -------------------------------------------------------------
void InstallHooks()
{
    // 1. 폰트 함수 32비트 패치는 바이너리를 수정하지 않고 메모리 패치로 한 방에!
    DWORD oldProtect;
    // GetCharHeight (0x00435e00) -> 8B 44 24 04 90
    VirtualProtect((void*)0x00435e00, 5, PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy((void*)0x00435e00, "\x8B\x44\x24\x04\x90", 5);
    VirtualProtect((void*)0x00435e00, 5, oldProtect, &oldProtect);

    // GetCharWidth (0x00435e30) -> 8B 44 24 04 90
    VirtualProtect((void*)0x00435e30, 5, PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy((void*)0x00435e30, "\x8B\x44\x24\x04\x90", 5);
    VirtualProtect((void*)0x00435e30, 5, oldProtect, &oldProtect);

    // 2. MinHook 초기화 및 WordWrap 후킹
    if (MH_Initialize() == MH_OK)
    {
        MH_CreateHook((LPVOID)0x00444890, &Hooked_WordWrap, (LPVOID*)&fpOriginalWordWrap);
        MH_EnableHook(MH_ALL_HOOKS);
    }
}