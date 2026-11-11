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


// ---------------------------------------------------------------------------
// sneg FUN_562b5760 (0xB5760): 게임 0x4451f0 대체 함수 (알파 블렌딩, RGB565).
// 소스 서피스를 폭*높이 만큼 촘촘히(pitch 무시) 읽는데, 실제 서피스(예: 메인 메뉴 버전 라벨,
// 800x15)가 객체의 w/h(400x400) 보다 작으면 버퍼 끝을 넘어 읽어서 AV 가 난다
// (힙 배치에 따라 죽기도 하고 안 죽기도 함). AV 가 나면 이 프레임의 블렌딩만 건너뛰고
// 잠겨 있던 서피스를 풀어 준다.
// ---------------------------------------------------------------------------
typedef BYTE(__fastcall* tSnegAlphaBlend)(void* self);
static tSnegAlphaBlend g_origSnegAlphaBlend = nullptr;
static uintptr_t g_snegBase = 0;

static void SafeUnlock(void* surf)
{
    if (!surf) return;
    __try {
        void** vtbl = *reinterpret_cast<void***>(surf);
        typedef long(__stdcall* tUnlock)(void*, void*);
        reinterpret_cast<tUnlock>(vtbl[0x80 / 4])(surf, nullptr);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

static __declspec(noinline) void LogBlendFault(void* self)
{
    static int s_count = 0;
    if (s_count++ < 5)
        PLOG_ERROR << "[sneg] alpha blend (0xB5760) access violation swallowed, self=" << self;
}

static BYTE __fastcall SnegAlphaBlend_Hook(void* self)
{
    __try {
        return g_origSnegAlphaBlend(self);
    } __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        LogBlendFault(self);
        __try {
            void** vtbl = *reinterpret_cast<void***>(self);
            typedef void* (__fastcall* tGetSurf)(void*);
            SafeUnlock(reinterpret_cast<tGetSurf>(vtbl[0x54 / 4])(self));
            uintptr_t gameBase = *reinterpret_cast<uintptr_t*>(g_snegBase + 0x1E6E70);
            uintptr_t g = *reinterpret_cast<uintptr_t*>(gameBase + 0x327290);
            uintptr_t gfx = *reinterpret_cast<uintptr_t*>(g + 0x18);
            SafeUnlock(*reinterpret_cast<void**>(gfx + 0x34));
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
        return 1;
    }
}

// ---------------------------------------------------------------------------
// SNEG 의 CJK 분기는 코드페이지 950(번체 중국어, 0x3B6)에서만 동작한다.
// 한국어 언어팩(949, 0x3B5)에서도 같은 분기를 타도록 즉시값 950 을 949 로 바꾼다.
// (예전에는 sneg.dll / ddraw.dll 파일을 직접 고쳤었다. 배포판은 원본 파일을 건드리지 않으므로 메모리에서 패치)
// 각 rva 는 imm32 의 첫 바이트(B6 03 00 00) 위치.
// ---------------------------------------------------------------------------
static void PatchCodePage950To949(uintptr_t base, const uint32_t* rvas, size_t count, const char* module)
{
    int ok = 0;
    for (size_t i = 0; i < count; ++i) {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(base + rvas[i]);
        if (p[0] == 0xB6 && p[1] == 0x03 && p[2] == 0x00 && p[3] == 0x00) {
            patchBYTE(base + rvas[i], 0xB5);
            ++ok;
        }
        else if (p[0] == 0xB5 && p[1] == 0x03 && p[2] == 0x00 && p[3] == 0x00) {
            ++ok;                                   // 이미 949 (파일을 직접 고친 경우)
        }
        else {
            PLOG_ERROR << "[" << module << "] cp950 patch site mismatch at rva 0x" << std::hex << rvas[i];
        }
    }
    PLOG_DEBUG << "[" << module << "] cp950 -> 949 patched " << ok << "/" << count;
}

// sneg.dll: isChina(FUN_562b6690, cmp eax,950) 등 비교 3곳 + china 변환용 MultiByteToWideChar 코드페이지 PUSH 6곳
static const uint32_t kSnegCp950[] = { 0x46696, 0x46926, 0x46935, 0x4696D, 0xAE516, 0xAE58D, 0xAE5BD, 0xAE60C, 0xAE662 };
// ddraw.dll: FUN_5972b2d0 아틀라스 폭 조건(+0x4B5D4), FUN_5972ba20 CJK 목록 로드 조건(+0x4BBC7),
//            FUN_59760450 의 요청 구조체 코드페이지(+0x8091A)
static const uint32_t kDDrawCp950[] = { 0x4B5D4, 0x4BBC7, 0x8091A };

void ApplySNEGPatches(uintptr_t baseAddress)
{
    PLOG_DEBUG << "[SNEG.DLL] baseAddress " << std::hex << std::showbase << baseAddress;
#define MB(addr)    (baseAddress + (addr))

    PatchCodePage950To949(baseAddress, kSnegCp950, _countof(kSnegCp950), "sneg");

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

    // 알파 블렌딩 AV 방지 (위 SnegAlphaBlend_Hook 설명 참고)
    {
        g_snegBase = baseAddress;
        static const uint8_t kPrologue[] = { 0x55, 0x8B, 0xEC, 0x81, 0xEC, 0x20, 0x01, 0x00, 0x00 };
        if (memcmp(reinterpret_cast<void*>(MB(0x000B5760)), kPrologue, sizeof(kPrologue)) == 0) {
            MH_STATUS st = MH_Initialize();
            if (st == MH_OK || st == MH_ERROR_ALREADY_INITIALIZED) {
                st = MH_CreateHook(reinterpret_cast<LPVOID>(MB(0x000B5760)), &SnegAlphaBlend_Hook,
                                   reinterpret_cast<LPVOID*>(&g_origSnegAlphaBlend));
                if (st == MH_OK) st = MH_EnableHook(reinterpret_cast<LPVOID>(MB(0x000B5760)));
            }
            PLOG_DEBUG << "[sneg] alpha blend guard hook status=" << st;
        } else {
            PLOG_ERROR << "[sneg] 0xB5760 prologue mismatch, alpha blend guard not installed";
        }
    }

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

// ---------------------------------------------------------------------------
// ddraw.dll (SNEG UI, Dear ImGui) CJK 글꼴 / 글자 목록 교체 (한국어 언어팩)
//
// FUN_5972b2d0 (ddraw +0x4B2D0): 코드페이지 949 일 때 CJK 글자 목록 파일의 글자를
//   <%WINDIR%\Fonts>/msjh.ttc  (없으면 <%WINDIR%\Fonts>/msyh.ttf)
// 로 ImGui 아틀라스에 넣는다. 둘 다 한글이 없어서 '?' 로 나온다.
//   5972b422: 68 64 DC 82 59      PUSH "/"
//   5972b427: FF B5 24 FF FF FF   PUSH [EBP-0xDC]   (폰트 폴더 std::string*)  -> PUSH <언어팩 폴더>; NOP
//   5972b439: 6A 08 / 68 <msjh>   PUSH 8 / PUSH "msjh.ttc"                    -> "MaruBuri-Regular.ttf"
//   5972b500: 6A 08 / 68 <msyh>   PUSH 8 / PUSH "msyh.ttf"  (폴더는 %WINDIR%\Fonts 그대로) -> "malgun.ttf"
// 즉 1순위 <게임>\languages\ko\MaruBuri-Regular.ttf, 없으면 시스템 맑은 고딕.
//
// CJK 글자 목록 경로: languages/zh-hant/... -> languages/ko/...
//   5972bd79: 6A 2C / 68 <...>    PUSH 0x2C / PUSH "languages/zh-hant/sneg_ui_cjk_characters.txt"
// ---------------------------------------------------------------------------
static const char g_szCjkFont[]     = "MaruBuri-Regular.ttf";
static const char g_szCjkFallback[] = "malgun.ttf";
static const char g_szCjkList[]     = "languages/ko/sneg_ui_cjk_characters.txt";

// MSVC x86 std::string 과 같은 배치 (FUN_5971bff0 이 읽기만 한다). 길이 16 이상이면 +0 이 포인터
struct MsvcStringView {
    const char* ptr;
    char        pad[12];
    size_t      size;       // +0x10
    size_t      capacity;   // +0x14
};
static char           g_cjkFontDir[MAX_PATH];
static MsvcStringView g_cjkFontDirStr = {};

static void PatchPushLenStr(uintptr_t site, uint32_t expectStr, const char* expectText, BYTE expectLen,
                            const char* newText, const char* what)
{
    const uint8_t* p = reinterpret_cast<const uint8_t*>(site);
    if (p[0] == 0x6A && p[1] == expectLen && p[2] == 0x68 &&
        *reinterpret_cast<const uint32_t*>(p + 3) == expectStr &&
        memcmp(reinterpret_cast<const void*>(expectStr), expectText, strlen(expectText) + 1) == 0)
    {
        patchBYTE(site + 1, static_cast<BYTE>(strlen(newText)));
        patchDWORD(site + 3, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(newText)));
        PLOG_DEBUG << "[ddraw] " << what << " -> " << newText;
    }
    else {
        PLOG_ERROR << "[ddraw] " << what << " patch site mismatch, not patched";
    }
}

// <게임 폴더>\languages\ko (ImGui 는 경로를 UTF-8 로 열고 존재 확인은 ANSI 라서, 비 ASCII 경로면 8.3 짧은 이름 사용)
static bool BuildCjkFontDir()
{
    char exe[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, exe, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return false;
    char* slash = strrchr(exe, '\\');
    if (!slash) return false;
    *slash = '\0';

    char dir[MAX_PATH];
    if (wsprintfA(dir, "%s\\languages\\ko", exe) <= 0) return false;

    bool ascii = true;
    for (const char* c = dir; *c; ++c)
        if (static_cast<unsigned char>(*c) >= 0x80) { ascii = false; break; }
    if (!ascii) {
        char shortDir[MAX_PATH];
        DWORD m = GetShortPathNameA(dir, shortDir, MAX_PATH);
        if (m == 0 || m >= MAX_PATH) return false;
        lstrcpyA(dir, shortDir);
    }
    lstrcpyA(g_cjkFontDir, dir);
    g_cjkFontDirStr.ptr = g_cjkFontDir;
    g_cjkFontDirStr.size = strlen(g_cjkFontDir);
    g_cjkFontDirStr.capacity = (g_cjkFontDirStr.size < 16) ? 16 : g_cjkFontDirStr.size;
    return g_cjkFontDirStr.size >= 16;      // 16 미만이면 SSO 배치라 포인터 방식이 안 맞음 (실제로는 없음)
}

// [진단] ddraw 게임폰트/retail 글꼴 슬롯 상태가 바뀔 때마다 error.log 에 기록. 배포 전 제거
//   +0x31A828 + i*0x200C : [0]=sneg 글리프표 받음, [1]=bit1(arial 경로), [2]=bit2(PoRGothic 경로)
//   +0x31A384 + i*0x3C   : 글꼴 모드 (3 = retailfont 사용 가능 판정에 쓰임)
//   +0x31A638 + i*0x3C   : 외곽선 글꼴 로드 시도함
static DWORD WINAPI DiagDDrawFontSlots(LPVOID p)
{
    const uintptr_t base = reinterpret_cast<uintptr_t>(p);
    char last[256] = {};
    for (;;) {
        char cur[256];
        int n = 0;
        for (int i = 0; i < 8; ++i) {
            const uint8_t* g = reinterpret_cast<const uint8_t*>(base + 0x31A828 + i * 0x200C);
            const int mode = *reinterpret_cast<const int*>(base + 0x31A384 + i * 0x3C);
            const uint8_t tried = *reinterpret_cast<const uint8_t*>(base + 0x31A638 + i * 0x3C);
            n += wsprintfA(cur + n, "%d:%d%d%d/m%d/o%d ", i, g[0], g[1], g[2], mode, tried);
        }
        if (lstrcmpA(cur, last) != 0) {
            lstrcpyA(last, cur);
            PLOG_DEBUG << "[diag] ddraw font slots " << cur;
        }
        Sleep(300);
    }
}

void ApplyDDrawPatches(uintptr_t baseAddress)
{
    CloseHandle(CreateThread(NULL, 0, DiagDDrawFontSlots, reinterpret_cast<LPVOID>(baseAddress), 0, NULL));

    PatchCodePage950To949(baseAddress, kDDrawCp950, _countof(kDDrawCp950), "ddraw");

    PatchPushLenStr(baseAddress + 0x4BD79, static_cast<uint32_t>(baseAddress + 0x14DA60),
                    "languages/zh-hant/sneg_ui_cjk_characters.txt", 0x2C, g_szCjkList, "CJK list");

    // 1순위 글꼴: 폴더를 언어팩 폴더로 바꾸고 파일 이름을 MaruBuri 로
    const uintptr_t dirSite = baseAddress + 0x4B427;
    static const uint8_t kPushFolder[] = { 0xFF, 0xB5, 0x24, 0xFF, 0xFF, 0xFF };
    if (memcmp(reinterpret_cast<const void*>(dirSite), kPushFolder, sizeof(kPushFolder)) == 0 && BuildCjkFontDir()) {
        uint8_t code[6] = { 0x68, 0, 0, 0, 0, 0x90 };      // PUSH imm32; NOP
        const uint32_t addr = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&g_cjkFontDirStr));
        memcpy(code + 1, &addr, 4);
        patchBytes(dirSite, code, sizeof(code));
        PLOG_DEBUG << "[ddraw] CJK font dir -> " << g_cjkFontDir;
        PatchPushLenStr(baseAddress + 0x4B439, static_cast<uint32_t>(baseAddress + 0x14DA90),
                        "msjh.ttc", 0x08, g_szCjkFont, "CJK font");
    }
    else {
        PLOG_ERROR << "[ddraw] CJK font dir patch site mismatch, not patched";
    }

    // 2순위 글꼴: 시스템 폰트 폴더의 맑은 고딕
    PatchPushLenStr(baseAddress + 0x4B500, static_cast<uint32_t>(baseAddress + 0x14DA9C),
                    "msyh.ttf", 0x08, g_szCjkFallback, "CJK fallback font");
}
