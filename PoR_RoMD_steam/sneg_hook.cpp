#include "por2.h"

#include <MinHook.h>
#include <algorithm>
#include <map>
#include <tuple>
#include <vector>
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

// ---------------------------------------------------------------------------
// [실험] SNEG 자체 CJK(china) 렌더 경로로 한글 출력
// 게임 폴더에 kr_native_test.txt 가 있으면: 950->949 패치 + sneg 의 china/고해상도 경로를 그대로 쓰고
// 우리 텍스트 훅(배율 0 고정, DrawMenu/DrawTextLines/HMenu/TextLabel/WordWrap/RenderShadow/KeyInput,
// 게임 입구 직접 훅, FontMetrics)은 모두 끈다. 파일을 지우면 원래 동작.
// ---------------------------------------------------------------------------
bool IsNativeCjkTest()
{
    static int s_state = -1;
    if (s_state < 0) {
        char path[MAX_PATH];
        DWORD n = GetModuleFileNameA(NULL, path, MAX_PATH);
        char* slash = (n > 0 && n < MAX_PATH) ? strrchr(path, '\\') : nullptr;
        if (slash) lstrcpyA(slash + 1, "kr_native_test.txt");
        s_state = (slash && GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) ? 1 : 0;
        PLOG_DEBUG << "[exp] native CJK test mode = " << s_state;
    }
    return s_state == 1;
}

// [실험] kr_native_test.txt 에 "hires" 가 있으면 고해상도(*_ani) 경로를 막지 않는다 (16절 2단계용).
// 없으면 1단계: 배율 함수 두 개를 0 으로 고정해서 저해상도(배율 1.0) 비트맵 경로만 쓴다.
static bool IsNativeCjkHiRes()
{
    static int s_state = -1;
    if (s_state < 0) {
        s_state = 0;
        char path[MAX_PATH];
        DWORD n = GetModuleFileNameA(NULL, path, MAX_PATH);
        char* slash = (n > 0 && n < MAX_PATH) ? strrchr(path, '\\') : nullptr;
        if (slash) {
            lstrcpyA(slash + 1, "kr_native_test.txt");
            HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
            if (f != INVALID_HANDLE_VALUE) {
                char buf[256] = {};
                DWORD rd = 0;
                ReadFile(f, buf, sizeof(buf) - 1, &rd, NULL);
                CloseHandle(f);
                _strlwr_s(buf);
                s_state = strstr(buf, "hires") ? 1 : 0;
            }
        }
        PLOG_DEBUG << "[exp] hi-res text = " << s_state;
    }
    return s_state == 1;
}

void InstallChinaGlyphHooks(uintptr_t snegBase, bool hiRes);   // china_glyph.cpp

// Wrap_ItemsCJK(+0x471E0)의 줄바꿈 후보 판정에서 U+2E80 이상 글자 조건을 끈다 → 공백/개행에서만 줄을 나눔.
// 후보가 없으면 원래 코드가 줄 폭에서 강제로 끊으므로 넘치지 않는다.
//   +0x4730C: 73 08 (JNC, 앞 글자 >= U+2E80 → 후보)        → 90 90
//   +0x47314: 72 73 (JC,  현재 글자 <  U+2E80 → 후보 아님)   → EB 73 (항상 후보 아님)
// ddraw.dll 에도 같은 코드의 복사본이 있다 (FUN_5972e150, +0x4E276). SNEG 내레이션(DM 대사) 상자가
// FUN_5972d520 에서 한글이 있으면 이 함수로 먼저 줄을 나눠 '\n' 을 넣고, 그리기에서 실제 폭으로 다시 나눈다
// → 단어 중간 1차 줄바꿈 + 공백 2차 줄바꿈으로 짧은 줄이 끼었다.
//   site = 첫 cmp 의 오프셋 (sneg 0x47306, ddraw 0x4E276)
static void PatchCjkWordWrap(uintptr_t base, uint32_t site, const char* tag)
{
    const uint8_t* p1 = reinterpret_cast<const uint8_t*>(base + site);
    const uint8_t* p2 = reinterpret_cast<const uint8_t*>(base + site + 8);
    static const uint8_t k1[] = { 0x81, 0xFE, 0x80, 0x2E, 0x00, 0x00, 0x73, 0x08 };   // cmp esi,0x2e80; jnc
    static const uint8_t k2[] = { 0x81, 0xFF, 0x80, 0x2E, 0x00, 0x00, 0x72, 0x73 };   // cmp edi,0x2e80; jc
    if (memcmp(p1, k1, sizeof(k1)) == 0 && memcmp(p2, k2, sizeof(k2)) == 0) {
        static const uint8_t nop2[] = { 0x90, 0x90 };
        patchBytes(base + site + 6, nop2, sizeof(nop2));
        patchBYTE(base + site + 14, 0xEB);
        PLOG_DEBUG << "[" << tag << "] CJK word wrap -> space only";
    } else {
        PLOG_ERROR << "[" << tag << "] CJK word wrap patch site mismatch";
    }
}

// sneg.dll: isChina(FUN_562b6690, cmp eax,950) 등 비교 3곳 + china 변환용 MultiByteToWideChar 코드페이지 PUSH 6곳
static const uint32_t kSnegCp950[] = { 0x46696, 0x46926, 0x46935, 0x4696D, 0xAE516, 0xAE58D, 0xAE5BD, 0xAE60C, 0xAE662 };
// ddraw.dll: FUN_5972b2d0 아틀라스 폭 조건(+0x4B5D4), FUN_5972ba20 CJK 목록 로드 조건(+0x4BBC7),
//            FUN_59760450 의 요청 구조체 코드페이지(+0x8091A)
static const uint32_t kDDrawCp950[] = { 0x4B5D4, 0x4BBC7, 0x8091A };

// [진단/실험] 고해상도 텍스트 배율 판단에 쓰이는 플래그와 배율 함수 결과를 주기적으로 기록
//   FUN_562758b0 (+0xC58B0) = DrawMenu 등 배율, FUN_56275830 (+0xC5830) = TextLabel 배율
//   조건: (ScaledEngineText || isChina) && c8 && ca && cb && [+0x1E44EF] && 배율 >= 1.01
static DWORD WINAPI DiagSnegScale(LPVOID p)
{
    const uintptr_t base = reinterpret_cast<uintptr_t>(p);
    typedef float(__cdecl* tScale)();
    typedef bool(__cdecl* tBool)();
    for (int i = 0; i < 6; ++i) {
        Sleep(i == 0 ? 3000 : 10000);
        const uint8_t* f = reinterpret_cast<const uint8_t*>(base + 0x1E43C8);
        float s1 = reinterpret_cast<tScale>(base + 0xC58B0)();
        bool china = reinterpret_cast<tBool>(base + 0x46690)();
        char buf[200];
        sprintf_s(buf, "c8=%d SET(c9)=%d ca=%d cb=%d ef=%d legacy=%d isChina=%d scaleMenu=%.3f",
                  f[0], f[1], f[2], f[3], *reinterpret_cast<const uint8_t*>(base + 0x1E44EF),
                  *reinterpret_cast<const uint8_t*>(base + 0x23926C), china, s1);
        PLOG_DEBUG << "[diag] sneg scale " << buf;
    }
    return 0;
}

// sneg.dll 로드 직후(SNEG_Load 전) DLL 로드 알림에서 호출됨 (dllmain.cpp). 로거가 아직 없을 수 있음
void EarlySnegPatch(uintptr_t baseAddress)
{
    PatchCodePage950To949(baseAddress, kSnegCp950, _countof(kSnegCp950), "sneg-early");
}

// [실험] sneg 의 CreateFontW (IAT) 를 가로채 china 글꼴(맑은 고딕) 높이를 줄인다.
// 맑은 고딕은 같은 높이에서 JhengHei 보다 커서 글자 칸을 넘어 잘린다.
typedef HFONT(WINAPI* tCreateFontW)(int, int, int, int, int, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, LPCWSTR);
static tCreateFontW g_origCreateFontW = nullptr;
static int g_cjkFontScalePercent = 71;

static HFONT WINAPI Sneg_CreateFontW_Hook(int h, int w, int esc, int ori, int weight, DWORD italic, DWORD underline,
                                          DWORD strike, DWORD charset, DWORD outPrec, DWORD clipPrec, DWORD quality,
                                          DWORD pitch, LPCWSTR face)
{
    if (charset == HANGUL_CHARSET && face &&
        (lstrcmpiW(face, L"MaruBuri Regular") == 0 || lstrcmpiW(face, L"Malgun Gothic") == 0)) {
        const int nh = h * g_cjkFontScalePercent / 100;
        static int s_logged = 0;
        if (s_logged++ < 10)
            PLOG_DEBUG << "[exp] china CreateFontW height " << h << " -> " << nh;
        h = nh;
    }
    return g_origCreateFontW(h, w, esc, ori, weight, italic, underline, strike, charset, outPrec, clipPrec, quality, pitch, face);
}

