#include <windows.h>
#include <intrin.h>
#include <stdint.h>

#include <stdio.h>

const char logFileNmae[] = "hooks.log";
static FILE* fp_log = stderr;
void openLogFile()
{
    if (fp_log == stderr) {
        fopen_s(&fp_log, logFileNmae, "wt");
    }
}

void closeLogFile()
{
    if (fp_log != stderr) {
        fclose(fp_log);
        fp_log = stderr;
    }
}

// ----------------------------------------------------------------
// cFont 글리프 검색 함수 포인터 (0x004355C0)
// ----------------------------------------------------------------
typedef int(__thiscall* pfnFindGlyphIndex)(void* charMap, int code);
const pfnFindGlyphIndex CFontCharMap_FindGlyphIndex = reinterpret_cast<pfnFindGlyphIndex>(0x004355C0);

// [구조체 오프셋 정의]
// this + 0x08 : glyphs 배열 포인터
// this + 0x0C : charMap 포인터
// glyph + 0x04: x0, glyph + 0x08: y0, glyph + 0x0C: x1, glyph + 0x10: y1

// 한 줄 최대 버퍼 길이(예: 1024바이트)를 커버하는 상태 테이블
thread_local bool g_isTailByte[1024] = { false };

// 호출자에 맞는 정확한 루프 인덱스를 추출하는 함수
__forceinline int GetCurrentLoopIndex()
{
    uintptr_t retAddr = reinterpret_cast<uintptr_t>(_ReturnAddress());
    int idx = 0;

    // 1. WordWrap (0x00444890 ~ 0x00444CAB 부근): ESI가 인덱스
    if (retAddr >= 0x00444890 && retAddr <= 0x00444CAB) {
        __asm { mov idx, esi }
		fprintf(fp_log, "[GetCurrentLoopIndex] WordWrap: retAddr=0x%p, idx=%d\n", (void*)retAddr, idx);
    }
    // 2. DrawTextLines (0x0043BD00 ~ 0x0043BF00 부근): EBX가 인덱스
    else if (retAddr >= 0x0043BD00 && retAddr <= 0x0043BF00) {
        __asm { mov idx, ebx }
		fprintf(fp_log, "[GetCurrentLoopIndex] DrawTextLines: retAddr=0x%p, idx=%d\n", (void*)retAddr, idx);
    }
    // 3. CalculateTextExtents: EDI가 인덱스!
    else if (retAddr >= 0x0043BF40 && retAddr <= 0x0043C00F) {
        __asm { mov idx, edi }
		fprintf(fp_log, "[GetCurrentLoopIndex] CalculateTextExtents: retAddr=0x%p, idx=%d\n", (void*)retAddr, idx);
    }
    // 4. RenderShadow: EDI가 인덱스!
    else if (retAddr >= 0x00444d40 && retAddr <= 0x00445039) {
        __asm { mov idx, edi }
		fprintf(fp_log, "[GetCurrentLoopIndex] RenderShadow: retAddr=0x%p, idx=%d\n", (void*)retAddr, idx);
    }
    // 5. unknown: EDI가 인덱스!
    else if (retAddr >= 0x00476820 && retAddr <= 0x00476adc) {
        uintptr_t ptrEBP = 0;
        __asm { mov ptrEBP, ebp }
        idx = static_cast<int>(ptrEBP & 0x3FF); // 0~1023 안전 매핑
    }
    else {
        __asm { mov idx, esi } // 기본값
		fprintf(fp_log, "[GetCurrentLoopIndex] Default: retAddr=0x%p, idx=%d\n", (void*)retAddr, idx);
    }

    return idx;
}

// ----------------------------------------------------------------
// 공통 한글 판별 및 상태 업데이트 헬퍼
// ----------------------------------------------------------------
__forceinline bool CheckAndProcessHangul(int currentIdx, int charCode, int& outFinalCode)
{
    // 새로운 문자열/라인 시작 시 (인덱스가 0이면) 테이블 클리어!
    if (currentIdx == 0) {
        memset(g_isTailByte, 0, sizeof(g_isTailByte));
    }

    // 테일바이트 위치라면 처리 중단 (너비/높이 0 반환 대상)
    if (currentIdx >= 0 && currentIdx < 1024 && g_isTailByte[currentIdx]) {
        return false; // 테일바이트임
    }

    unsigned char c1 = static_cast<unsigned char>(charCode & 0xFF);
    unsigned char c2 = static_cast<unsigned char>((charCode >> 8) & 0xFF);

    if (c1 >= 0xB0 && c1 <= 0xC8 && c2 >= 0xA1 && c2 <= 0xFE) {
        outFinalCode = (c1 << 8) | c2; // 한글 완성형

        // 다음 바이트를 테일바이트로 마킹
        if (currentIdx >= 0 && currentIdx < 1023) {
            g_isTailByte[currentIdx] = false;
            g_isTailByte[currentIdx + 1] = true;
        }
    }
    else {
        outFinalCode = static_cast<char>(c1); // ASCII

        if (currentIdx >= 0 && currentIdx < 1024) {
            g_isTailByte[currentIdx] = false;
        }
    }
    return true; // 유효한 글자
}


