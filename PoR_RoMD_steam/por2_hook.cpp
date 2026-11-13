#include "por2.h"

#include <MinHook.h>
#include "help_util.h"

RECT g_dummyZeroRect = { 0, 0, 0, 0 };

int __cdecl Input_Hook(int /*a1*/, int /*a2*/, int ch);
BOOL __fastcall cTextManager_CalculateTextExtents_Hook(cTextManager* self, void* /*edx*/);
BOOL __fastcall cTextLabel_CalcExtents_Hook(cTextLabel* self, void* /*edx*/);

BOOL __fastcall cTextManager_MenuDraw_Hook(cTextManager* self, void* /*edx*/);
BOOL __fastcall cTextManager_DrawTextLines_Hook(cTextManager* self, void* /*edx*/, int /*unusedStackArg*/);
BOOL __fastcall cTextManager_DrawMenuMask_Hook(cTextManager* self, void* /*edx*/);
BOOL __fastcall cTextManager_DrawHMenu_Hook(cTextManager* self, void* /*edx*/, int /*unusedStackArg*/);
void InstallFontMetricHooks();
bool IsNativeCjkTest();     // sneg_hook.cpp

/* 게임 함수 입구에 sneg(MinHook) 의 JMP 가 없을 때만 직접 훅한다.
   원본 입구는 모두 E9 가 아니다 (SUB ESP,xx 등) */
static void HookGameIfNotHookedBySneg(uintptr_t gameAddr, uintptr_t hook, const char* name)
{
    if (*reinterpret_cast<const uint8_t*>(gameAddr) == 0xE9) {
        PLOG_DEBUG << "[por2] " << name << " @" << std::hex << gameAddr << " already hooked by sneg, hook sneg side";
        return;
    }
    HookFunction(gameAddr, hook);
    PLOG_DEBUG << "[por2] " << name << " @" << std::hex << gameAddr << " hooked directly";
}

void ApplyMemoryPatches(uintptr_t baseAddress)
{
    UNREFERENCED_PARAMETER(baseAddress);

    uint8_t ESPtoEAX_I[5] = { 0x8b, 0x44, 0x24, 0x04, 0x90 };
    //patchBytes(0x00435e00, ESPtoEAX_I, 5); // GetCharHeight_00435e00
    //patchBytes(0x00435e30, ESPtoEAX_I, 5); // GetCharWidth_00435e30

    PatchRelativeCall(0x0049EDCE, reinterpret_cast<uintptr_t>(Input_Hook));

    if (IsNativeCjkTest()) {
        // [실험] 그리기는 sneg china 경로에 맡기고, 게임 텍스트 훅은 끈다.
        // 단 게임 쪽 줄 나누기/박스 크기 계산(cTextManager 등)은 여전히 1바이트씩 vtable 메트릭을 부르므로
        // FontMetrics(한글 바이트 = '가' 폭의 절반)는 켜 둔다. 끄면 비트맵 라틴-1 폭으로 1차 줄바꿈 → sneg 가 TTF 폭으로 2차 줄바꿈.
        // sneg 가 대체하지 않는 게임 함수(메뉴 마스크, 박스 크기 계산)는 본선과 같은 우리 훅을 유지한다.
        // 한글 폭 = 비트맵 RECT 폭으로 china_pass 훅(china_glyph.cpp)과 같다.
        HookGameIfNotHookedBySneg(0x0043B8E0, reinterpret_cast<uintptr_t>(cTextManager_DrawMenuMask_Hook),         "MenuMask");
        HookGameIfNotHookedBySneg(0x0043BF40, reinterpret_cast<uintptr_t>(cTextManager_CalculateTextExtents_Hook), "CalculateTextExtents");
        HookGameIfNotHookedBySneg(0x004428B0, reinterpret_cast<uintptr_t>(cTextLabel_CalcExtents_Hook),            "CalcExtentTextLabel");
        InstallFontMetricHooks();
        PLOG_DEBUG << "[exp] sneg draw functions kept (china_pass path), game extents hooks + FontMetrics on";
        return;
    }

    //PatchRelativeCall(0x0043c614, reinterpret_cast<uintptr_t>(cTextManager_CalculateTextExtents_Hook));
    //PatchRelativeCall(0x0043e9fd, reinterpret_cast<uintptr_t>(cTextManager_CalculateTextExtents_Hook));

    //patchDWORD(0x00623040, reinterpret_cast<uintptr_t>(cTextLabel_CalcExtents_Hook));

    // ---- 1바이트로 출력하는 게임 함수 대체 ----
    // sneg.dll 은 ddraw.dll 의 DllMain 에서 LoadLibraryA + SNEG_Load 까지 실행되어
    // (FUN_594a4680 에서 MinHook 등록), 이 스레드가 돌 때는 sneg 훅이 이미 깔려 있다.
    //  - 입구가 이미 sneg 의 JMP(E9) 이면 : sneg 대체 함수 쪽을 ApplySNEGPatches 에서 훅함
    //                                      (MinHook 상태와 충돌하지 않도록 게임 입구는 건드리지 않음)
    //  - sneg 가 등록하지 않은 경우(설정 off 등) : 게임 입구를 직접 훅
    HookGameIfNotHookedBySneg(0x0043B4F0, reinterpret_cast<uintptr_t>(cTextManager_MenuDraw_Hook),      "DrawMenu");      // RET   / sneg DrawMenu_59529ea0
    HookGameIfNotHookedBySneg(0x0043BC80, reinterpret_cast<uintptr_t>(cTextManager_DrawTextLines_Hook), "DrawTextLines"); // RET 4 / sneg DrawTextLines_59529d90
    HookGameIfNotHookedBySneg(0x0043F1B0, reinterpret_cast<uintptr_t>(cTextManager_DrawHMenu_Hook),     "HMenu");         // RET 4 / sneg FUN_59529c50
    // sneg 가 등록하지 않는 함수들 (sneg.dll 에서 오프셋 검색 결과 없음)
    HookGameIfNotHookedBySneg(0x0043B8E0, reinterpret_cast<uintptr_t>(cTextManager_DrawMenuMask_Hook),         "MenuMask");             // RET
    HookGameIfNotHookedBySneg(0x0043BF40, reinterpret_cast<uintptr_t>(cTextManager_CalculateTextExtents_Hook), "CalculateTextExtents"); // RET
    HookGameIfNotHookedBySneg(0x004428B0, reinterpret_cast<uintptr_t>(cTextLabel_CalcExtents_Hook),            "CalcExtentTextLabel");  // RET

    // ---- 나머지 "폭 계산만 하는" 1바이트 호출부 (툴팁 0x436a50/0x43dfc0, SprintfMeasure 0x476820,
    //      sneg FUN_59576cc0/FUN_59576ae0/SprintfMeasure_5955cb10 등) 는 vtable 에서 처리 ----
    InstallFontMetricHooks();
}