static void HookSnegCreateFontW(uintptr_t baseAddress)
{
    // IAT 슬롯 sneg+0x1A7068 (= 0x56357068, CreateFontW)
    const uintptr_t slot = baseAddress + 0x1A7068;
    void* real = reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"gdi32.dll"), "CreateFontW"));
    if (*reinterpret_cast<void**>(slot) == real && real) {
        g_origCreateFontW = reinterpret_cast<tCreateFontW>(real);
        patchDWORD(slot, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&Sneg_CreateFontW_Hook)));
        // 배율: 게임 폴더 kr_native_test.txt 첫 줄에 숫자(%)가 있으면 그 값
        char path[MAX_PATH];
        DWORD n = GetModuleFileNameA(NULL, path, MAX_PATH);
        char* slash = (n > 0 && n < MAX_PATH) ? strrchr(path, '\\') : nullptr;
        if (slash) {
            lstrcpyA(slash + 1, "kr_native_test.txt");
            HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
            if (f != INVALID_HANDLE_VALUE) {
                char buf[16] = {};
                DWORD rd = 0;
                ReadFile(f, buf, sizeof(buf) - 1, &rd, NULL);
                CloseHandle(f);
                int v = atoi(buf);
                if (v >= 50 && v <= 150) g_cjkFontScalePercent = v;
            }
        }
        PLOG_DEBUG << "[exp] sneg CreateFontW hooked, scale " << g_cjkFontScalePercent << "%";
    } else {
        PLOG_ERROR << "[exp] sneg CreateFontW IAT slot mismatch";
    }
}

void ApplySNEGPatches(uintptr_t baseAddress)
{
    PLOG_DEBUG << "[SNEG.DLL] baseAddress " << std::hex << std::showbase << baseAddress;
#define MB(addr)    (baseAddress + (addr))

    PatchCodePage950To949(baseAddress, kSnegCp950, _countof(kSnegCp950), "sneg");

    if (IsNativeCjkTest()) {
        // DrawMenu 그룹(FUN_5627a050)은 SNEG_Load 때 isChina(950 기준)로 판단해 이미 건너뛰었을 수 있다.
        // 이제 isChina 가 949 에서도 참이므로 직접 다시 호출해 등록한다 (이미 등록돼 있으면 생략).
        if (*reinterpret_cast<const uint8_t*>(0x0043B4F0) != 0xE9) {
            typedef void(__cdecl* tReg)();
            reinterpret_cast<tReg>(MB(0x000CA050))();
            PLOG_DEBUG << "[exp] sneg DrawMenu group registered: " << (*reinterpret_cast<const uint8_t*>(0x0043B4F0) == 0xE9);
        } else {
            PLOG_DEBUG << "[exp] sneg DrawMenu group already registered";
        }
        // china 실시간 렌더 글꼴 (FUN_561f6740 → FUN_561f6d90 → CreateFontW):
        //   561f67b4: 68 <L"Microsoft JhengHei">  561f67b9: 68 88 00 00 00 (CHINESEBIG5_CHARSET)
        //   561f67ca: C7 45 EC 93 7D 00 00  (글리프 확인 문자 U+7D93 經, 없으면 L"Microsoft YaHei" 로)
        //   561f6805: 68 <L"Microsoft YaHei">
        // 둘 다 한글이 없어서 '?' → 1순위 마루 부리(언어팩 폴더, 비공개 등록) / 2순위 맑은 고딕, HANGUL_CHARSET, 확인 문자 U+AC00
        {
            static const wchar_t kFace[] = L"MaruBuri Regular";      // GDI 패밀리 이름 (name ID 1)
            static const wchar_t kFallbackFace[] = L"Malgun Gothic";
            {
                wchar_t ttf[MAX_PATH];
                DWORD n = GetModuleFileNameW(NULL, ttf, MAX_PATH);
                wchar_t* slash = (n > 0 && n < MAX_PATH) ? wcsrchr(ttf, L'\\') : nullptr;
                int added = 0;
                if (slash && (slash - ttf) + 40 < MAX_PATH) {
                    lstrcpyW(slash + 1, L"languages\\ko\\MaruBuri-Regular.ttf");
                    added = AddFontResourceExW(ttf, FR_PRIVATE, 0);
                }
                PLOG_DEBUG << "[exp] AddFontResourceEx(MaruBuri) = " << added;
            }
            const uint8_t* p1 = reinterpret_cast<const uint8_t*>(MB(0x467B4));
            const uint8_t* p2 = reinterpret_cast<const uint8_t*>(MB(0x467CA));
            const uint8_t* p3 = reinterpret_cast<const uint8_t*>(MB(0x46805));
            if (p1[0] == 0x68 && p1[5] == 0x68 && *reinterpret_cast<const uint32_t*>(p1 + 6) == 0x88 &&
                p2[0] == 0xC7 && p2[1] == 0x45 && p2[2] == 0xEC && *reinterpret_cast<const uint32_t*>(p2 + 3) == 0x7D93 &&
                p3[0] == 0x68)
            {
                const uint32_t face = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(kFace));
                patchDWORD(MB(0x467B5), face);
                patchDWORD(MB(0x467BA), 0x81);          // HANGUL_CHARSET
                patchDWORD(MB(0x467CD), 0xAC00);        // '가'
                patchDWORD(MB(0x46806), static_cast<uint32_t>(reinterpret_cast<uintptr_t>(kFallbackFace)));
                PLOG_DEBUG << "[exp] china font -> MaruBuri (fallback Malgun Gothic) / HANGUL_CHARSET";
                HookSnegCreateFontW(baseAddress);
            } else {
                PLOG_ERROR << "[exp] china font patch site mismatch";
            }
        }
        // 16절 1단계: china_pass / GetChinaFontBitmap 훅 (한글 = 비트맵 폭 + 비트맵 마스크), CJK 줄바꿈은 공백에서만
        InstallChinaGlyphHooks(baseAddress, IsNativeCjkHiRes());
        PatchCjkWordWrap(baseAddress, 0x47306, "exp");
        if (!IsNativeCjkHiRes()) {
            // 저해상도만: 배율 함수 두 개를 0 으로 (본선과 같은 위치에서 비교하기 위해)
            static const uint8_t kZero[] = { 0xD9, 0xEE, 0xC3 }; // fldz; ret
            patchBytes(MB(0x000C58B0), kZero, sizeof(kZero));
            patchBytes(MB(0x000C5830), kZero, sizeof(kZero));
        }
        CloseHandle(CreateThread(NULL, 0, DiagSnegScale, reinterpret_cast<LPVOID>(baseAddress), 0, NULL));
        return;     // 우리 텍스트 훅, 알파 블렌딩 가드는 생략
    }

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
static char       g_szCjkFont[64]   = "MaruBuri-Regular.ttf";   // language.ini [Font] UiFontFile 로 바꿀 수 있음
static const char g_szCjkFallback[] = "malgun.ttf";

// <게임>\languages\ko\language.ini 경로
static bool GetKoLanguageIni(char* path, size_t cch)
{
    DWORD n = GetModuleFileNameA(NULL, path, static_cast<DWORD>(cch));
    char* slash = (n > 0 && n < cch) ? strrchr(path, '\\') : nullptr;
    if (!slash || static_cast<size_t>(slash - path) + 32 >= cch)
        return false;
    lstrcpyA(slash + 1, "languages\\ko\\language.ini");
    return true;
}

static int KoIniInt(const char* section, const char* key, int def, int lo, int hi)
{
    char ini[MAX_PATH];
    if (!GetKoLanguageIni(ini, MAX_PATH))
        return def;
    const int v = static_cast<int>(GetPrivateProfileIntA(section, key, def, ini));
    return (v >= lo && v <= hi) ? v : def;
}

static uint32_t FloatBits(float f)
{
    uint32_t u;
    memcpy(&u, &f, sizeof(u));
    return u;
}

// SNEG UI(ImGui) CJK 글꼴 (16절 3단계). FUN_5972b2d0 에서 AddFontFromFileTTF(path, 48.0f, cfg{OversampleH=1,V=1}, ranges).
// 툴팁 등은 48px 아틀라스를 훨씬 작게 줄여 그려서 마루 부리의 가는 획이 흐려진다.
// language.ini [Font]:
//   UiFontFile     = languages\ko 안의 TTF 파일 이름 (기본 MaruBuri-Regular.ttf, 없으면 시스템 맑은 고딕)
//   UiFontSize     = 아틀라스 글자 크기 px (기본 48, 12~96)
//   UiFontMultiply = ImFontConfig::RasterizerMultiply % (기본 100, 50~400). 크면 획이 진해진다
//   +0x4B603: C7 04 24 00 00 40 42             mov [esp], 48.0f            → 크기
//   +0x4B61B: C7 85 48 FF FF FF 01 00 00 00    mov [ebp-0xB8], 1 (OversampleV, 생성자 기본도 1)
//             → C7 85 74 FF FF FF <float>      mov [ebp-0x8C], m (RasterizerMultiply, cfg = ebp-0xD0, +0x44)
static void PatchUiCjkFontRaster(uintptr_t base)
{
    const int sizePx = KoIniInt("Font", "UiFontSize", 48, 12, 96);
    const int mulPct = KoIniInt("Font", "UiFontMultiply", 100, 50, 400);

    const uint8_t* ps = reinterpret_cast<const uint8_t*>(base + 0x4B603);
    static const uint8_t kSize[] = { 0xC7, 0x04, 0x24, 0x00, 0x00, 0x40, 0x42 };
    const uint8_t* pm = reinterpret_cast<const uint8_t*>(base + 0x4B61B);
    static const uint8_t kOvV[] = { 0xC7, 0x85, 0x48, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x00, 0x00 };
    if (memcmp(ps, kSize, sizeof(kSize)) != 0 || memcmp(pm, kOvV, sizeof(kOvV)) != 0) {
        PLOG_ERROR << "[ddraw] UI CJK font raster patch site mismatch";
        return;
    }
    if (sizePx != 48)
        patchDWORD(base + 0x4B606, FloatBits(static_cast<float>(sizePx)));
    if (mulPct != 100) {
        uint8_t code[10] = { 0xC7, 0x85, 0x74, 0xFF, 0xFF, 0xFF, 0, 0, 0, 0 };
        const uint32_t m = FloatBits(mulPct / 100.0f);
        memcpy(code + 6, &m, 4);
        patchBytes(base + 0x4B61B, code, sizeof(code));
    }
    PLOG_DEBUG << "[ddraw] UI CJK font size " << sizePx << "px, rasterizer multiply " << mulPct << "%";
}
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

