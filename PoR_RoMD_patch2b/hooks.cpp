#include <windows.h>
#include <shlwapi.h>
#include <intrin.h>
#include <stdint.h>
#include <stdio.h>

#include "por2.h"
#include "help_util.h"

#define _LOG_   1
#if _LOG_
#define CHECK(charcode) do { PLOG_DEBUG << "Caller" << __func__; GetCurrentCall(charcode); } while(0)
#else
#define CHECK(charcode) ((void)0)
#endif


__forceinline void GetCurrentCall(int charCode)
{
    uintptr_t retAddr = reinterpret_cast<uintptr_t>(_ReturnAddress());
    uint8_t c1 = static_cast<uint8_t>(charCode & 0xFF);        // 현재 바이트
    uint8_t c2 = static_cast<uint8_t>((charCode >> 8) & 0xFF); // 다음 바이트 (4바이트 읽기)


    // 1. WordWrap (0x00444890 ~ 0x00444CAB 부근): ESI가 인덱스
    if (retAddr >= 0x00444890 && retAddr <= 0x00444CAB) {
        //OutputDebugStringFormat("[WordWrap] retAddr=0x%p, charCode=0x%02X,0x%02X, %s\n", (void*)retAddr, c1, c2, OutputEucKrChar(c1, c2));
		PLOG_DEBUG << "[WordWrap] retAddr=0x" << std::hex << retAddr << ", charCode=0x" << std::hex << (int)c1 << ",0x" << std::hex << (int)c2 << ", " << OutputEucKrChar(c1, c2);
    }
    // 2. DrawTextLines (0x0043BD00 ~ 0x0043BF00 부근): EBX가 인덱스
    else if (retAddr >= 0x0043BD00 && retAddr <= 0x0043BF00) {
        //OutputDebugStringFormat("[DrawTextLines] retAddr=0x%p, charCode=0x%02X,0x%02X, %s\n", (void*)retAddr, c1, c2, OutputEucKrChar(c1, c2));
		PLOG_DEBUG << "[DrawTextLines] retAddr=0x" << std::hex << retAddr << ", charCode=0x" << std::hex << (int)c1 << ",0x" << std::hex << (int)c2 << ", " << OutputEucKrChar(c1, c2);
    }
    // 3. CalculateTextExtents: EDI가 인덱스!
    else if (retAddr >= 0x0043BF40 && retAddr <= 0x0043C00F) {
        //OutputDebugStringFormat("[CalculateTextExtents] retAddr=0x%p, charCode=0x%02X,0x%02X, %s\n", (void*)retAddr, c1, c2, OutputEucKrChar(c1, c2));
		PLOG_DEBUG << "[CalculateTextExtents] retAddr=0x" << std::hex << retAddr << ", charCode=0x" << std::hex << (int)c1 << ",0x" << std::hex << (int)c2 << ", " << OutputEucKrChar(c1, c2);
    }
    // 4. RenderShadow: EDI가 인덱스!
    else if (retAddr >= 0x00444d40 && retAddr <= 0x00445039) {
        //OutputDebugStringFormat("[RenderShadow] retAddr=0x%p, charCode=0x%02X,0x%02X, %s\n", (void*)retAddr, c1, c2, OutputEucKrChar(c1, c2));
		PLOG_DEBUG << "[RenderShadow] retAddr=0x" << std::hex << retAddr << ", charCode=0x" << std::hex << (int)c1 << ",0x" << std::hex << (int)c2 << ", " << OutputEucKrChar(c1, c2);
    }
    // 5. unknown: EDI가 인덱스!
    else if (retAddr >= 0x00476820 && retAddr <= 0x00476adc) {
        //OutputDebugStringFormat("[Case5(unknown)] retAddr=0x%p, charCode=0x%02X,0x%02X, %s\n", (void*)retAddr, c1, c2, OutputEucKrChar(c1, c2));
		PLOG_DEBUG << "[Case5(unknown)] retAddr=0x" << std::hex << retAddr << ", charCode=0x" << std::hex << (int)c1 << ",0x" << std::hex << (int)c2 << ", " << OutputEucKrChar(c1, c2);
    }
    else {
        //OutputDebugStringFormat("[!!!ELSE!!!] retAddr=0x%p, charCode=0x%02X,0x%02X, %s\n", (void*)retAddr, c1, c2, OutputEucKrChar(c1, c2));
		PLOG_DEBUG << "[!!!ELSE!!!] retAddr=0x" << std::hex << retAddr << ", charCode=0x" << std::hex << (int)c1 << ",0x" << std::hex << (int)c2 << ", " << OutputEucKrChar(c1, c2);
    }
}


