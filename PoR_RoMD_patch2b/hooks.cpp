#include <windows.h>
#include <vector>
#include "MinHook.h"


// 폰트 인터페이스
class cFont {
public:
    virtual void  Dummy00() = 0;
    virtual void  Dummy04() = 0;
    virtual void  Dummy08() = 0;
    virtual void  Dummy0C() = 0;
    virtual void  Dummy10() = 0;
    virtual void  Dummy14() = 0;
    virtual void  Dummy18() = 0;
    virtual int   GetCharHeight(int code) = 0; // +0x1C
    virtual int   GetCharWidth(int code) = 0; // +0x20
    virtual RECT* GetCharRect(int code) = 0; // +0x24
    virtual void* GetSurface() = 0; // +0x28
};

typedef void* (__cdecl* tEngineNew)(size_t size);
tEngineNew EngineNew = (tEngineNew)0x005ea8fd;
typedef void(__cdecl* tEngineDelete)(void* ptr);
tEngineDelete EngineDelete = (tEngineDelete)0x005ea8f2;

// [후킹 1] CTextBox::CalculateTextExtents (0x0043bf40)
struct CTextBox {
    void* vftable;
    char   pad_00[0x138];
    int    m_nLineCount;
    int    m_rcBounds_left;
    int    m_nTotalWidth;
    int    m_rcBounds_top;
    char   pad_14C[0x40];
    char** m_ppLines;
    char   pad_19C[4];
    cFont* m_pFont;
};

typedef BOOL(__thiscall* tCalculateTextExtents)(CTextBox*);
tCalculateTextExtents fpOriginalCalculateTextExtents = NULL;

inline bool IsKoreanLeadByte(unsigned char c1, unsigned char c2)
{
    // CP949 한글 범위 검사: 완성형 2350자
    // 첫 바이트가 0xB0~0xC8 이고, 두 번째 바이트도 유효한 범위(0xA1~0xFE)일 때만 진짜 한글!
    return (0xB0 <= c1 && c1 <= 0xC8) && (0xA1 <= c2 && c2 <= 0xFE);
}

BOOL __fastcall Hooked_CalculateTextExtents(CTextBox* pThis, void* /*edx*/)
{
    if (!pThis) return FALSE;

    int maxBoxWidth = 0;
    int maxLineHeight = 0;

    int lineCount = *(int*)((char*)pThis + 0x13C);
    char** ppLines = *(char***)((char*)pThis + 0x198);
    cFont* pFont = *(cFont**)((char*)pThis + 0x1A0);

    if (lineCount > 0 && ppLines && pFont)
    {
        for (int lineIdx = 0; lineIdx < lineCount; ++lineIdx)
        {
            const char* szLine = ppLines[lineIdx];
            if (!szLine) continue;

            int currentLineWidth = 10; // 초기 마진 10px

            for (int i = 0; szLine[i] != '\0'; )
            {
                int code = 0;
                int charBytes = 1;

                unsigned char c1 = (unsigned char)szLine[i];
                unsigned char c2 = (unsigned char)szLine[i + 1];

                if (IsKoreanLeadByte(c1, c2)) {
                    code = (c1 << 8) | c2;
                    charBytes = 2;
                }
                else {
                    code = (char)c1;
                    charBytes = 1;
                }

                // 글자 높이 측정 및 최대 라인 높이 갱신
                int h = pFont->GetCharHeight(code);
                if (maxLineHeight < h) {
                    maxLineHeight = h;
                }

                // 글자 폭 누적
                int w = pFont->GetCharWidth(code);
                currentLineWidth += w;

                if (maxBoxWidth < currentLineWidth + 20) {
                    maxBoxWidth = currentLineWidth + 20;
                }

                i += charBytes;
            }
        }
    }

    // -------------------------------------------------------------
    // ★★★ 핵심 교정: 어셈블리 0043bff4 ~ 0043c000 과 100% 일치시킴!
    // -------------------------------------------------------------
    int totalBoxHeight = lineCount * maxLineHeight; // IMUL EDX, EAX

    *(int*)((char*)pThis + 0x140) = maxLineHeight;  // [ESI + 0x140] = 한 줄 높이
    *(int*)((char*)pThis + 0x144) = maxBoxWidth;    // [ESI + 0x144] = 전체 폭
    *(int*)((char*)pThis + 0x148) = totalBoxHeight; // [ESI + 0x148] = 전체 박스 높이 (절대 0이면 안 됨!)

    return TRUE;
}