// ---------------------------------------------------------------------------
// SNEG UI(ImGui) 크기별 글꼴을 마루 부리로 (16절 3단계 후속)
//
// config.ini [Text] *Font=retailfont 는 ddraw 에서 "type 3": UI 가 쓰는 크기마다
// FUN_597b7a30(atlas, "sneg_ui/fonts/PoRGothic-Regular.ttf", 크기, cfg, 범위) (= AddFontFromFileTTF) 로 넣고,
// FUN_59727010 으로 등록해 게임 retail(.ink) 비트맵 라틴 글자를 확대해 덮어쓴다.
// 한글은 FUN_5972b2d0 에서 48px 한 크기만 → 작게/크게 그릴 때 흐림/뭉개짐.
//   - AddFontFromFileTTF 훅: PoRGothic 이면 경로 → languages\ko\<UiFontFile>, 범위 → 라틴(0x20~0xFF) + CJK 글자 목록
//     → 크기마다 마루 부리 영문 + 한글.
//   - FUN_59727010 훅: UiLatin=maru 이면 등록하지 않음 (비트맵 라틴 덮어쓰기 끔).
// language.ini [Font] UiLatin = maru (기본) | retail (게임 비트맵 영문 유지, 한글만 크기별)
// ---------------------------------------------------------------------------
typedef void* (__fastcall* tAddFontTTF)(void* atlas, void* edx, const char* path, float size, void* cfg, const uint16_t* ranges);
typedef void(__cdecl* tRetailRegister)(void* p1, unsigned slot, float scale, void* font, int cp);
static tAddFontTTF     g_origAddFontTTF = nullptr;
static tRetailRegister g_origRetailRegister = nullptr;
static char            g_uiFontPath[MAX_PATH] = {};
static bool            g_uiLatinMaru = true;
static std::vector<uint16_t> g_uiRanges;
// 글꼴 역할별 크기 배율: language.ini [UiFontScale] "<역할>=%" (기본 100, 50~300).
//   역할 = ddraw UI 글꼴 kind (config.ini [Text] *Font 순서): 0 Overlay(툴팁 등), 1 Dialogue(DM 대사 상자), 2 Menu,
//   3 Inventory, 4 Action, 5 Prompt, 6 Radial, 7 Settings(설정창). 해상도와 무관한 키라서 배포용 설정으로 쓸 수 있다.
//   FUN_5978a0c0(글꼴정보, kind, slot, scale, size) 의 scale/size 에 곱한다 → 크기를 명시해 그리는 글에 적용된다.
//   게임 확인: Overlay(툴팁), Dialogue(DM 대사) 는 바뀜. Settings(설정창)·Menu 는 바뀌지 않음
//   (글꼴 조회 FUN_59720640 에 곱해 봐도 효과 없음, 사용자 판단으로 조사 중단).
static const char* const kUiFontRole[8] = { "Overlay", "Dialogue", "Menu", "Inventory", "Action", "Prompt", "Radial", "Settings" };
static int g_uiRoleScale[8] = { 100, 100, 100, 100, 100, 100, 100, 100 };
// Overlay(툴팁) 는 그리는 곳이 여러 개라 호출 위치(FUN_5978a0c0 의 반환 주소, ddraw 기준 오프셋)로 한 번 더 나눈다.
// 실제 배율 = 역할 키 × 하위 키. ddraw 가 바뀌면 오프셋이 달라질 수 있다 (그때는 하위 키만 효과가 없어짐).
// oldKey: 예전 이름 (새 키가 ini 에 없으면 그 값을 이어받고 예전 키는 지운다).
struct UiSubScale { uint32_t retOff; const char* key; const char* oldKey; int pct; };
static UiSubScale g_uiOverlaySub[] = {
    { 0xAD081, "OverlayActionPreview", "OverlayCursor",    100 },  // FUN_5978cf80: 반투명 행동 미리보기(설명) 툴팁
    { 0x6515A, "OverlayItem",          "OverlayActionBar", 100 },  // FUN_59745110 (툴팁 상자 도우미): 아이템 설명 툴팁
    { 0x77E8A, "OverlayStatus",        "OverlayHint",      100 },  // FUN_59757dd0: 왼쪽 아래 캐릭터 상태 툴팁
    { 0,       "OverlayRollLog",       nullptr,            100 },  // FUN_5975eb60: 왼쪽 위 주사위 굴림 등 (별도 훅)
    { 0,       "ActionIconTooltip",    "OverlayActionIcon", 100 }, // FUN_5974d580: 행동 아이콘 툴팁 (Action 역할, 별도 훅)
};
enum { kSubRollLog = 3, kSubActionIcon = 4 };
static uintptr_t g_ddrawBase = 0;

static uint32_t RetOff(void* ret)
{
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ret) - g_ddrawBase);
}

// FUN_5975eb60: 왼쪽 위 게임 글 덧그리기 (주사위 굴림 표시 등, 테두리 글).
// FUN_5978a0c0 를 거치지 않고 글자 크기를 직접 계산한다:
//   FUN_59720700(0, slot, h) (ddraw +0x40700, 부른 곳 반환 +0x7EE83) 또는 FUN_5971db50(h, w) (+0x3DB50, 부르는 곳은 여기뿐).
// 두 함수 결과에 Overlay × OverlayRollLog 배율을 곱한다. (둘 다 float 를 x87 ST0 로 돌려준다)
static float RollLogScale(float r)
{
    return r * (g_uiRoleScale[0] * g_uiOverlaySub[kSubRollLog].pct / 100) / 100.0f;
}
typedef float(__cdecl* tSlotFontSize)(int a, int slot, float h);
static tSlotFontSize g_origSlotFontSize = nullptr;
static float __cdecl SlotFontSize_Hook(int a, int slot, float h)
{
    const float r = g_origSlotFontSize(a, slot, h);
    return (g_uiFontPath[0] && RetOff(_ReturnAddress()) == 0x7EE83) ? RollLogScale(r) : r;
}
typedef float(__cdecl* tPopupFontSize)(float a, float b);
static tPopupFontSize g_origPopupFontSize = nullptr;
static float __cdecl PopupFontSize_Hook(float a, float b)
{
    const float r = g_origPopupFontSize(a, b);
    return g_uiFontPath[0] ? RollLogScale(r) : r;
}

// FUN_597204b0 (ddraw +0x404B0): float (kind, size, mode) — 역할별로 글자 px 를 retail 글꼴 배수에 맞춰 고른다.
// 행동 막대·팔레트·인벤토리 등 신UI 창이 FUN_5978a0c0 없이 이것으로 크기를 정한다 → 결과에 역할 키를 곱한다.
//   - kind 0/1/7 (Overlay/Dialogue/Settings) 은 FUN_5978a0c0 쪽에서 이미 곱하거나 건드리지 않으므로 제외.
//   - 행동 아이콘 툴팁 (FUN_5974d580, 반환 +0x6D6D0 제목 / +0x6D6F0 본문) 은 Action × ActionIconTooltip.
typedef float(__cdecl* tRoleFontSize)(uint32_t kind, float size, int mode);
static tRoleFontSize g_origRoleFontSize = nullptr;
static float __cdecl RoleFontSize_Hook(uint32_t kind, float size, int mode)
{
    const float r = g_origRoleFontSize(kind, size, mode);
    if (!g_uiFontPath[0] || kind < 2 || kind > 6)
        return r;
    int pct = g_uiRoleScale[kind];
    const uint32_t off = RetOff(_ReturnAddress());
    if (off == 0x6D6D0 || off == 0x6D6F0)
        pct = pct * g_uiOverlaySub[kSubActionIcon].pct / 100;
    return pct == 100 ? r : r * pct / 100.0f;
}

// 행동 아이콘 툴팁 배경 불투명도 ([UiStyle] ActionIconTooltipOpacity, 0~100%, 기본 100).
// FUN_5974d580 의 배경은 둘 중 하나:
//   - 패널 그림 (DAT_59a14ec8 있음): FUN_597b7f20 (ImDrawList::AddImage, thiscall, col=흰색) 3번 → 훅에서 col 알파를 줄인다.
//   - 단색: AddRectFilled col 0xF51E1C19 (push 즉치값 +0x6E1A1) → 알파 바이트를 패치.
static int g_actionTipOpacity = 100;
typedef void(__thiscall* tAddImage)(void* dl, void* tex, float* pmin, float* pmax, float* uvmin, float* uvmax, uint32_t col);
static tAddImage g_origAddImage = nullptr;
// ddraw 표면 메서드 원본 (Blt +0x4E60, BltFast +0x5480). 아래 '저장된 화면 위에 고해상도 글 다시 그리기' 참고
typedef HRESULT(__stdcall* tSurfBltFast)(uint8_t* dst, int x, int y, uint8_t* src, RECT* rc, uint32_t flags);
typedef HRESULT(__stdcall* tSurfBlt)(uint8_t* dst, RECT* rd, uint8_t* src, RECT* rs, uint32_t flags, void* fx);
static tSurfBltFast g_origSurfBltFast = nullptr;
static tSurfBlt g_origSurfBlt = nullptr;
static uint8_t* PrimarySurf()
{
    return g_ddrawBase ? *reinterpret_cast<uint8_t**>(g_ddrawBase + 0x304310) : nullptr;
}