// ----------------------------------------------------------------
// [1] GetCharWidth 훅
// ----------------------------------------------------------------
int __fastcall Hook_GetCharWidth(void* thisPtr, void* /*dummyEDX*/, int charCode)
{
    int currentIdx = GetCurrentLoopIndex();
    int finalCode = 0;

    // 테일바이트 위치면 즉시 너비 0 반환!
    if (!CheckAndProcessHangul(currentIdx, charCode, finalCode)) {
		fprintf(fp_log, "[Hook_GetCharWidth] Tail byte detected at index %d, charCode %d, finalCode %d\n", currentIdx, charCode, finalCode);
        return 0;
    }

    void* charMap = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(thisPtr) + 0x0C);
    int idx = CFontCharMap_FindGlyphIndex(charMap, finalCode);

    uintptr_t glyphs = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(thisPtr) + 0x08);
    uintptr_t pGlyph = *reinterpret_cast<uintptr_t*>(glyphs + idx * 4);
	fprintf(fp_log, "[Hook_GetCharWidth] at index %d, charCode %d, finalCode %d, glyph idx=%d\n", currentIdx, charCode, finalCode, idx);
    return *reinterpret_cast<int*>(pGlyph + 0x0C) - *reinterpret_cast<int*>(pGlyph + 0x04);
}


// ----------------------------------------------------------------
// [2] GetCharHeight 훅
// ----------------------------------------------------------------
int __fastcall Hook_GetCharHeight(void* thisPtr, void* /*dummyEDX*/, int charCode)
{
    int currentIdx = GetCurrentLoopIndex();
    int finalCode = 0;

    // 테일바이트 위치면 즉시 높이 0 반환!
    if (!CheckAndProcessHangul(currentIdx, charCode, finalCode)) {
		fprintf(fp_log, "[Hook_GetCharHeight] Tail byte detected at index %d, charCode %d, finalCode %d\n", currentIdx, charCode, finalCode);
        return 0;
    }

    void* charMap = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(thisPtr) + 0x0C);
    int idx = CFontCharMap_FindGlyphIndex(charMap, finalCode);

    uintptr_t glyphs = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(thisPtr) + 0x08);
    uintptr_t pGlyph = *reinterpret_cast<uintptr_t*>(glyphs + idx * 4);
	fprintf(fp_log, "[Hook_GetCharHeight] at index %d, charCode %d, finalCode %d, glyph idx=%d\n", currentIdx, charCode, finalCode, idx);
    return *reinterpret_cast<int*>(pGlyph + 0x10) - *reinterpret_cast<int*>(pGlyph + 0x08);
}


// 크래시 방지용 전역 빈 사각형 구조체
static RECT g_dummyZeroRect = { 0, 0, 0, 0 };


// ----------------------------------------------------------------
// [훅 3] GetCharRect (0x00435E60)
// ----------------------------------------------------------------
RECT* __fastcall Hook_GetCharRect(void* thisPtr, void* /*edx*/, int charCode)
{
    int currentIdx = GetCurrentLoopIndex();
    int finalCode = 0;

    // 테일바이트 위치면 즉시 높이 0 반환!
    if (!CheckAndProcessHangul(currentIdx, charCode, finalCode)) {
		fprintf(fp_log, "[Hook_GetCharRect] Tail byte detected at index %d, charCode %d, finalCode %d\n", currentIdx, charCode, finalCode);
        return &g_dummyZeroRect;
    }

    void* charMap = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(thisPtr) + 0x0C);
    int idx = CFontCharMap_FindGlyphIndex(charMap, finalCode);
    if (idx < 0)
        return &g_dummyZeroRect;

    uintptr_t glyphs = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(thisPtr) + 0x08);
    uintptr_t pGlyph = *reinterpret_cast<uintptr_t*>(glyphs + idx * 4);
	fprintf(fp_log, "[Hook_GetCharRect] at index %d, charCode %d, finalCode %d, glyph idx=%d\n", currentIdx, charCode, finalCode, idx);
    // &pGlyph->x0 주소 반환 (RECT 포인터)
    return reinterpret_cast<RECT*>(pGlyph + 0x04);
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
}

// 1. 새로운 폰트 경로 (길이에 제한 없음!)
const char g_szNewFontPath[] = ".\\Data\\Fonts_kr\\font%d";
const char g_szNewDialgouePath[] = ".\\Data\\Dialogue_kr\\%s";

void PatchResourcePath()
{
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
}
//////////////////============================================================


// ============================================================
// [메모리 패치] 전체 적용
// ============================================================
void ApplyMemoryPatches()
{
    PatchResourcePath();
    
    ApplyHooks();

    PatchWordWrap_4ByteRead();
    PatchCalculateTextExtents_4ByteRead();
    PatchDrawTextLines_4ByteRead();
    PatchRenderShadow_4ByteRead();
    PatchSprintfMeasure_4ByteRead();
}