// [후킹 2] CTextLayout::WordWrap (0x00444890)
inline cFont* GetFont(void* pThis) { return *(cFont**)((char*)pThis + 0x124); }
inline const char* GetText(void* pThis) { return *(const char**)((char*)pThis + 0x128); }
inline int& GetLineCount(void* pThis) { return *(int*)((char*)pThis + 0x138); }
inline int*& GetLineEndIndices(void* pThis) { return *(int**)((char*)pThis + 0x13C); }
inline int*& GetLineWidths(void* pThis) { return *(int**)((char*)pThis + 0x140); }
inline int& GetMaxLineWidth(void* pThis) { return *(int*)((char*)pThis + 0x144); }
inline int& GetTotalHeight(void* pThis) { return *(int*)((char*)pThis + 0x148); }

// 원본의 GetMaxWidth() 호출 교정:
// 원본 어셈블리: MOV ECX, pThis -> CALL [vftable + 0x48] (순수 __thiscall, 인자 없음!)
inline int CallGetMaxWidth(void* pThis)
{
    typedef int(__thiscall* tGetMaxWidth)(void*);
    void** vftable = *(void***)pThis;
    tGetMaxWidth fnGetMaxWidth = (tGetMaxWidth)(vftable[0x48 / 4]);
    return fnGetMaxWidth(pThis);
}