// ----------------------------------------------------------------
// cFont 글리프 검색 함수 포인터 (0x004355C0)
// ----------------------------------------------------------------
typedef int(__thiscall* pfnFindGlyphIndex)(cCharCodeMap* charMap, int code);
const pfnFindGlyphIndex CFontCharMap_FindGlyphIndex = reinterpret_cast<pfnFindGlyphIndex>(0x004355C0);

// ----------------------------------------------------------------
// [2] 독립 채널용 한글 상태 머신 (레지스터 / 인덱스 배열 완전 불필요)
// ----------------------------------------------------------------
struct HangulChannelState
{
    bool    expectTail = false;       // 바로 다음 바이트가 Tail Byte여야 하는가?
    uint8_t expectedTailVal = 0;      // 기대하는 Tail Byte의 값

    // 반환값:
    //  1: 유효한 한글 완성형 음절 (outFinalCode에 2바이트 코드)
    //  0: 테일바이트 (너비 0 / 빈 Rect 처리 대상)
    // -1: 일반 ASCII 문자 (outFinalCode에 1바이트 코드)
    int __fastcall Process(int charCode, int& outFinalCode)
    {
        uint8_t c1 = static_cast<uint8_t>(charCode & 0xFF);        // 현재 바이트
        uint8_t c2 = static_cast<uint8_t>((charCode >> 8) & 0xFF); // 다음 바이트 (4바이트 읽기)

        // 1. 직전에 Lead Byte를 처리했고, 이번 바이트가 기대했던 Tail Byte인 경우
        if (expectTail)
        {
            expectTail = false; // 1회성 소비 후 즉시 해제!

            // 만약 현재 바이트가 기대한 Tail 바이트와 일치하면 정상 테일바이트
            if (c1 == expectedTailVal) {
                outFinalCode = 0;
                return 0; // 테일바이트임
            }
            // (만약 기대와 다르면 문자열이 비정상적으로 끊긴 것이므로 통과)
        }

        // 2. 새로운 완성형 한글 음절 시작 (0xB0A1 ~ 0xC8FE)
        if (c1 >= 0xB0 && c1 <= 0xC8 && c2 >= 0xA1 && c2 <= 0xFE)
        {
            expectTail = true;          // 다음 호출은 반드시 이 글자의 Tail Byte여야 함!
            expectedTailVal = c2;       // 기대하는 Tail Byte 값 기억
            outFinalCode = (c1 << 8) | c2;
            return 1; // 한글 음절
        }

        // 3. 영문, 숫자, 공백(0x20), 개행(0x0A) 등 일반 문자
        expectTail = false;             // 비한글 문자를 만나면 상태 자동 복구(Self-Healing)
        outFinalCode = static_cast<char>(c1);
        return -1; // 일반 문자
    }
};


// 각 훅 함수 전용 독립 채널 (스레드 안전)
thread_local HangulChannelState g_chanWidth;
thread_local HangulChannelState g_chanHeight;
thread_local HangulChannelState g_chanRect;

// ----------------------------------------------------------------
// [3] GetCharWidth 훅
// ----------------------------------------------------------------
int __fastcall Hook_GetCharWidth(cFont* thisPtr, void* /*dummyEDX*/, int charCode)
{
	CHECK(charCode);

    int finalCode = 0;
    int type = g_chanWidth.Process(charCode, finalCode);

    // 테일바이트: 너비 0 반환
    if (type == 0) {
        return 0;
    }
	
    cCharCodeMap* pCharMap = thisPtr->pCharMap; // +0x0C
    int idx = CFontCharMap_FindGlyphIndex(pCharMap, finalCode);
    if (idx < 0) 
        return 0;

    cGlyph** pGlyphs = thisPtr->pGlyphs; // +0x08
    RECT &rect = (*pGlyphs[idx]).rect;

    // (x1 - x0)
	return rect.right - rect.left;
}


