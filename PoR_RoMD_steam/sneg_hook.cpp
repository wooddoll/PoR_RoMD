#include "por2.h"

#include <MinHook.h>
#include "help_util.h"

int __cdecl cTextManager_DrawMenuTrunc_Hook(cTextManager* self, int lineIdx, char* buf, char* flag);
BOOL __fastcall cTextManager_MenuDraw_Hook(cTextManager* self, void* /*edx*/);

BOOL __cdecl cTextLabel_DrawText_Hook(cTextLabel* self, IDirectDrawSurface7* pDst);

BOOL __fastcall cTextLayout_WordWrap_Hook(cTextLayout* self, void* /*edx*/);
BOOL __fastcall cTextLayout_RenderShadow_Hook(cTextLayout* self, void* /*edx*/);
BOOL __fastcall cTextManager_DrawTextLines_Hook(cTextManager* self, void* /*edx*/, int /*unusedStackArg*/);
BOOL __fastcall CKeyInput_Draw_Hook(CKeyInput* self, void* /*edx*/);
BOOL __fastcall cTextManager_DrawHMenu_Hook(cTextManager* self, void* /*edx*/, int /*unusedStackArg*/);

void ApplySNEGPatches(uintptr_t baseAddress) 
{
    PLOG_DEBUG << "[SNEG.DLL] baseAddress " << std::hex << std::showbase << baseAddress;
#define MB(addr)    (baseAddress + (addr))

    // sneg.dll 내 FUN_562758b0 (오프셋 0x000C58B0): float 0.0f를 반환하도록 패치
    // x86 FPU: FLDZ (D9 EE) / RET (C3)
    uint8_t patch_return_zero_float[] = { 0xD9, 0xEE, 0xC3 }; // fldz; ret
    patchBytes(MB(0x000C58B0), patch_return_zero_float, sizeof(patch_return_zero_float));
    // FUN_59525830 (0x000C5830): TextLabel 용 배율. DrawTextTextLabel_59527720 / FUN_59527480 이 사용.
    // 0 보다 크면 DrawText_ani_59528020 (china 실시간 렌더 경로) 로 빠지므로 같이 0 으로 고정 (fldz; ret)
    patchBytes(MB(0x000C5830), patch_return_zero_float, sizeof(patch_return_zero_float));
    // sneg.dll의 FUN_5627a050 진입을 막아 메뉴/텍스트 3D 오버레이 후킹 등록 차단
    // (주의) ddraw.dll DllMain 이 sneg 로드 직후 SNEG_Load 를 호출해 등록이 끝난 뒤에 이 코드가 돌므로
    //        실제로는 효과가 없다. 그래서 아래에서 sneg 대체 함수들을 직접 훅함 (DrawMenu/DrawTextLines/FUN_59529c50)
    patchBYTE(MB(0x000CA050), 0xC3); // ret

    HookFunction(MB(0x000CC600), reinterpret_cast<uintptr_t>(CKeyInput_Draw_Hook));

    HookFunction(MB(0x000C9EA0), reinterpret_cast<uintptr_t>(cTextManager_MenuDraw_Hook)); // DrawMenu_59529ea0 가로채기
    
    HookFunction(MB(0x000C9D90), reinterpret_cast<uintptr_t>(cTextManager_DrawTextLines_Hook));  // DrawTextLines_56279d90

    HookFunction(MB(0x000C9C50), reinterpret_cast<uintptr_t>(cTextManager_DrawHMenu_Hook));      // FUN_59529c50 (game 0x43f1b0, thiscall RET 4)

    HookFunction(MB(0x000C7CD0), reinterpret_cast<uintptr_t>(cTextLabel_DrawText_Hook));   // DrawText_59527cd0

    HookFunction(MB(0x00047F70), reinterpret_cast<uintptr_t>(cTextLayout_WordWrap_Hook));   // WordWrap_594a7f70
    HookFunction(MB(0x000B5AA0), reinterpret_cast<uintptr_t>(cTextLayout_RenderShadow_Hook));   // RenderShadow_59515aa0
    
    

    //subDrawMenu_5952ac20 - 59460000 = CAC20
    // 5952ae7e ~ 5952ae8A (총 13바이트)
    // 기존 헥스(Hex): 0F BE C1 8B CB 50 8B 13 8B 42 24 FF D0
    // 변경 헥스(Hex): 66 0F 7E C0 50 8B CB 8B 13 FF 52 24 90
    uint8_t patch_subDrawMenu[] = {0x66, 0x0F, 0x7E, 0xC0, 0x50, 0x8B, 0xCB, 0x8B, 0x13, 0xFF, 0x52, 0x24, 0x90};
//    patchBytes(MB(0x000CAE7E), patch_subDrawMenu, sizeof(patch_subDrawMenu));

    //RenderShadow_59515aa0 - 59460000 = B5AA0
    // 패치 주소: 59515d43 ~ 59515d57 (총 21바이트)
    // 기존 헥스(Hex): 0F BE C1 8B 8D D4 FE FF FF 50 8B 85 C4 FE FF FF 8B 40 24 FF D0
    // 변경 헥스(Hex): 66 0F 7E C0 8B 8D D4 FE FF FF 50 8B 85 C4 FE FF FF FF 50 24 90
    uint8_t patch_RenderShadow[] = {0x66, 0x0F, 0x7E, 0xC0, 0x8B, 0x8D, 0xD4, 0xFE, 0xFF, 0xFF, 0x50, 0x8B, 0x85, 0xC4, 0xFE, 0xFF, 0xFF, 0xFF, 0x50, 0x24, 0x90};
//    patchBytes(MB(0x000B5D43), patch_RenderShadow, sizeof(patch_RenderShadow));

    //DrawText_59527cd0 - 59460000 = C7CD0
    //패치 주소: 59527e9a ~ 59527eb0 (총 23바이트)
    //기존 헥스(Hex): 8B 85 FC FE FF FF 8B 10 0F BE C1 8B 8D FC FE FF FF 50 8B 42 24 FF D0
    //변경 헥스(Hex): 66 0F 7E C0 8B 8D FC FE FF FF 50 8B 11 FF 52 24 90 90 90 90 90 90 90
    uint8_t patch_DrawText[] = {0x66, 0x0F, 0x7E, 0xC0, 0x8B, 0x8D, 0xFC, 0xFE, 0xFF, 0xFF, 0x50, 0x8B, 0x11, 0xFF, 0x52, 0x24, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
//    patchBytes(MB(0x000C7E9A), patch_DrawText, sizeof(patch_DrawText));

    //DrawText_ani_59528020 패치 필요없음
    //subDrawMenu_ani_59528e60  // 다시 확인
    //DrawMenuTrunc_595293b0    // 다시 확인

    //DrawTextLines_59529d90
    // => FUN_5952a4d0
    // 1. MakeChinaFont 블록 진입 차단 (1바이트 수정) 가장 먼저 MakeChinaFont를 감싸고 있는 첫 번째 JZ를 무조건 점프(JMP)로 변경하여, 해당 블록 전체를 실행되지 않는 '빈 공간(Dead Code)'으로 만듭니다.
    // 주소: 5952a671 - 59460000 = CA671
    // 기존 헥스: 74 (JZ LAB_5952a69c)
    // 수정 헥스: EB (JMP LAB_5952a69c)
//    patchBYTE(MB(0x000CA671), 0xEB);
    // 2. 확보된 빈 공간에 새로운 로직 작성 (20바이트 덮어쓰기) 실행되지 않게 된 5952a673부터의 공간에 우리가 원하는 charCode 로드 로직을 작성하고, 원래 실행되어야 할 함수 호출부(5952a6bd)로 돌아가는 점프문을 넣습니다.
    // 주소: 5952a673 - 59460000 = CA673
    // 기존 헥스 (20바이트): 84 C0 74 25 68 FB 24 00 00 57 53 FF 75 88 8D 85 A8 FE FF FF
    // 수정 헥스 (20바이트): 8B BD F0 FE FF FF 8B 85 A8 FE FF FF 8B CF 50 E9 36 00 00 00 (어셈블리 의미: EDI 세팅 -> EAX에 charCode 로드 -> ECX 세팅 -> EAX 푸시 -> 5952a6bd로 JMP)
    uint8_t patch2_FUN_5952a4d0[] = {0x8B, 0xBD, 0xF0, 0xFE, 0xFF, 0xFF, 0x8B, 0x85, 0xA8, 0xFE, 0xFF, 0xFF, 0x8B, 0xCF, 0x50, 0xE9, 0x36, 0x00, 0x00, 0x00};
//    patchBytes(MB(0x000CA673), patch2_FUN_5952a4d0, sizeof(patch2_FUN_5952a4d0));
    // 3. 기존 GetCharRect 인자 세팅부를 빈 공간으로 점프시키기 (12바이트 덮어쓰기) 원래 문제가 되었던 originalText[0] 로드 구간을 방금 만든 빈 공간(5952a673)으로 점프하도록 수정하고, 남는 공간은 NOP으로 깔끔하게 채웁니다.
    // 주소: 5952a6b1 - 59460000 = CA6B1
    // 기존 헥스 (12바이트): 8B BD F0 FE FF FF 0F BE C1 8B CF 50
    // 수정 헥스 (12바이트): E9 BD FF FF FF 90 90 90 90 90 90 90 (어셈블리 의미: 5952a673으로 JMP -> 나머지 7바이트는 NOP 처리)
    uint8_t patch3_FUN_5952a4d0[] = {0xE9, 0xBD, 0xFF, 0xFF, 0xFF, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
//    patchBytes(MB(0x000CA6B1), patch3_FUN_5952a4d0, sizeof(patch3_FUN_5952a4d0));

    // FUN_594a7ca0 - 59460000 = 47CA0
    // 패치 주소: 595236d4 ~ 595236d7 (총 4바이트)
    // 기존 헥스(Hex): 0F B6 41 10
    // 변경 헥스(Hex): 8B 01 90 90
    uint8_t patch_FUN_594a7ca0[] = {0x8B, 0x01, 0x90, 0x90};
//    patchBytes(MB(0x000C36D4), patch_FUN_594a7ca0, sizeof(patch_FUN_594a7ca0));

    // _2_china_FUN_59524580
    // 첫 번째 인자 (param_2) 수정
    // 패치 주소: 595245b7 ~ 595245ba (총 4바이트)
    // 기존 헥스(Hex): 0F B6 41 10 (MOVZX EAX, byte ptr [ECX+0x10])
    // 변경 헥스(Hex): 8B 01 90 90 (MOV EAX, dword ptr [ECX] / NOP / NOP)
    uint8_t patch1_FUN_59524580[] = {0x8B, 0x01, 0x90, 0x90};
//    patchBytes(MB(0x000C45B7), patch1_FUN_59524580, sizeof(patch1_FUN_59524580));
    // 두 번째 인자 (param_3) 수정
    // 패치 주소: 595245cc ~ 595245cf (총 4바이트)
    // 기존 헥스(Hex): 0F B6 46 10 (MOVZX EAX, byte ptr [ESI+0x10])
    // 변경 헥스(Hex): 8B 06 90 90 (MOV EAX, dword ptr [ESI] / NOP / NOP)
    uint8_t patch2_FUN_59524580[] = {0x8B, 0x06, 0x90, 0x90};
//    patchBytes(MB(0x000C45CC), patch2_FUN_59524580, sizeof(patch2_FUN_59524580));


    //_V_china_FUN_59522b70
    //주소: 59522ce7 - 59460000 = C2CE7
    //기존: FF 75 E8 (PUSH dword ptr [EBP - 0x18] -> originalText)
    //수정: FF 75 C0 (PUSH dword ptr [EBP - 0x40] -> charCode)
//    patchBYTE(MB(0x000C2CE9), 0xC0);   // PUSH dword ptr [EBP - 0x40] (charCode)
    //주소: 59522d6d - 59460000 = C2D6D
    //기존: 8B 45 E8 (MOV EAX, dword ptr [EBP - 0x18])
    //수정: 8B 45 C0 (MOV EAX, dword ptr [EBP - 0x40])
//    patchBYTE(MB(0x000C2D6F), 0xC0);   // MOV EAX, dword ptr [EBP - 0x40] (charCode)
    //주소: 59522df3 - 59460000 = C2DF3
    //기존: 8B 45 E8 (MOV EAX, dword ptr [EBP - 0x18])
    //수정: 8B 45 C0 (MOV EAX, dword ptr [EBP - 0x40])
//    patchBYTE(MB(0x000C2DF5), 0xC0);   // MOV EAX, dword ptr [EBP - 0x40] (charCode)

    //FUN_595249a0
    //시작 주소: 59524e35 - 59460000 = C4E35
    // 기존 헥스 (23바이트): 8B 85 D4 FE FF FF 8B 10 0F BE C1 8B 8D D4 FE FF FF 50 8B 42 24 FF D0
    // 수정 헥스 (23바이트): 8B 8D D4 FE FF FF 8B 11 0F BE 85 44 FE FF FF 50 8B 42 24 FF D0 90 90
    uint8_t patch1_FUN_595249a0[] = {0x8D, 0xD4, 0xFE, 0xFF, 0xFF, 0x8B, 0x11, 0x0F, 0xBE, 0x85, 0x44, 0xFE, 0xFF, 0xFF, 0x50, 0x8B, 0x42, 0x24, 0xFF, 0xD0, 0x90, 0x90};
//    patchBytes(MB(0x000C4E36), patch1_FUN_595249a0, sizeof(patch1_FUN_595249a0));
    //주소: 59524e94 - 59460000 = C4E94
    //기존 헥스: FF B5 A8 FE FF FF (PUSH dword ptr [EBP - 0x158] -> originalText)
    //수정 헥스: FF B5 44 FE FF FF (PUSH dword ptr [EBP - 0x1BC] -> charCode)
//    patchBYTE(MB(0x000C4E96), 0x44);

    //FUN_59523f90
    //주소: 59523fca - 59460000 = C3FCA
    //기존 헥스: 0F BE 46 10 (MOVSX EAX, byte ptr [ESI + 0x10])
    //수정 헥스: 0F BE 46 00 (MOVSX EAX, byte ptr [ESI + 0x00])
//    patchBYTE(MB(0x000C3FCD), 0x00);
    //주소: 59523fe7 - 59460000 = C3FE7
    //기존 헥스: 0F B6 46 10 (MOVZX EAX, byte ptr [ESI + 0x10])
    //수정 헥스: 0F B6 46 00 (MOVZX EAX, byte ptr [ESI + 0x00])
//    patchBYTE(MB(0x000C3FEA), 0x00);

    

}
