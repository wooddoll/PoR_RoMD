#if !GLYPH_TRACKING

#include "por2.h"

#include <shlwapi.h> // PathFindFileNameA 등을 사용하기 위해 포함 (필요시 shlwapi.lib 링크)
#pragma comment(lib, "Shlwapi.lib")

#include <MinHook.h>
#include "help_util.h"

RECT g_dummyZeroRect = { 0, 0, 0, 0 };

int __cdecl Input_Hook(int /*a1*/, int /*a2*/, int ch);

int __cdecl cTextManager_DrawMenuTrunc_Hook(cTextManager* self, int lineIdx, char* buf, char* flag);
BOOL __cdecl cTextManager_MenuDraw_Hook(cTextManager* self);

BOOL __fastcall cTextManager_CalculateTextExtents_Hook(cTextManager* self, void* /*edx*/);

BOOL __fastcall cTextLabel_CalcExtents_Hook(cTextLabel* self, void* /*edx*/);
BOOL __cdecl cTextLabel_DrawText_Hook(cTextLabel* self, IDirectDrawSurface7* pDst);

BOOL __fastcall cTextLayout_WordWrap_Hook(cTextLayout* self, void* /*edx*/);
BOOL __fastcall cTextLayout_RenderShadow_Hook(cTextLayout* self, void* /*edx*/);

cGlyphResult* __cdecl china_pass_Hook(cGlyphResult* out_result, cFont* pFont, const char* text, int textLen);



void ApplyMemoryPatches(uintptr_t baseAddress) {
#define MB(addr)    (baseAddress + (addr))

    //patchDWORD(0x00622E24, reinterpret_cast<uintptr_t>(CKeyInput_Draw_Hook)); // TODO
    PatchRelativeCall(0x0049EDCE, reinterpret_cast<uintptr_t>(Input_Hook));

    HookFunction(MB(0x000468D0), reinterpret_cast<uintptr_t>(china_pass_Hook));   // china_pass_594a68d0

    //HookFunction(MB(0x000C93B0), reinterpret_cast<uintptr_t>(cTextManager_DrawMenuTrunc_Hook));   // FUN_595293b0
    //HookFunction(MB(0x000CAC20), reinterpret_cast<uintptr_t>(cTextManager_MenuDraw_Hook));   // FUN_5952ac20    

    PatchRelativeCall(0x0043c614, reinterpret_cast<uintptr_t>(cTextManager_CalculateTextExtents_Hook));
    PatchRelativeCall(0x0043e9fd, reinterpret_cast<uintptr_t>(cTextManager_CalculateTextExtents_Hook));
    //PatchRelativeCall(0x0043c73f, reinterpret_cast<uintptr_t>(cTextManager_DrawTextLines_Hook));  // TODO

    patchDWORD(0x00623040, reinterpret_cast<uintptr_t>(cTextLabel_CalcExtents_Hook));
    //HookFunction(MB(0x000C7CD0), reinterpret_cast<uintptr_t>(cTextLabel_DrawText_Hook));   // DrawText_59527cd0

    //HookFunction(MB(0x00047F70), reinterpret_cast<uintptr_t>(cTextLayout_WordWrap_Hook));   // WordWrap_594a7f70
    //HookFunction(MB(0x000B5AA0), reinterpret_cast<uintptr_t>(cTextLayout_RenderShadow_Hook));   // RenderShadow_59515aa0
}