// ----------------------------------------------------------------
// [4] GetCharHeight 훅
// ----------------------------------------------------------------
int __fastcall Hook_GetCharHeight(cFont* thisPtr, void* /*dummyEDX*/, int charCode)
{
    CHECK(charCode);

    int finalCode = 0;
    int type = g_chanHeight.Process(charCode, finalCode);

    // 테일바이트: 높이 0 반환
    if (type == 0) {
        return 0;
    }

    cCharCodeMap* pCharMap = thisPtr->pCharMap; // +0x0C
    int idx = CFontCharMap_FindGlyphIndex(pCharMap, finalCode);
    if (idx < 0)
        return 0;

    cGlyph** pGlyphs = thisPtr->pGlyphs; // +0x08
    RECT& rect = (*pGlyphs[idx]).rect;

    // (y1 - y0)
    return rect.bottom - rect.top;
}

// 크래시 방지용 전역 빈 사각형 구조체
static RECT g_dummyZeroRect = { 0, 0, 0, 0 };

// ----------------------------------------------------------------
// [5] GetCharRect 훅
// ----------------------------------------------------------------
RECT* __fastcall Hook_GetCharRect(cFont* thisPtr, void* /*edx*/, int charCode)
{
    CHECK(charCode);

    int finalCode = 0;
    int type = g_chanRect.Process(charCode, finalCode);

    // 테일바이트: 화면에 그리지 않도록 크기 0 사각형 반환
    if (type == 0) {
        return &g_dummyZeroRect;
    }

    cCharCodeMap* pCharMap = thisPtr->pCharMap; // +0x0C
    int idx = CFontCharMap_FindGlyphIndex(pCharMap, finalCode);
    if (idx < 0)
        return 0;

    cGlyph** pGlyphs = thisPtr->pGlyphs; // +0x08
    RECT& rect = (*pGlyphs[idx]).rect;

    // &pGlyph->x0 주소 반환 (RECT 포인터)
    return &rect;
}


void patchBytes(uintptr_t targetAddr, const void* data, size_t bytes) {
    DWORD oldProtect;
    if (VirtualProtect(reinterpret_cast<void*>(targetAddr), bytes, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        memcpy(reinterpret_cast<void*>(targetAddr), data, bytes);
        VirtualProtect(reinterpret_cast<void*>(targetAddr), bytes, oldProtect, &oldProtect);
        // 패치한 메모리 영역의 CPU 캐시를 갱신합니다.
        FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(targetAddr), bytes);
    }
}

// ============================================================
// [패치 3] CTextLayout::WordWrap (0x00444890) 4바이트 읽기
// ============================================================
void PatchWordWrap_4ByteRead()
{
    // MOV AL, [EAX+ESI*1] (8A 04 30) → MOV EAX, [EAX+ESI*1] (8B 04 30)
    // MOV DL, [EDX+ESI*1] (8A 14 32) → MOV EDX, [EDX+ESI*1] (8B 14 32)
    // 각 호출부에서 1바이트만 변경하여 4바이트를 읽어서 전달합니다.
    // GetCharHeight/GetCharWidth 내부에서 리드바이트(0xB0-0xC8) 확인 후
    // 테일바이트(0xA1-0xFE)와 결합하여 사용하고, 테일바이트면 0을 반환합니다.

    const unsigned char patch[] = { 0x8B };

    // 1. GetCharHeight 호출부 (0x00444a08)
    patchBytes(0x00444a08, patch, sizeof(patch));

    // 2. GetCharWidth 호출부 (0x00444a27)
    patchBytes(0x00444a27, patch, sizeof(patch));

    // 3. 백트래킹 GetCharWidth 호출부 (0x00444a52)
    patchBytes(0x00444a52, patch, sizeof(patch));

    // 4. 공백 스킵 GetCharWidth 호출부 (0x00444a89)
    patchBytes(0x00444a89, patch, sizeof(patch));

    //OutputDebugStringW(L"[Patch] WordWrap() patched.\n");
    PLOG_DEBUG << "[Patch] WordWrap() patched.";
}

// ============================================================
// [패치 4] CTextBox::CalculateTextExtents (0x0043bf40) 4바이트 읽기
// ============================================================
void PatchCalculateTextExtents_4ByteRead()
{
    // MOV AL, [EDI+EAX*1] (8A 04 07) → MOV EAX, [EDI+EAX*1] (8B 04 07)
    // MOV AL, [EAX+EDI*1] (8A 04 38) → MOV EAX, [EAX+EDI*1] (8B 04 38)
    // 각 호출부에서 1바이트만 변경하여 4바이트를 읽어서 전달합니다.
    // GetCharHeight/GetCharWidth 내부에서 리드바이트(0xB0-0xC8) 확인 후
    // 테일바이트(0xA1-0xFE)와 결합하여 사용하고, 테일바이트면 0을 반환합니다.

    const unsigned char patch[] = { 0x8B };

    // 1. GetCharHeight 호출부 (0x0043bf83)
    patchBytes(0x0043bf83, patch, sizeof(patch));

    // 2. GetCharWidth 호출부 (0x0043bfa7)
    patchBytes(0x0043bfa7, patch, sizeof(patch));

    //OutputDebugStringW(L"[Patch] CalculateTextExtents() patched.\n");
	PLOG_DEBUG << "[Patch] CalculateTextExtents() patched.";
}