// ── 저장된 화면 위에 고해상도 글 다시 그리기 ──
// 게임은 고정 글(캐릭터 목록 제목, 가운데 안내 글 등)을 한 번만 그리고 곧바로 후면 버퍼를 통째로 저장 표면에 복사한 뒤,
// 다음 프레임부터는 그 저장 표면만 깐다. 고해상도 글은 ddraw 의 창 크기 UI 레이어에만 있어서, 복사할 때 ddraw 가
// UI 레이어를 저해상도 후면 버퍼로 점 샘플링해 섞는다(FUN_597e2650 → FUN_597ee950) → 줄이 빠진 저해상도 사본만 남는다.
// 그래서: 후면 버퍼에 그린 32비트(sneg 고해상도 글) Blt 를 기록해 두고, 후면 버퍼를 저장할 때 그 목록을 저장 표면에 묶어 두었다가,
// 저장 표면을 후면 버퍼에 통째로 다시 깔 때 그 위에 같은 Blt 를 다시 한다.
struct HiResBltRec { uint8_t* src; RECT rd; RECT rs; uint32_t flags; };
struct SavedScreen { uint8_t* surf; std::vector<HiResBltRec> recs; };
static std::vector<HiResBltRec> g_frameHiRes;     // 마지막으로 화면을 통째로 깐 뒤 그린 고해상도 글
static std::vector<SavedScreen> g_savedScreens;   // 최근 저장 표면 (최대 8개)
static bool SurfAddRef(uint8_t* s)
{
    __try { (*reinterpret_cast<ULONG(__stdcall***)(void*)>(s))[1](s); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static void SurfRelease(uint8_t* s)
{
    __try { (*reinterpret_cast<ULONG(__stdcall***)(void*)>(s))[2](s); }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}
static void ReleaseRecs(std::vector<HiResBltRec>& v)
{
    for (HiResBltRec& r : v)
        SurfRelease(r.src);
    v.clear();
}
static bool SurfInfo(const uint8_t* s, int& w, int& h, int& bpp)
{
    __try {
        w = *reinterpret_cast<const int*>(s + 0x14);
        h = *reinterpret_cast<const int*>(s + 0x10);
        bpp = *reinterpret_cast<const int*>(s + 0x98);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
static uint8_t* BackBufferSurf()
{
    return g_ddrawBase ? *reinterpret_cast<uint8_t**>(g_ddrawBase + 0x304314) : nullptr;
}
// 후면 버퍼에 화면 크기(높이 600 이상) 그림을 까는 것은 새 화면 시작 → 기록을 비운다
static void ResetOnFullScreenDraw(uint8_t* src)
{
    int w, h, bpp;
    if (src && SurfInfo(src, w, h, bpp) && h >= 600)
        ReleaseRecs(g_frameHiRes);
}
static void ClearFrameHiRes()
{
    ReleaseRecs(g_frameHiRes);
}
// 저장 표면에 게임이 다른 것을 그리면(후면 버퍼 복사가 아닌 Blt/BltFast, Lock, GetDC) 묶어 둔 글은 더는 맞지 않으니 버린다
static void DropSavedScreen(const uint8_t* s)
{
    for (auto it = g_savedScreens.begin(); it != g_savedScreens.end(); ++it)
        if (it->surf == s) {
            static int s_log = 0;
            if (s_log++ < 10)
                PLOG_DEBUG << "[ddraw] saved screen " << static_cast<const void*>(s) << " overwritten -> drop " << it->recs.size() << " hi-res text blits";
            ReleaseRecs(it->recs);
            SurfRelease(it->surf);
            g_savedScreens.erase(it);
            return;
        }
}
static bool IsSavedScreen(const uint8_t* s)
{
    for (const SavedScreen& v : g_savedScreens)
        if (v.surf == s)
            return true;
    return false;
}
// 고해상도 글 Blt 는 바로 그리지 않고 미뤄 둔다 (g_pendingHiRes). 화면 저장 복사가 바로 뒤따르면 복사 다음에 그려서
// 저장 화면에 저해상도 사본이 남지 않게 한다. 그 밖에 후면 버퍼를 건드리는 일(다른 그리기, 화면 표시, Lock, GetDC)이
// 오면 그 전에 그려서 겹치는 순서는 원래대로 둔다.
static std::vector<HiResBltRec> g_pendingHiRes;
static void FlushPendingHiRes()
{
    if (g_pendingHiRes.empty())
        return;
    std::vector<HiResBltRec> v;
    v.swap(g_pendingHiRes);
    uint8_t* bb = BackBufferSurf();
    for (HiResBltRec& r : v) {
        RECT rd = r.rd, rs = r.rs;
        if (bb)
            g_origSurfBlt(bb, &rd, r.src, &rs, r.flags, nullptr);
        SurfRelease(r.src);
    }
}
// 후면 버퍼에 32비트 원본을 알파로 늘여 그리는 Blt 면 기록하고 미룬다 (true = 미뤘으니 원래 Blt 는 하지 말 것)
static bool DeferHiResBlt(uint8_t* dst, RECT* rd, uint8_t* src, RECT* rs, uint32_t flags, void* fx)
{
    if (fx || !dst || !src || !rd || !rs || dst != BackBufferSurf() || (flags & ~0x1000000u) != 0x20 ||
        g_frameHiRes.size() >= 256 || g_pendingHiRes.size() >= 256)
        return false;
    int w, h, bpp;
    if (!SurfInfo(src, w, h, bpp) || bpp != 4)
        return false;
    if (!SurfAddRef(src))
        return false;
    if (!SurfAddRef(src)) {
        SurfRelease(src);
        return false;
    }
    g_frameHiRes.push_back({ src, *rd, *rs, flags });
    g_pendingHiRes.push_back({ src, *rd, *rs, flags });
    return true;
}
// 후면 버퍼 → 다른 표면 복사: 지금까지의 고해상도 글 목록을 그 표면에 묶는다
static void OnBackBufferSaved(uint8_t* dst)
{
    for (auto it = g_savedScreens.begin(); it != g_savedScreens.end(); ++it)
        if (it->surf == dst) {
            ReleaseRecs(it->recs);
            SurfRelease(it->surf);
            g_savedScreens.erase(it);
            break;
        }
    if (g_frameHiRes.empty() || !SurfAddRef(dst))
        return;
    SavedScreen s{ dst, {} };
    for (const HiResBltRec& r : g_frameHiRes)
        if (SurfAddRef(r.src))
            s.recs.push_back(r);
    if (g_savedScreens.size() >= 8) {
        ReleaseRecs(g_savedScreens.front().recs);
        SurfRelease(g_savedScreens.front().surf);
        g_savedScreens.erase(g_savedScreens.begin());
    }
    static int s_log = 0;
    if (s_log++ < 10)
        PLOG_DEBUG << "[ddraw] saved screen " << static_cast<void*>(dst) << " with " << s.recs.size() << " hi-res text blits";
    g_savedScreens.push_back(std::move(s));
}
// 저장 표면을 후면 버퍼에 통째로 깐 뒤: 새 프레임 시작으로 보고 목록을 비운 다음, 묶어 둔 고해상도 글을 다시 그린다
static void OnScreenRestored(uint8_t* src, const RECT* rs)
{
    int w, h, bpp;
    if (!SurfInfo(src, w, h, bpp))
        return;
    if (rs && (rs->left != 0 || rs->top != 0 || rs->right != w || rs->bottom != h))
        return;   // 일부만 다시 까는 경우는 건드리지 않는다
    ReleaseRecs(g_frameHiRes);
    for (const SavedScreen& s : g_savedScreens) {
        if (s.surf != src)
            continue;
        uint8_t* bb = BackBufferSurf();
        for (const HiResBltRec& r : s.recs) {
            RECT rd = r.rd, rsrc = r.rs;
            g_origSurfBlt(bb, &rd, r.src, &rsrc, r.flags, nullptr);
            if (g_frameHiRes.size() < 256 && SurfAddRef(r.src))
                g_frameHiRes.push_back(r);    // 이 화면을 또 저장하면 함께 넘어가도록
        }
        break;
    }
}

static HRESULT __stdcall SurfBlt_Hook(uint8_t* dst, RECT* rd, uint8_t* src, RECT* rs, uint32_t flags, void* fx)
{
    uint8_t* bb = BackBufferSurf();
    uint8_t* prim = PrimarySurf();
    if (src && src == bb && dst && dst != bb) {
        if (dst == prim) {                 // 화면 표시: 미룬 글을 먼저, 그리고 프레임 기록 비우기
            FlushPendingHiRes();
            ReleaseRecs(g_frameHiRes);
            return g_origSurfBlt(dst, rd, src, rs, flags, fx);
        }
        const HRESULT hr = g_origSurfBlt(dst, rd, src, rs, flags, fx);   // 화면 저장: 미룬 글 없이 복사
        OnBackBufferSaved(dst);
        FlushPendingHiRes();               // 그다음 이번 프레임용으로 그린다
        return hr;
    }
    if (dst && dst == bb) {
        if (src && (flags & 0x400) == 0 && DeferHiResBlt(dst, rd, src, rs, flags, fx))
            return DD_OK;
        FlushPendingHiRes();
        if (src && (flags & 0x400) == 0) {
            if (IsSavedScreen(src)) {
                const HRESULT hr = g_origSurfBlt(dst, rd, src, rs, flags, fx);
                OnScreenRestored(src, rs);
                return hr;
            }
            ResetOnFullScreenDraw(src);
        }
    } else if (dst && dst == prim) {
        FlushPendingHiRes();
    } else if (dst && IsSavedScreen(dst)) {
        DropSavedScreen(dst);
    }
    return g_origSurfBlt(dst, rd, src, rs, flags, fx);
}

static HRESULT __stdcall SurfBltFast_Hook(uint8_t* dst, int x, int y, uint8_t* src, RECT* rc, uint32_t flags)
{
    uint8_t* bb = BackBufferSurf();
    uint8_t* prim = PrimarySurf();
    if (src && src == bb && dst && dst != bb) {
        if (dst == prim) {
            FlushPendingHiRes();
            ClearFrameHiRes();
            return g_origSurfBltFast(dst, x, y, src, rc, flags);
        }
        const HRESULT hr = g_origSurfBltFast(dst, x, y, src, rc, flags);
        OnBackBufferSaved(dst);
        FlushPendingHiRes();
        return hr;
    }
    if (dst && (dst == bb || dst == prim))
        FlushPendingHiRes();
    else if (dst && IsSavedScreen(dst))
        DropSavedScreen(dst);
    if (dst && dst == bb && src && IsSavedScreen(src)) {
        const HRESULT hr = g_origSurfBltFast(dst, x, y, src, rc, flags);
        OnScreenRestored(src, rc);
        return hr;
    }
    if (dst && dst == bb)
        ResetOnFullScreenDraw(src);
    return g_origSurfBltFast(dst, x, y, src, rc, flags);
}

// 미룬 고해상도 글을 그려야 하는 다른 때: 화면 표시(Flip), 후면 버퍼 Lock / GetDC
typedef HRESULT(__stdcall* tSurfFlip)(uint8_t* s, void* target, uint32_t flags);
typedef HRESULT(__stdcall* tSurfLock)(uint8_t* s, RECT* rc, void* desc, uint32_t flags, HANDLE ev);
typedef HRESULT(__stdcall* tSurfGetDC)(uint8_t* s, HDC* dc);
static tSurfFlip g_origSurfFlip = nullptr;
static tSurfLock g_origSurfLock = nullptr;
static tSurfGetDC g_origSurfGetDC = nullptr;
static HRESULT __stdcall SurfFlip_Hook(uint8_t* s, void* target, uint32_t flags)
{
    FlushPendingHiRes();
    ReleaseRecs(g_frameHiRes);    // 한 프레임이 끝났다: 다음 화면 저장에는 이번 프레임 이후에 그린 글만 묶는다
    return g_origSurfFlip(s, target, flags);
}
static HRESULT __stdcall SurfLock_Hook(uint8_t* s, RECT* rc, void* desc, uint32_t flags, HANDLE ev)
{
    if (s && (s == BackBufferSurf() || s == PrimarySurf()))
        FlushPendingHiRes();
    else if (s && IsSavedScreen(s) && (flags & 0x10) == 0)   // DDLOCK_READONLY 가 아니면
        DropSavedScreen(s);
    return g_origSurfLock(s, rc, desc, flags, ev);
}
static HRESULT __stdcall SurfGetDC_Hook(uint8_t* s, HDC* dc)
{
    if (s && (s == BackBufferSurf() || s == PrimarySurf()))
        FlushPendingHiRes();
    else if (s && IsSavedScreen(s))
        DropSavedScreen(s);
    return g_origSurfGetDC(s, dc);
}

static void __fastcall AddImage_Hook(void* dl, void* /*edx*/, void* tex, float* pmin, float* pmax, float* uvmin, float* uvmax, uint32_t col)
{
    const uint32_t off = RetOff(_ReturnAddress());
    if (off >= 0x6D580 && off < 0x6E830)      // FUN_5974d580 (행동 아이콘 툴팁) 안에서 부른 것만
        col = (col & 0x00FFFFFF) | (((col >> 24) * g_actionTipOpacity / 100) << 24);
    g_origAddImage(dl, tex, pmin, pmax, uvmin, uvmax, col);
}

// 테두리 있는 판 (param_7 경로): FUN_5974c8c0(dl, rect, scale) = 검은 바탕 AddRectFilled(0xFF000000, push 즉치값 +0x6C938)
// + 테두리 조각 8개. 행동 막대 등 다른 2곳도 같이 쓰므로, 툴팁에서 부를 때(반환 +0x6DFBF)만 바탕 알파 바이트를 잠깐 바꾼다.
static uint8_t* g_framedFillAlpha = nullptr;   // push 0xFF000000 의 마지막 바이트 (쓰기 가능하게 열어 둠)
typedef void(__cdecl* tFramedPanel)(void* dl, float* rect, float scale);
static tFramedPanel g_origFramedPanel = nullptr;
static void __cdecl FramedPanel_Hook(void* dl, float* rect, float scale)
{
    if (g_framedFillAlpha && RetOff(_ReturnAddress()) == 0x6DFBF) {
        *g_framedFillAlpha = static_cast<uint8_t>(0xFF * g_actionTipOpacity / 100);
        g_origFramedPanel(dl, rect, scale);
        *g_framedFillAlpha = 0xFF;
        return;
    }
    g_origFramedPanel(dl, rect, scale);
}

static void LoadUiScaleMap()
{
    char ini[MAX_PATH];
    if (!GetKoLanguageIni(ini, MAX_PATH))
        return;
    auto readKey = [&](const char* key, int& out) {
        const int v = static_cast<int>(GetPrivateProfileIntA("UiFontScale", key, -1, ini));
        if (v < 0)
            WritePrivateProfileStringA("UiFontScale", key, "100", ini);    // 없는 키는 100 으로 채워 둔다
        else if (v >= 50 && v <= 300)
            out = v;
    };
    for (int k = 0; k < 8; ++k)
        readKey(kUiFontRole[k], g_uiRoleScale[k]);
    for (UiSubScale& s : g_uiOverlaySub) {
        if (s.oldKey && GetPrivateProfileIntA("UiFontScale", s.key, -1, ini) < 0) {
            const int old = static_cast<int>(GetPrivateProfileIntA("UiFontScale", s.oldKey, -1, ini));
            if (old >= 50 && old <= 300) {
                char buf[16];
                wsprintfA(buf, "%d", old);
                WritePrivateProfileStringA("UiFontScale", s.key, buf, ini);
            }
            WritePrivateProfileStringA("UiFontScale", s.oldKey, nullptr, ini);   // 예전 키 지우기
        }
        readKey(s.key, s.pct);
    }
    {
        const int v = static_cast<int>(GetPrivateProfileIntA("UiStyle", "ActionIconTooltipOpacity", -1, ini));
        if (v < 0)
            WritePrivateProfileStringA("UiStyle", "ActionIconTooltipOpacity", "100", ini);
        else
            g_actionTipOpacity = v > 100 ? 100 : v;
    }
    PLOG_DEBUG << "[ddraw] UI font scale Overlay " << g_uiRoleScale[0] << " (preview " << g_uiOverlaySub[0].pct
               << ", item " << g_uiOverlaySub[1].pct << ", status " << g_uiOverlaySub[2].pct
               << ", roll log " << g_uiOverlaySub[kSubRollLog].pct << ") Dialogue " << g_uiRoleScale[1]
               << " Action " << g_uiRoleScale[4] << " (icon tooltip " << g_uiOverlaySub[kSubActionIcon].pct << ")";
}

static int GetUiRoleScale(uint32_t kind, uintptr_t ret = 0)
{
    if (kind >= 8)
        return 100;
    int pct = g_uiRoleScale[kind];
    if (kind == 0 && g_ddrawBase && ret > g_ddrawBase) {
        const uint32_t off = static_cast<uint32_t>(ret - g_ddrawBase);
        for (const UiSubScale& s : g_uiOverlaySub)
            if (s.retOff && s.retOff == off)
                return pct * s.pct / 100;
    }
    // [진단] 표에 없는 호출 위치 (역할, 오프셋) 를 한 번씩 기록 — 새 하위 키를 찾을 때 쓴다
    if (g_ddrawBase && ret > g_ddrawBase) {
        static std::vector<uint64_t> s_seen;
        const uint64_t id = (static_cast<uint64_t>(kind) << 32) | static_cast<uint32_t>(ret - g_ddrawBase);
        if (s_seen.size() < 64 && std::find(s_seen.begin(), s_seen.end(), id) == s_seen.end()) {
            s_seen.push_back(id);
            PLOG_DEBUG << "[ddraw] UI font caller not mapped: " << kUiFontRole[kind] << " +0x" << std::hex
                       << static_cast<uint32_t>(ret - g_ddrawBase) << std::dec;
        }
    }
    return pct;
}

// FUN_5978a750 (ddraw +0xAA750): ImFont* (글꼴정보* info, float* outSize) — 그리기·측정이 글꼴과 그릴 px 를 얻는 곳.
//   보통은 outSize = info[4], retail 슬롯은 retail 높이 * info[2](배율) / 0.7. 훅하지 않고 주소로 직접 부른다.
typedef void* (__cdecl* tUiFontForDraw)(uint32_t* info, float* outSize);
static tUiFontForDraw g_origUiFontForDraw = nullptr;

// FUN_5978a7f0 (ddraw +0xAA7F0): 글자가 모두 retail(게임 비트맵) 글꼴에 있으면(= ASCII 만) 비트맵 글리프로
// 줄을 나누고 그리는 경로 (그리기 FUN_59789850, 측정 FUN_59789c70 이 먼저 묻는다). 거짓이면 TTF 경로.
// UiLatin=maru 이면 항상 거짓 → "DM:", "1706 x 720" 같은 영문만 있는 글도 마루 부리로.
typedef char(__cdecl* tRetailTextPath)(uint32_t* info, const char* text, float wrap, void* outLines, void* outWidths);
static tRetailTextPath g_origRetailTextPath = nullptr;

static char __cdecl RetailTextPath_Hook(uint32_t* info, const char* text, float wrap, void* outLines, void* outWidths)
{
    if (g_uiLatinMaru && g_uiFontPath[0])
        return 0;
    return g_origRetailTextPath(info, text, wrap, outLines, outWidths);
}

// FUN_5978a0c0 (ddraw +0xAA0C0): 글꼴정보 {kind, slot, scale, font, 요청 px, retail 그릴 px} 를 만든다.
//  1) [UiFontScale] 배율을 scale/size 에 곱한다.
//  2) 인라인 아이콘 글(FUN_5973b530/6d0/750)은 여기의 font/요청 px([3],[4])를 그대로 쓰는데,
//     보통 글(FUN_5978a750)은 retail 슬롯이면 retail 높이 * 배율 / 0.7 로 다시 계산해서 크기가 달랐다
//     (튜토리얼 아이콘 대사가 일반 대사보다 작음). → [3],[4] 를 FUN_5978a750 이 줄 값으로 맞춘다.
typedef uint32_t* (__cdecl* tUiFontInfo)(uint32_t* out, uint32_t kind, uint32_t slot, float scale, float size);
static tUiFontInfo g_origUiFontInfo = nullptr;

static uint32_t* __cdecl UiFontInfo_Hook(uint32_t* out, uint32_t kind, uint32_t slot, float scale, float size)
{
    if (g_uiFontPath[0]) {
        const int pct = GetUiRoleScale(kind, reinterpret_cast<uintptr_t>(_ReturnAddress()));
        if (pct != 100) {
            scale = scale * pct / 100.0f;
            size = size * pct / 100.0f;
        }
    }
    uint32_t* r = g_origUiFontInfo(out, kind, slot, scale, size);
    if (r && g_uiFontPath[0] && g_origUiFontForDraw) {
        float s = 0.0f;
        void* font = g_origUiFontForDraw(r, &s);
        if (font && s > 0.0f) {
            r[3] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(font));
            memcpy(&r[4], &s, 4);
        }
    }
    return r;
}

static bool EndsWithI(const char* s, const char* suffix)
{
    const size_t a = strlen(s), b = strlen(suffix);
    return a >= b && _stricmp(s + a - b, suffix) == 0;
}

// 라틴 + languages\ko\sneg_ui_cjk_characters.txt(UTF-8) 의 글자 → ImGui 범위 (정렬된 [시작,끝] 쌍, 0 종료)
static bool BuildUiRanges()
{
    if (!g_uiRanges.empty())
        return true;
    char path[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, path, MAX_PATH);
    char* slash = (n > 0 && n < MAX_PATH) ? strrchr(path, '\\') : nullptr;
    if (!slash || (slash - path) + 48 >= MAX_PATH)
        return false;
    lstrcpyA(slash + 1, "languages\\ko\\sneg_ui_cjk_characters.txt");
    std::vector<uint8_t> buf;
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f != INVALID_HANDLE_VALUE) {
        const DWORD size = GetFileSize(f, NULL);
        if (size != INVALID_FILE_SIZE && size < 4 * 1024 * 1024) {
            buf.resize(size);
            DWORD rd = 0;
            if (!ReadFile(f, buf.data(), size, &rd, NULL)) rd = 0;
            buf.resize(rd);
        }
        CloseHandle(f);
    }
    std::vector<uint16_t> cps;
    for (uint32_t c = 0x20; c <= 0xFF; ++c)
        if (c < 0x7F || c >= 0xA0) cps.push_back(static_cast<uint16_t>(c));
    // 새 UI 문구에 쓰이는 기호: 라틴 확장 A, 일반 구두점(– — ‘ ’ “ ” … 등), 통화/상표, 화살표
    // (예: "1–6 캐릭터 전환" 의 en dash 가 없어서 '?' 로 나왔다)
    static const uint16_t kExtra[][2] = { {0x0100, 0x017F}, {0x2010, 0x205E}, {0x20AC, 0x20AC},
                                          {0x2122, 0x2122}, {0x2190, 0x21FF}, {0x2212, 0x2212} };
    for (const auto& r : kExtra)
        for (uint32_t c = r[0]; c <= r[1]; ++c) cps.push_back(static_cast<uint16_t>(c));
    for (size_t i = 0; i < buf.size(); ) {         // UTF-8 디코드 (BMP 만)
        const uint8_t b = buf[i];
        uint32_t cp = 0; size_t len = 1;
        if (b < 0x80) cp = b;
        else if ((b & 0xE0) == 0xC0 && i + 1 < buf.size()) { cp = ((b & 0x1F) << 6) | (buf[i + 1] & 0x3F); len = 2; }
        else if ((b & 0xF0) == 0xE0 && i + 2 < buf.size()) { cp = ((b & 0x0F) << 12) | ((buf[i + 1] & 0x3F) << 6) | (buf[i + 2] & 0x3F); len = 3; }
        else if ((b & 0xF8) == 0xF0) len = 4;
        i += len;
        if (cp >= 0x20 && cp < 0xFFFF && cp != 0xFEFF) cps.push_back(static_cast<uint16_t>(cp));
    }
    std::sort(cps.begin(), cps.end());
    cps.erase(std::unique(cps.begin(), cps.end()), cps.end());
    for (size_t i = 0; i < cps.size(); ) {
        size_t j = i;
        while (j + 1 < cps.size() && cps[j + 1] == cps[j] + 1) ++j;
        g_uiRanges.push_back(cps[i]);
        g_uiRanges.push_back(cps[j]);
        i = j + 1;
    }
    g_uiRanges.push_back(0);
    PLOG_DEBUG << "[ddraw] UI font ranges: " << cps.size() << " chars, " << (g_uiRanges.size() / 2) << " ranges"
               << (buf.empty() ? " (CJK list not found)" : "");
    return !buf.empty();
}

// 같은 아틀라스에서 같은 px(반올림)로 이미 만든 UI 글꼴은 다시 만들지 않고 돌려준다 (시작 시간·아틀라스 크기 절약).
// 아틀라스를 새로 만들 때마다 처음 부르는 AddFontDefault(FUN_597b7810) 에서 비운다.
typedef void* (__fastcall* tAddFontDefault)(void* atlas, void* edx, const void* cfg);
static tAddFontDefault g_origAddFontDefault = nullptr;
static std::map<std::pair<void*, int>, void*> g_uiFontCache;

static void* __fastcall AddFontDefault_Hook(void* atlas, void* edx, const void* cfg)
{
    g_uiFontCache.clear();
    return g_origAddFontDefault(atlas, edx, cfg);
}

static void* __fastcall AddFontTTF_Hook(void* atlas, void* edx, const char* path, float size, void* cfg, const uint16_t* ranges)
{
    const char* usePath = path;
    const uint16_t* useRanges = ranges;
    float useSize = size;
    const bool swap = path && g_uiFontPath[0] && EndsWithI(path, "PoRGothic-Regular.ttf") && BuildUiRanges();
    std::pair<void*, int> cacheKey(atlas, 0);
    if (swap) {
        usePath = g_uiFontPath;
        useRanges = g_uiRanges.data();
        cacheKey.second = static_cast<int>(useSize + 0.5f);
        auto it = g_uiFontCache.find(cacheKey);
        if (it != g_uiFontCache.end())
            return it->second;
    }
    void* font = g_origAddFontTTF(atlas, edx, usePath, useSize, cfg, useRanges);
    if (swap && font)
        g_uiFontCache[cacheKey] = font;
    static int s_logged = 0;
    if (s_logged++ < 64)
        PLOG_DEBUG << "[ddraw] AddFontFromFileTTF " << (path ? path : "(null)") << " " << size << "px"
                   << (swap ? " -> UI font " : "") << (swap ? std::to_string(useSize) : std::string()) << " = " << font;
    return font;
}

static void __cdecl RetailRegister_Hook(void* p1, unsigned slot, float scale, void* font, int cp)
{
    if (g_uiLatinMaru)
        return;                 // 비트맵 라틴 덮어쓰기 끔 → 마루 부리 영문
    g_origRetailRegister(p1, slot, scale, font, cp);
}

// FUN_59720180 (ddraw +0x40180): 글꼴(ImFont*) 마다 커닝 쌍 표를 만든다 — 글리프 N 개면 N*N 번 커닝 값을 묻는다.
// 한글 2,600자가 들어간 크기별 글꼴 16개에서 이 단계가 6초 걸렸다 (FUN_59721df0 → FUN_59720180).
// 한글은 커닝이 없고 마루 부리 커닝도 라틴끼리라서, 글리프 목록(ImFont: [8] 개수, [10] 40바이트 ImFontGlyph 배열,
// 코드포인트 오름차순)을 U+2E80 앞까지로 잠시 줄여서 부른다. 정렬이 아니면(뒤에 U+2E80 미만이 있으면) 원래대로.
typedef void(__cdecl* tBuildKerning)(uint32_t* font, int filterFn, int kernFn);
static tBuildKerning g_origBuildKerning = nullptr;

static void __cdecl BuildKerning_Hook(uint32_t* font, int filterFn, int kernFn)
{
    // 이 ddraw 의 글리프 배열은 코드포인트 순서가 아니므로, U+2E80 미만 글리프만 임시 배열에 모아
    // 글리프 포인터/개수를 잠시 바꿔 부른다 (커닝 함수는 글리프의 코드포인트만 읽는다).
    if (font && font[10] && static_cast<int>(font[8]) > 0) {
        const uint32_t* glyphs = reinterpret_cast<const uint32_t*>(font[10]);
        const int count = static_cast<int>(font[8]);
        std::vector<uint32_t> latin;
        latin.reserve(static_cast<size_t>(count) * 10);
        for (int i = 0; i < count; ++i)
            if ((glyphs[i * 10] & 0x7FFFFFFF) < 0x2E80)
                latin.insert(latin.end(), glyphs + i * 10, glyphs + i * 10 + 10);
        const int kept = static_cast<int>(latin.size() / 10);
        static int s_logged = 0;
        if (s_logged++ < 6)
            PLOG_DEBUG << "[ddraw] kerning glyphs " << count << " -> " << kept << " (below U+2E80)";
        if (kept > 0 && kept < count) {
            const uint32_t savedData = font[10];
            font[10] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(latin.data()));
            font[8] = static_cast<uint32_t>(kept);
            g_origBuildKerning(font, filterFn, kernFn);
            font[8] = static_cast<uint32_t>(count);
            font[10] = savedData;
            return;
        }
    }
    g_origBuildKerning(font, filterFn, kernFn);
}

static void InstallUiFontHooks(uintptr_t base)
{
    char ini[MAX_PATH], latin[16] = {};
    if (GetKoLanguageIni(ini, MAX_PATH))
        GetPrivateProfileStringA("Font", "UiLatin", "maru", latin, sizeof(latin), ini);
    g_uiLatinMaru = _stricmp(latin, "retail") != 0;

    if (g_cjkFontDir[0]) {
        wsprintfA(g_uiFontPath, "%s\\%s", g_cjkFontDir, g_szCjkFont);
        if (GetFileAttributesA(g_uiFontPath) == INVALID_FILE_ATTRIBUTES) {
            PLOG_ERROR << "[ddraw] UI font not found: " << g_uiFontPath;
            g_uiFontPath[0] = '\0';
        }
    }

    LoadUiScaleMap();

    static const uint8_t kAddFontPro[] = { 0x55, 0x8B, 0xEC, 0x81, 0xEC, 0x08, 0x01, 0x00, 0x00 };
    static const uint8_t kRetailPro[]  = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x08, 0x57, 0x8B, 0x7D, 0x08 };
    void* addFont = reinterpret_cast<void*>(base + 0xD7A30);
    void* retail  = reinterpret_cast<void*>(base + 0x47010);
    MH_STATUS st = MH_Initialize();
    if (st != MH_OK && st != MH_ERROR_ALREADY_INITIALIZED) {
        PLOG_ERROR << "[ddraw] UI font hooks: MH_Initialize " << st;
        return;
    }
    if (memcmp(addFont, kAddFontPro, sizeof(kAddFontPro)) == 0) {
        st = MH_CreateHook(addFont, reinterpret_cast<void*>(&AddFontTTF_Hook), reinterpret_cast<void**>(&g_origAddFontTTF));
        if (st == MH_OK) st = MH_EnableHook(addFont);
        PLOG_DEBUG << "[ddraw] AddFontFromFileTTF hook status=" << st << ", UI font " << (g_uiFontPath[0] ? g_uiFontPath : "(none)");
    } else {
        PLOG_ERROR << "[ddraw] AddFontFromFileTTF prologue mismatch";
    }
    if (memcmp(retail, kRetailPro, sizeof(kRetailPro)) == 0) {
        st = MH_CreateHook(retail, reinterpret_cast<void*>(&RetailRegister_Hook), reinterpret_cast<void**>(&g_origRetailRegister));
        if (st == MH_OK) st = MH_EnableHook(retail);
        PLOG_DEBUG << "[ddraw] retail Latin overlay hook status=" << st << ", UiLatin=" << (g_uiLatinMaru ? "maru" : "retail");
    } else {
        PLOG_ERROR << "[ddraw] retail Latin overlay prologue mismatch";
    }
    static const uint8_t kKernPro[] = { 0x55, 0x8B, 0xEC, 0x6A, 0xFF, 0x68, 0xCD, 0xEC };
    void* kern = reinterpret_cast<void*>(base + 0x40180);
    if (memcmp(kern, kKernPro, sizeof(kKernPro)) == 0) {
        st = MH_CreateHook(kern, reinterpret_cast<void*>(&BuildKerning_Hook), reinterpret_cast<void**>(&g_origBuildKerning));
        if (st == MH_OK) st = MH_EnableHook(kern);
        PLOG_DEBUG << "[ddraw] kerning table (Latin only) hook status=" << st;
    } else {
        PLOG_ERROR << "[ddraw] kerning table prologue mismatch";
    }

    static const uint8_t kDefaultPro[] = { 0x55, 0x8B, 0xEC, 0x81, 0xEC, 0x04, 0x01, 0x00, 0x00 };
    void* addDefault = reinterpret_cast<void*>(base + 0xD7810);
    if (memcmp(addDefault, kDefaultPro, sizeof(kDefaultPro)) == 0) {
        st = MH_CreateHook(addDefault, reinterpret_cast<void*>(&AddFontDefault_Hook), reinterpret_cast<void**>(&g_origAddFontDefault));
        if (st == MH_OK) st = MH_EnableHook(addDefault);
        PLOG_DEBUG << "[ddraw] AddFontDefault (UI font cache reset) hook status=" << st;
    } else {
        PLOG_ERROR << "[ddraw] AddFontDefault prologue mismatch";
    }

    // 영문만 있는 글의 retail 비트맵 경로 끄기, 아이콘 글 크기 맞추기
    {
        static const uint8_t kRetailTextPro[] = { 0x55, 0x8B, 0xEC, 0x6A, 0xFF, 0x68, 0x4D, 0x21, 0x82, 0x59 };
        void* t = reinterpret_cast<void*>(base + 0xAA7F0);
        if (memcmp(t, kRetailTextPro, 6) == 0) {    // 6 바이트만 비교 (뒤 4바이트는 재배치되는 주소)
            st = MH_CreateHook(t, reinterpret_cast<void*>(&RetailTextPath_Hook), reinterpret_cast<void**>(&g_origRetailTextPath));
            if (st == MH_OK) st = MH_EnableHook(t);
            PLOG_DEBUG << "[ddraw] retail text path (ASCII bitmap) hook status=" << st;
        } else {
            PLOG_ERROR << "[ddraw] retail text path prologue mismatch";
        }
        static const uint8_t kInfoPro[] = { 0x55, 0x8B, 0xEC, 0xF3, 0x0F, 0x10, 0x45, 0x14 };
        void* info = reinterpret_cast<void*>(base + 0xAA0C0);
        if (memcmp(info, kInfoPro, sizeof(kInfoPro)) == 0) {
            st = MH_CreateHook(info, reinterpret_cast<void*>(&UiFontInfo_Hook), reinterpret_cast<void**>(&g_origUiFontInfo));
            if (st == MH_OK) st = MH_EnableHook(info);
            PLOG_DEBUG << "[ddraw] UI font info (case scale, icon text size) hook status=" << st;
        } else {
            PLOG_ERROR << "[ddraw] UI font info prologue mismatch";
        }
    }

    g_ddrawBase = base;
    {   // 저장된 화면 위에 고해상도 글 다시 그리기: 표면 메서드 5개 (하나라도 프롤로그가 다르면 전부 건너뜀)
        struct { uint32_t off; uint8_t pro[6]; size_t n; void* hook; void** orig; const char* name; } surf[] = {
            { 0x4E60, { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x50 }, 6, reinterpret_cast<void*>(&SurfBlt_Hook), reinterpret_cast<void**>(&g_origSurfBlt), "Blt" },
            { 0x5480, { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x40 }, 6, reinterpret_cast<void*>(&SurfBltFast_Hook), reinterpret_cast<void**>(&g_origSurfBltFast), "BltFast" },
            { 0x5D50, { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x0C }, 6, reinterpret_cast<void*>(&SurfFlip_Hook), reinterpret_cast<void**>(&g_origSurfFlip), "Flip" },
            { 0x61F0, { 0x55, 0x8B, 0xEC, 0xE8 }, 4, reinterpret_cast<void*>(&SurfLock_Hook), reinterpret_cast<void**>(&g_origSurfLock), "Lock" },
            { 0x5FD0, { 0x55, 0x8B, 0xEC, 0x56, 0x8B, 0x75 }, 6, reinterpret_cast<void*>(&SurfGetDC_Hook), reinterpret_cast<void**>(&g_origSurfGetDC), "GetDC" },
        };
        bool ok = true;
        for (auto& e : surf)
            if (memcmp(reinterpret_cast<void*>(base + e.off), e.pro, e.n) != 0) {
                PLOG_ERROR << "[ddraw] surface " << e.name << " prologue mismatch -> saved-screen hi-res text fix disabled";
                ok = false;
            }
        for (auto& e : surf) {
            if (!ok)
                break;
            void* t = reinterpret_cast<void*>(base + e.off);
            st = MH_CreateHook(t, e.hook, e.orig);
            if (st == MH_OK) st = MH_EnableHook(t);
            PLOG_DEBUG << "[ddraw] surface " << e.name << " hook status=" << st;
        }
    }
    // 신UI 글자 크기 키가 바로 닿지 않는 곳: 역할별 크기 함수, 왼쪽 위 게임 글
    {
        static const uint8_t kRoleSizePro[] = { 0x55, 0x8B, 0xEC, 0x51, 0x56, 0x8B, 0x75, 0x08, 0x57, 0x83, 0xFE, 0x06 };
        void* rs = reinterpret_cast<void*>(base + 0x404B0);
        if (memcmp(rs, kRoleSizePro, sizeof(kRoleSizePro)) == 0) {
            st = MH_CreateHook(rs, reinterpret_cast<void*>(&RoleFontSize_Hook), reinterpret_cast<void**>(&g_origRoleFontSize));
            if (st == MH_OK) st = MH_EnableHook(rs);
            PLOG_DEBUG << "[ddraw] role font size (Menu..Radial, action icon tooltip) hook status=" << st;
        } else {
            PLOG_ERROR << "[ddraw] role font size prologue mismatch";
        }
        // 행동 아이콘 툴팁 배경 불투명도
        if (g_actionTipOpacity < 100) {
            static const uint8_t kImgPro[] = { 0x55, 0x8B, 0xEC, 0x51, 0xF7, 0x45, 0x1C, 0x00, 0x00, 0x00, 0xFF, 0x56, 0x8B, 0xF1 };
            void* ai = reinterpret_cast<void*>(base + 0xD7F20);
            if (memcmp(ai, kImgPro, sizeof(kImgPro)) == 0) {
                st = MH_CreateHook(ai, reinterpret_cast<void*>(&AddImage_Hook), reinterpret_cast<void**>(&g_origAddImage));
                if (st == MH_OK) st = MH_EnableHook(ai);
                PLOG_DEBUG << "[ddraw] action icon tooltip panel opacity hook status=" << st;
            } else {
                PLOG_ERROR << "[ddraw] AddImage prologue mismatch";
            }
            uint8_t* fill = reinterpret_cast<uint8_t*>(base + 0x6E1A0);   // 68 19 1C 1E F5 = push 0xF51E1C19
            static const uint8_t kFill[] = { 0x68, 0x19, 0x1C, 0x1E, 0xF5 };
            if (memcmp(fill, kFill, sizeof(kFill)) == 0) {
                DWORD old;
                VirtualProtect(fill + 4, 1, PAGE_EXECUTE_READWRITE, &old);
                fill[4] = static_cast<uint8_t>(0xF5 * g_actionTipOpacity / 100);
                VirtualProtect(fill + 4, 1, old, &old);
            }
            uint8_t* black = reinterpret_cast<uint8_t*>(base + 0x6C938);  // 68 00 00 00 FF = push 0xFF000000
            static const uint8_t kBlack[] = { 0x68, 0x00, 0x00, 0x00, 0xFF };
            static const uint8_t kPanelPro[] = { 0x55, 0x8B, 0xEC, 0x81, 0xEC, 0x18, 0x01, 0x00, 0x00 };
            void* fp = reinterpret_cast<void*>(base + 0x6C8C0);
            DWORD old;
            if (memcmp(black, kBlack, sizeof(kBlack)) == 0 && memcmp(fp, kPanelPro, sizeof(kPanelPro)) == 0 &&
                VirtualProtect(black + 4, 1, PAGE_EXECUTE_READWRITE, &old)) {
                g_framedFillAlpha = black + 4;
                st = MH_CreateHook(fp, reinterpret_cast<void*>(&FramedPanel_Hook), reinterpret_cast<void**>(&g_origFramedPanel));
                if (st == MH_OK) st = MH_EnableHook(fp);
                PLOG_DEBUG << "[ddraw] action icon tooltip framed panel opacity hook status=" << st;
            } else {
                PLOG_ERROR << "[ddraw] framed panel bytes mismatch";
            }
        }
        static const uint8_t kSlotPro[] = { 0x55, 0x8B, 0xEC, 0x6A, 0xFF, 0x68 };
        void* sf = reinterpret_cast<void*>(base + 0x40700);
        if (memcmp(sf, kSlotPro, sizeof(kSlotPro)) == 0) {
            st = MH_CreateHook(sf, reinterpret_cast<void*>(&SlotFontSize_Hook), reinterpret_cast<void**>(&g_origSlotFontSize));
            if (st == MH_OK) st = MH_EnableHook(sf);
            PLOG_DEBUG << "[ddraw] popup slot font size hook status=" << st;
        } else {
            PLOG_ERROR << "[ddraw] popup slot font size prologue mismatch";
        }
        static const uint8_t kPopPro[] = { 0x55, 0x8B, 0xEC, 0x51, 0xF3, 0x0F, 0x10, 0x55, 0x0C };
        void* pf = reinterpret_cast<void*>(base + 0x3DB50);
        if (memcmp(pf, kPopPro, sizeof(kPopPro)) == 0) {
            st = MH_CreateHook(pf, reinterpret_cast<void*>(&PopupFontSize_Hook), reinterpret_cast<void**>(&g_origPopupFontSize));
            if (st == MH_OK) st = MH_EnableHook(pf);
            PLOG_DEBUG << "[ddraw] popup font size hook status=" << st;
        } else {
            PLOG_ERROR << "[ddraw] popup font size prologue mismatch";
        }
    }
    // FUN_5978a750 은 훅하지 않고 직접 부른다 (UiFontInfo_Hook 에서 font/그릴 px 계산용)
    static const uint8_t kDrawPro[] = { 0x55, 0x8B, 0xEC, 0x56, 0x8B, 0x75, 0x08, 0x57, 0x8B, 0x7D, 0x0C };
    if (memcmp(reinterpret_cast<void*>(base + 0xAA750), kDrawPro, sizeof(kDrawPro)) == 0)
        g_origUiFontForDraw = reinterpret_cast<tUiFontForDraw>(base + 0xAA750);
    else
        PLOG_ERROR << "[ddraw] UI font draw size function mismatch";
}