typedef BOOL(__fastcall* tWordWrap)(void*, void*);
tWordWrap fpOriginalWordWrap = NULL;
#if 1
BOOL __fastcall Hooked_WordWrap(void* pThis, void* /*edx*/)
{
    if (!pThis) return FALSE;

    int nBoxMaxWidth = CallGetMaxWidth(pThis);
    const char* szText = GetText(pThis);
    cFont* pFont = GetFont(pThis);

    if (!szText || szText[0] == '\0' || !pFont) {
        *(int*)((char*)pThis + 0x138) = 0;
        *(int*)((char*)pThis + 0x144) = 0;
        *(int*)((char*)pThis + 0x148) = 0;
        *(int*)((char*)pThis + 0x14C) = 0;
        return TRUE;
    }

    int textLen = (int)strlen(szText);

    std::vector<int> lineEndIndices;
    std::vector<int> lineWidths;

    int currentLineWidth = 0;
    int maxLineWidth = 0;
    int maxLineHeight = 0;
    int lineStartIdx = 0;
    int lastSpaceIdx = -1;
    int widthAtLastSpace = 0;

    int i = 0;
    while (i < textLen)
    {
        // 1. 개행 문자 ('\n')
        if (szText[i] == '\n')
        {
            lineEndIndices.push_back(i);
            lineWidths.push_back(currentLineWidth);
            if (maxLineWidth < currentLineWidth) maxLineWidth = currentLineWidth;

            currentLineWidth = 0;
            lastSpaceIdx = -1;
            i++;
            lineStartIdx = i;
            continue;
        }

        // 2. 글자 파싱 (한글 2바이트 vs ASCII 1바이트)
        int code = 0;
        int charBytes = 1;
        unsigned char c1 = (unsigned char)szText[i];
        unsigned char c2 = (unsigned char)szText[i + 1];

        if (IsKoreanLeadByte(c1, c2)) {
            code = (c1 << 8) | c2;
            charBytes = 2;
        }
        else 
        {
            code = (char)c1;
            charBytes = 1;
        }

        if (code == ' ') {
            lastSpaceIdx = i;
            widthAtLastSpace = currentLineWidth;
        }

        int charH = pFont->GetCharHeight(code);
        if (maxLineHeight < charH) maxLineHeight = charH;

        int charW = pFont->GetCharWidth(code);

        // 3. 줄바꿈 조건
        if (currentLineWidth + charW > nBoxMaxWidth && i > lineStartIdx)
        {
            if (lastSpaceIdx != -1 && lastSpaceIdx > lineStartIdx)
            {
                lineEndIndices.push_back(lastSpaceIdx);
                lineWidths.push_back(widthAtLastSpace);
                if (maxLineWidth < widthAtLastSpace) maxLineWidth = widthAtLastSpace;

                i = lastSpaceIdx + 1;
            }
            else
            {
                lineEndIndices.push_back(i);
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

    // 4. 마지막 줄 추가
    lineEndIndices.push_back(textLen);
    lineWidths.push_back(currentLineWidth);
    if (maxLineWidth < currentLineWidth) maxLineWidth = currentLineWidth;

    int lineCount = (int)lineEndIndices.size();

    // 5. 엔진 힙 메모리 할당 (0x005ea8fd)
    int* pNewEndIndices = (int*)EngineNew(lineCount * sizeof(int));
    int* pNewWidths = (int*)EngineNew(lineCount * sizeof(int));

    if (pNewEndIndices && pNewWidths)
    {
        for (int k = 0; k < lineCount; ++k) {
            pNewEndIndices[k] = lineEndIndices[k];
            pNewWidths[k] = lineWidths[k];
        }

        // 기존 배열 해제 (0x005ea8f2)
        if (*(int**)((char*)pThis + 0x13C)) EngineDelete(*(int**)((char*)pThis + 0x13C));
        if (*(int**)((char*)pThis + 0x140)) EngineDelete(*(int**)((char*)pThis + 0x140));

        // ★★★ 원본 덤프 값과 100% 동일하게 세팅! ★★★
        *(int*)((char*)pThis + 0x138) = lineCount;                         // orig_138 = 1
        *(int**)((char*)pThis + 0x13C) = pNewEndIndices;                    // orig_13c -> 13
        *(int**)((char*)pThis + 0x140) = pNewWidths;                        // orig_140 -> 65
        *(int*)((char*)pThis + 0x144) = maxLineWidth + 2;                  // orig_144 = 67
        *(int*)((char*)pThis + 0x148) = (maxLineHeight * lineCount) + 2;  // orig_148 = 15
        *(int*)((char*)pThis + 0x14C) = textLen;                           // orig_14c = 13 (★★★ 핵심 해결!)
    }

    return TRUE;
}
#else
BOOL __fastcall Hooked_WordWrap(void* pThis, void* edx)
{
    // 1. 원본 엔진 함수를 그대로 먼저 실행시킵니다!
    BOOL result = fpOriginalWordWrap(pThis, edx);

    // 2. 원본 함수가 끝난 직후, 객체에 무슨 값들이 들어갔는지 확인!
    int lineCount = *(int*)((char*)pThis + 0x138);
    int* pLineEnds = *(int**)((char*)pThis + 0x13C);
    int* pLineWidths = *(int**)((char*)pThis + 0x140);
    int maxWidth = *(int*)((char*)pThis + 0x144);
    int totalHeight = *(int*)((char*)pThis + 0x148);
    const char* text = *(const char**)((char*)pThis + 0x128);

    // ★ 여기에 중단점(BP)을 걸고 Visual Studio 조사식(Watch) 창을 봅니다!
    // text가 무엇일 때, lineCount가 몇이고, pLineEnds[0]이 무엇인지 확인!
    return result;
}
#endif

void patchBytes(void* ptr, const void* data, size_t bytes) {
    DWORD oldProtect;
    if (VirtualProtect(ptr, bytes, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        memcpy(ptr, data, bytes);
        VirtualProtect(ptr, bytes, oldProtect, &oldProtect);
        // 패치한 메모리 영역의 CPU 캐시를 갱신합니다.
        FlushInstructionCache(GetCurrentProcess(), ptr, bytes);
    }
}

// [메모리 패치] GetCharHeight/Width 32비트화 및 DrawTextLines 인라인 패치
void ApplyMemoryPatches()
{
    {
        // CFontCharMap::FindGlyphIndex (0x004355c0)
        const unsigned char patch[] = { 0x31, 0xC0, 0x90 };
        patchBytes(reinterpret_cast<void*>(0x00435601), patch, sizeof(patch));
    }

    {
        const unsigned char patch[] = { 0x8B, 0x44, 0x24, 0x04, 0x90 };
        // 1. cFont::GetCharHeight (0x00435e00)
        patchBytes(reinterpret_cast<void*>(0x00435e00), patch, sizeof(patch));
    
        // 2. cFont::GetCharWidth (0x00435e30)
        patchBytes(reinterpret_cast<void*>(0x00435E30), patch, sizeof(patch));
    }

    {
        // 3.1.a CTextBox::DrawTextLines 내부 점프 패치
        const unsigned char patch1[] = { 0xE9, 0x60, 0x77, 0x1D, 0x00, 0x90 };
        patchBytes(reinterpret_cast<void*>(0x0043bd9b), patch1, sizeof(patch1));
        // 3.1.b Code Cave1 쓰기 (0x00613500)
		const unsigned char cave1[] = {
		0x0F, 0xB6, 0x14, 0x18, 0x80, 0xFA, 0xB0, 0x72, 0x1E, 0x80, 0xFA, 0xC8, 0x77, 0x19,
		0x0F, 0xB6, 0x44, 0x18, 0x01, 0x3C, 0xA1, 0x72, 0x10, 0x3C, 0xFE, 0x77, 0x0C, 0xC1,
		0xE2, 0x08, 0x0B, 0xD0, 0x8B, 0xC2, 0xEB, 0x06, 0x90, 0x90, 0x90, 0x0F, 0xBE, 0xC2,
		0x8B, 0x11, 0xE9, 0x70, 0x88, 0xE2, 0xFF
		};
        patchBytes(reinterpret_cast<void*>(0x00613500), cave1, sizeof(cave1));

        // 3.2.a CTextBox::DrawTextLines 내부 점프 패치
        const unsigned char patch2[] = { 0xE9, 0x8B, 0x77, 0x1D, 0x00, 0x90 };
        patchBytes(reinterpret_cast<void*>(0x0043be72), patch2, sizeof(patch2));
        // 3.2.b Code Cave2 쓰기 (0x00613600)
        const unsigned char cave2[] = {
        0x0F, 0xB6, 0x14, 0x18, 0x80, 0xFA, 0xB0, 0x72, 0x1E, 0x80, 0xFA, 0xC8, 0x77, 0x19,
        0x0F, 0xB6, 0x44, 0x18, 0x01, 0x3C, 0xA1, 0x72, 0x10, 0x3C, 0xFE, 0x77, 0x0C, 0xC1,
        0xE2, 0x08, 0x0B, 0xD0, 0x8B, 0xC2, 0xEB, 0x06, 0x90, 0x90, 0x90, 0x0F, 0xBE, 0xC2,
        0x50, 0x8B, 0x11, 0xE9, 0x44, 0x88, 0xE2, 0xFF
        };
        patchBytes(reinterpret_cast<void*>(0x00613600), cave2, sizeof(cave2));

        // 3.3.a CTextBox::DrawTextLines 내부 점프 패치
        const unsigned char patch3[] = { 0xE9, 0x7C, 0x78, 0x1D, 0x00 };
        patchBytes(reinterpret_cast<void*>(0x0043be7f), patch3, sizeof(patch3));
        // 3.3.b Code Cave3 쓰기 (0x00613700)
        const unsigned char cave3[] = {
        0x8A, 0x04, 0x18, 0x3C, 0xB0, 0x72, 0x1B, 0x3C, 0xC8, 0x77, 0x17, 0x8A, 0x44, 0x18,
        0x01, 0x3C, 0xA1, 0x72, 0x0F, 0x3C, 0xFE, 0x77, 0x0B, 0x83, 0xC3, 0x02, 0xEB, 0x07,
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x43, 0x89, 0x4C, 0x24, 0x30, 0xE9, 0x58, 0x87,
        0xE2, 0xFF
        };
        patchBytes(reinterpret_cast<void*>(0x00613700), cave3, sizeof(cave3));
    }

}

// 1. 새로운 폰트 경로 (길이에 제한 없음!)
const char g_szNewFontPath[] = ".\\Data\\Fonts_kr\\font%d";
const char g_szNewDialgouePath[] = ".\\Data\\Dialogue_kr\\%s";

void PatchResourcePath()
{
    //DWORD oldProtect;

    uint32_t newPathAddr = reinterpret_cast<uint32_t>(g_szNewFontPath);
    // 2. 0x0043ffd1 주소의 4바이트 포인터를 문자열 주소로 교체!
    patchBytes(reinterpret_cast<void*>(0x0043ffd1), &newPathAddr, 4);

    newPathAddr = reinterpret_cast<uint32_t>(g_szNewDialgouePath);
    uint32_t pDialpath[] = { 0x005b2b86, 0x005b2ba5, 0x005b35d6, 0x005b35f5, 0x005b3832, 0x005b39d2, 0x005b3a31,0x005b3a6f, 0x005b3e3a, 0x005b44f1, 0x005b4541 };
    int elementCount = sizeof(pDialpath) / sizeof(pDialpath[0]);
    // 2. pDialpath[] 주소들 4바이트 포인터를 문자열 주소로 교체!
    for (int i = 0; i < elementCount; ++i) {
        patchBytes(reinterpret_cast<void*>(pDialpath[i]), &newPathAddr, 4);
    }
}


// [초기화 진입점]
void InstallHooks()
{
    PatchResourcePath();
    ApplyMemoryPatches();
    
    if (MH_Initialize() == MH_OK)
    {
        MH_CreateHook((LPVOID)0x0043bf40, &Hooked_CalculateTextExtents, (LPVOID*)&fpOriginalCalculateTextExtents);
        MH_CreateHook((LPVOID)0x00444890, &Hooked_WordWrap, (LPVOID*)&fpOriginalWordWrap);
        MH_EnableHook(MH_ALL_HOOKS);
    }
}