// ============================================================
// [패치 5] CTextBox::DrawTextLines (0x0043bd9b) 4바이트 읽기
// ============================================================
void PatchDrawTextLines_4ByteRead()
{
    // MOVSX EAX, byte ptr [EAX+EBX*1] (0F BE 04 18) → MOV EAX, [EAX+EBX*1] (8B 04 18) + NOP
    // MOV AL, byte ptr [EAX+EBX*1] (8A 04 18) → MOV EAX, [EAX+EBX*1] (8B 04 18)
    // 각 호출부에서 4바이트를 읽어서 전달합니다.
    // GetCharRect/GetCharWidth 내부에서 리드바이트(0xB0-0xC8) 확인 후
    // 테일바이트(0xA1-0xFE)와 결합하여 사용하고, 테일바이트면 0을 반환합니다.
    // 인덱스 증가(INC EBX)는 항상 +1이지만, 테일바이트에서 0이 반환되므로
    // 그리기/폭 누적이 건너뛰어지고 다음 반복에서 테일바이트 다음으로 이동합니다.

    // 1. GetCharRect 호출부 (0x0043bd9b): MOVSX → MOV + NOP
    const unsigned char patch1[] = { 0x8B, 0x04, 0x18, 0x90 };
    patchBytes(0x0043bd9b, patch1, sizeof(patch1));

    // 2. GetCharWidth 호출부 (0x0043be72): MOV AL → MOV EAX (1바이트만 변경)
    const unsigned char patch2[] = { 0x8B };
    patchBytes(0x0043be72, patch2, sizeof(patch2));

    //OutputDebugStringW(L"[Patch] DrawTextLines() patched.\n");
	PLOG_DEBUG << "[Patch] DrawTextLines() patched.";
}

// ============================================================
// [패치 6] cTextLayout::RenderShadow (00444d40) 4바이트 읽기
// ============================================================
void PatchRenderShadow_4ByteRead()
{
    // 1. GetCharRect 호출부 (0x00444e65): MOVSX → MOV + NOP (4바이트 교체)
    const unsigned char patch1[] = { 0x8B, 0x04, 0x28, 0x90 };
    patchBytes(0x00444e65, patch1, sizeof(patch1));

    // 2. GetCharWidth 호출부 (0x00444f3e): MOV AL → MOV EAX (첫 1바이트만 0x8B로 변경)
    const unsigned char patch2[] = { 0x8B };
    patchBytes(0x00444f3e, patch2, sizeof(patch2));

    //OutputDebugStringA("[Patch] RenderShadow() patched.\n");
    PLOG_DEBUG << "[Patch] RenderShadow() patched.";
}

// ============================================================
// [패치 7] 0x0047695D (sprintf 텍스트 폭 측정 루프) 4바이트 읽기
// ============================================================
void PatchSprintfMeasure_4ByteRead()
{
    // 004769a5: MOVSX EAX, AL (0F BE C0) -> MOV EAX, [EBP] (8B 45 00)
    // 3바이트 1:1 교체
    const unsigned char patch[] = { 0x8B, 0x45, 0x00 };
    patchBytes(0x004769a5, patch, sizeof(patch));

	//OutputDebugStringA("[Patch] SprintfMeasure() patched.\n");
    PLOG_DEBUG << "[Patch] SprintfMeasure() patched.";
}

// 1. 새로운 폰트 경로 (길이에 제한 없음!)
char g_szNewFontPath[MAX_PATH] = "./Data_kr/Fonts/font%d";
char g_szNewDialgouePath[MAX_PATH] = "./Data_kr/Dialogue/%s";