void ApplyDDrawPatches(uintptr_t baseAddress)
{
    CloseHandle(CreateThread(NULL, 0, DiagDDrawFontSlots, reinterpret_cast<LPVOID>(baseAddress), 0, NULL));

    PatchCodePage950To949(baseAddress, kDDrawCp950, _countof(kDDrawCp950), "ddraw");

    // SNEG 내레이션(DM 대사) 상자의 1차 줄바꿈: 한글은 공백에서만 (위 PatchCjkWordWrap 설명)
    PatchCjkWordWrap(baseAddress, 0x4E276, "ddraw");

    // 그래도 1차(폭 420*배율, '\n' 삽입)와 그리기의 2차(실제 폭) 줄바꿈 폭이 달라 짧은 줄이 낀다.
    // FUN_5972d520(+0x4D520) 은 U+2E80 이상 글자가 하나도 없으면(영어) 1차를 건너뛰고 원문을 돌려준다.
    // 그 "CJK 있음" 플래그를 세우지 않게 해서 한글도 영어처럼 그리기에서 한 번만 줄을 나누게 한다.
    //   +0x4D701: 81 FE 80 2E 00 00 | 0F 93 C1 (setnc cl) | 08 4D D7 (or [ebp-29],cl)
    //   → setnc cl 을 xor cl,cl; nop (32 C9 90) 로
    {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(baseAddress + 0x4D701);
        static const uint8_t kSite[] = { 0x81, 0xFE, 0x80, 0x2E, 0x00, 0x00, 0x0F, 0x93, 0xC1, 0x08, 0x4D, 0xD7 };
        if (memcmp(p, kSite, sizeof(kSite)) == 0) {
            static const uint8_t kXorCl[] = { 0x32, 0xC9, 0x90 };
            patchBytes(baseAddress + 0x4D707, kXorCl, sizeof(kXorCl));
            PLOG_DEBUG << "[ddraw] narration pre-wrap skipped for CJK";
        } else {
            PLOG_ERROR << "[ddraw] narration pre-wrap patch site mismatch";
        }
    }

    PatchPushLenStr(baseAddress + 0x4BD79, static_cast<uint32_t>(baseAddress + 0x14DA60),
                    "languages/zh-hant/sneg_ui_cjk_characters.txt", 0x2C, g_szCjkList, "CJK list");

    // 1순위 글꼴 파일 이름: language.ini [Font] UiFontFile (ASCII, 63자까지). 없으면 MaruBuri-Regular.ttf
    {
        char ini[MAX_PATH], name[64] = {};
        if (GetKoLanguageIni(ini, MAX_PATH) &&
            GetPrivateProfileStringA("Font", "UiFontFile", "", name, sizeof(name), ini) > 0)
        {
            bool ok = true;
            for (const char* c = name; *c; ++c)
                if (static_cast<unsigned char>(*c) >= 0x80 || *c == '\\' || *c == '/') { ok = false; break; }
            if (ok) lstrcpynA(g_szCjkFont, name, sizeof(g_szCjkFont));
            else PLOG_ERROR << "[ddraw] UiFontFile must be an ASCII file name in languages\\ko";
        }
    }
    PatchUiCjkFontRaster(baseAddress);

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

    // 크기별 UI 글꼴을 마루 부리 + 한글로
    InstallUiFontHooks(baseAddress);

}
