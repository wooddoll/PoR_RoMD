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
    // CP949 한글 범위 검사: 
    // 첫 바이트가 0x81~0xFE 이고, 두 번째 바이트도 유효한 범위(0x41~0xFE)일 때만 진짜 한글!
    return (c1 >= 0x81 && c1 <= 0xFE) && (c2 >= 0x41 && c2 <= 0xFE);
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

BOOL __fastcall Hooked_WordWrap(void* pThis, void* /*edx*/)
{
    if (!pThis) return FALSE;

    int nBoxMaxWidth = CallGetMaxWidth(pThis);
    const char* szText = GetText(pThis);
    cFont* pFont = GetFont(pThis);

    GetTotalHeight(pThis) = 0;
    GetMaxLineWidth(pThis) = 0;

    if (!szText || szText[0] == '\0' || !pFont) {
        GetLineCount(pThis) = 0;
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
        // 1. 강제 줄바꿈('\n') 처리
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

        // 2. 글자 판별 (CP949 한글 2바이트 vs ASCII 1바이트)
        int code = 0;
        int charBytes = 1;

        unsigned char c1 = (unsigned char)szText[i];

        if (c1 >= 0x81 && c1 <= 0xFE && szText[i + 1] != '\0')
        {
            // 한글 (빅엔디안 결합)
            unsigned char c2 = (unsigned char)szText[i + 1];
            code = (c1 << 8) | c2;
            charBytes = 2;
        }
        else
        {
            // ASCII
            code = (char)c1;
            charBytes = 1;
        }

        // 공백 위치 기록
        if (code == ' ') {
            lastSpaceIdx = i;
            widthAtLastSpace = currentLineWidth;
        }

        // 글자 폭 가져오기
        int charW = pFont->GetCharWidth(code);

        // 3. 줄바꿈 조건 검사
        if (currentLineWidth + charW > nBoxMaxWidth && i > lineStartIdx)
        {
            if (lastSpaceIdx != -1 && lastSpaceIdx > lineStartIdx)
            {
                // 영문: 단어 단위 줄바꿈 (공백 위치에서 자름)
                lineEndIndices.push_back(lastSpaceIdx);
                lineWidths.push_back(widthAtLastSpace);
                if (maxLineWidth < widthAtLastSpace) maxLineWidth = widthAtLastSpace;

                i = lastSpaceIdx + 1; // 다음 줄은 공백 다음부터
            }
            else
            {
                // 한글 또는 공백 없는 긴 단어: 현재 글자 바로 앞에서 줄바꿈!
                lineEndIndices.push_back(i - 1);
                lineWidths.push_back(currentLineWidth);
                if (maxLineWidth < currentLineWidth) maxLineWidth = currentLineWidth;
                // i는 현재 위치 그대로 유지하여 다음 줄의 첫 글자가 됨!
            }

            currentLineWidth = 0;
            lastSpaceIdx = -1;
            lineStartIdx = i;
            continue;
        }

        currentLineWidth += charW;
        i += charBytes;
    }

    // 마지막 줄 등록
    lineEndIndices.push_back(i - 1);
    lineWidths.push_back(currentLineWidth);
    if (maxLineWidth < currentLineWidth) maxLineWidth = currentLineWidth;

    int lineCount = (int)lineEndIndices.size();
    GetLineCount(pThis) = lineCount;
    GetMaxLineWidth(pThis) = maxLineWidth;

    // -------------------------------------------------------------
    // ★ 핵심 수정: 힙 불일치 크래시 방지!
    // DLL의 new[] 대신 게임 엔진의 malloc/operator_new 사용
    // -------------------------------------------------------------
    size_t allocSize = (lineCount + 1) * sizeof(int); // 여유 버퍼 1칸 추가

    // 엔진의 operator_new 호출 (0x0047e6d0 등이 확실하지 않다면 LocalAlloc이나 원본 포인터 재활용)
    int* pNewEndIndices = (int*)EngineNew(allocSize);
    int* pNewWidths = (int*)EngineNew(allocSize);

    if (pNewEndIndices && pNewWidths)
    {
        memcpy(pNewEndIndices, lineEndIndices.data(), lineCount * sizeof(int));
        memcpy(pNewWidths, lineWidths.data(), lineCount * sizeof(int));

        // 기존 메모리 해제
        if (GetLineEndIndices(pThis)) EngineDelete(GetLineEndIndices(pThis));
        if (GetLineWidths(pThis))     EngineDelete(GetLineWidths(pThis));

        // 새 포인터 장착
        GetLineEndIndices(pThis) = pNewEndIndices;
        GetLineWidths(pThis) = pNewWidths;
    }

    return TRUE;
}

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
        const unsigned char patch[] = { 0x31, 0xC0, 0x90 };
        // CFontCharMap::FindGlyphIndex (0x004355c0)
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
        0x0F, 0xB6, 0x14, 0x18, 0x80, 0xFA, 0x80, 0x72, 0x0F, 0x0F, 0xB6, 0x44, 0x18, 0x01, 0xC1, 0xE2, 0x08, 0x0B, 0xD0, 0x8B, 0xC2, 0xEB, 0x03, 0x90, 0x0F, 0xBE, 0xC2, 0x8B, 0x11, 0xE9, 0x80, 0x88, 0xE2, 0xFF
        };
        patchBytes(reinterpret_cast<void*>(0x00613500), cave1, sizeof(cave1));

        // 3.2.a CTextBox::DrawTextLines 내부 점프 패치
        const unsigned char patch2[] = { 0xE9, 0x8B, 0x77, 0x1D, 0x00, 0x90 };
        patchBytes(reinterpret_cast<void*>(0x0043be72), patch2, sizeof(patch2));
        // 3.2.b Code Cave2 쓰기 (0x00613600)
        const unsigned char cave2[] = {
        0x0F, 0xB6, 0x14, 0x18, 0x80, 0xFA, 0x80, 0x72, 0x0F, 0x0F, 0xB6, 0x44, 0x18, 0x01, 0xC1, 0xE2, 0x08, 0x0B, 0xD0, 0x8B, 0xC2, 0xEB, 0x03, 0x90, 0x0F, 0xBE, 0xC2, 0x50, 0x8B, 0x11, 0xE9, 0x54, 0x88, 0xE2, 0xFF
        };
        patchBytes(reinterpret_cast<void*>(0x00613600), cave2, sizeof(cave2));

        // 3.3.a CTextBox::DrawTextLines 내부 점프 패치
        const unsigned char patch3[] = { 0xE9, 0x7C, 0x78, 0x1D, 0x00 };
        patchBytes(reinterpret_cast<void*>(0x0043be7f), patch3, sizeof(patch3));
        // 3.3.b Code Cave3 쓰기 (0x00613700)
        const unsigned char cave3[] = {
        0x8A, 0x04, 0x18, 0x3C, 0x80, 0x72, 0x05, 0x83, 0xC3, 0x02, 0xEB, 0x01, 0x43, 0x89, 0x4C, 0x24, 0x30, 0xE9, 0x6E, 0x87, 0xE2, 0xFF
        };
        patchBytes(reinterpret_cast<void*>(0x00613700), cave3, sizeof(cave3));
    }
}

// 1. 새로운 폰트 경로 (길이에 제한 없음!)
const char g_szNewFontPath[] = ".\\Data\\Fonts_kr\\font%d";
const char g_szNewDialgouePath[] = ".\\Data\\Dialogue_kr\\%s";

void PatchFontPath()
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
    PatchFontPath();
    ApplyMemoryPatches();
    
    if (MH_Initialize() == MH_OK)
    {
        MH_CreateHook((LPVOID)0x0043bf40, &Hooked_CalculateTextExtents, (LPVOID*)&fpOriginalCalculateTextExtents);
        MH_CreateHook((LPVOID)0x00444890, &Hooked_WordWrap, (LPVOID*)&fpOriginalWordWrap);
        MH_EnableHook(MH_ALL_HOOKS);
    }
}