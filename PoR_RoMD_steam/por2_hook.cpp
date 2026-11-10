#include "por2.h"

#include <MinHook.h>
#include "help_util.h"

RECT g_dummyZeroRect = { 0, 0, 0, 0 };

int __cdecl Input_Hook(int /*a1*/, int /*a2*/, int ch);
BOOL __fastcall cTextManager_CalculateTextExtents_Hook(cTextManager* self, void* /*edx*/);
BOOL __fastcall cTextLabel_CalcExtents_Hook(cTextLabel* self, void* /*edx*/);

void ApplyMemoryPatches(uintptr_t baseAddress) 
{
    UNREFERENCED_PARAMETER(baseAddress);

    uint8_t ESPtoEAX_I[5] = { 0x8b, 0x44, 0x24, 0x04, 0x90 };
    //patchBytes(0x00435e00, ESPtoEAX_I, 5); // GetCharHeight_00435e00
    //patchBytes(0x00435e30, ESPtoEAX_I, 5); // GetCharWidth_00435e30

    PatchRelativeCall(0x0049EDCE, reinterpret_cast<uintptr_t>(Input_Hook));

    //PatchRelativeCall(0x0043c614, reinterpret_cast<uintptr_t>(cTextManager_CalculateTextExtents_Hook));
    //PatchRelativeCall(0x0043e9fd, reinterpret_cast<uintptr_t>(cTextManager_CalculateTextExtents_Hook));

    //patchDWORD(0x00623040, reinterpret_cast<uintptr_t>(cTextLabel_CalcExtents_Hook));
}