void ReadIniFilePath() 
{
    std::wstring fullSubPath;

    std::wstring fontFolder = g_Config.GetString(L"Settings", L"FontPath", L"./Data_kr/Fonts");
    if (PathIsRelativeW(fontFolder.c_str())) {
        fullSubPath = CombinePath(g_Config.currentExePath, fontFolder);
    }
    else {
        fullSubPath = fontFolder;
    }
    fontFolder = CombinePath(fullSubPath, L"font%d");
	WideCharToMultiByte(CP_UTF8, 0, fontFolder.c_str(), -1, g_szNewFontPath, sizeof(g_szNewFontPath), NULL, NULL);

    std::wstring dialogueFolder = g_Config.GetString(L"Settings", L"DialoguePath", L"./Data_kr/Dialogue");
    if (PathIsRelativeW(dialogueFolder.c_str())) {
        fullSubPath = CombinePath(g_Config.currentExePath, dialogueFolder);
    }
    else {
        fullSubPath = dialogueFolder;
    }
    dialogueFolder = CombinePath(fullSubPath, L"%s");
    WideCharToMultiByte(CP_UTF8, 0, dialogueFolder.c_str(), -1, g_szNewDialgouePath, sizeof(g_szNewDialgouePath), NULL, NULL);
}

void PatchResourcePath()
{
    ReadIniFilePath();

    uintptr_t newPathAddr = reinterpret_cast<uintptr_t>(g_szNewFontPath);
    // 2. 0x0043ffd1 주소의 4바이트 포인터를 문자열 주소로 교체!
    patchBytes(0x0043ffd1, &newPathAddr, 4);

    newPathAddr = reinterpret_cast<uintptr_t>(g_szNewDialgouePath);
    uintptr_t pDialpath[] = { 0x005b2b86, 0x005b2ba5, 0x005b35d6, 0x005b35f5, 0x005b3832, 0x005b39d2, 0x005b3a31,0x005b3a6f, 0x005b3e3a, 0x005b44f1, 0x005b4541 };
    int elementCount = sizeof(pDialpath) / sizeof(pDialpath[0]);
    // 2. pDialpath[] 주소들 4바이트 포인터를 문자열 주소로 교체!
    for (int i = 0; i < elementCount; ++i) {
        patchBytes(pDialpath[i], &newPathAddr, 4);
    }
}


//////////////////============================================================
// 32비트 전용: 5바이트 점프 훅 함수 (MinHook 대체)
bool HookFunction(uintptr_t targetAddr, uintptr_t hookFuncAddr)
{
    DWORD oldProtect;
    if (!VirtualProtect(reinterpret_cast<void*>(targetAddr), 5, PAGE_EXECUTE_READWRITE, &oldProtect))
        return false;

    // E9 [상대 변위 4바이트] : JMP hookFuncAddr
    *reinterpret_cast<uint8_t*>(targetAddr) = 0xE9;
    *reinterpret_cast<int32_t*>(targetAddr + 1) = static_cast<int32_t>(hookFuncAddr - (targetAddr + 5));

    VirtualProtect(reinterpret_cast<void*>(targetAddr), 5, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(targetAddr), 5);
    return true;
}

void ApplyHooks()
{
    // 1. GetCharHeight (0x00435E00) -> Hook_GetCharHeight 점프
    HookFunction(0x00435E00, reinterpret_cast<uintptr_t>(Hook_GetCharHeight));
    // 2. GetCharWidth (0x00435E30) -> Hook_GetCharWidth 점프
    HookFunction(0x00435E30, reinterpret_cast<uintptr_t>(Hook_GetCharWidth));
    // 3. GetCharRect (0x00435E60) -> Hook_GetCharRect 점프
    HookFunction(0x00435E60, reinterpret_cast<uintptr_t>(Hook_GetCharRect));

    //OutputDebugStringA("[Patch] font functions hooked.\n");
	PLOG_DEBUG << "[Patch] font functions hooked.";
}
//////////////////============================================================


// ============================================================
// [메모리 패치] 전체 적용
// ============================================================
void ApplyMemoryPatches()
{
    //ReadIniFilePath();
    PatchResourcePath();
    
    ApplyHooks();

    PatchWordWrap_4ByteRead();
    PatchCalculateTextExtents_4ByteRead();
    PatchDrawTextLines_4ByteRead();
    PatchRenderShadow_4ByteRead();
	//PatchSprintfMeasure_4ByteRead();  'A'의 높이만 측정하기 위해 호출하므로 패치하지 않아도 됨. 모든 폰트는 동일한 높이를 가짐.
}
