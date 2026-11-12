#include "por2.h"

#include <MinHook.h>
#include <algorithm>
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
// 크기별 조절: language.ini [UiFontSizes] 의 "요청 px(반올림)=쓸 px".
// 목록에 없는 크기는 처음 만날 때 "N=N" 으로 자동 추가한다 → ini 에 전체 목록이 생기고 값만 고치면 된다.
// 값이 키와 같으면 원래 크기(소수점 포함) 그대로.
static float AdjustUiFontSize(float size)
{
    const int key = static_cast<int>(size + 0.5f);
    if (key <= 0)
        return size;
    char ini[MAX_PATH], k[16];
    if (!GetKoLanguageIni(ini, MAX_PATH))
        return size;
    wsprintfA(k, "%d", key);
    const int v = static_cast<int>(GetPrivateProfileIntA("UiFontSizes", k, -1, ini));
    if (v < 0) {
        WritePrivateProfileStringA("UiFontSizes", k, k, ini);
        return size;
    }
    if (v == key || v < 6 || v > 96)
        return size;
    return static_cast<float>(v);
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

static void* __fastcall AddFontTTF_Hook(void* atlas, void* edx, const char* path, float size, void* cfg, const uint16_t* ranges)
{
    const char* usePath = path;
    const uint16_t* useRanges = ranges;
    float useSize = size;
    const bool swap = path && g_uiFontPath[0] && EndsWithI(path, "PoRGothic-Regular.ttf") && BuildUiRanges();
    if (swap) {
        usePath = g_uiFontPath;
        useRanges = g_uiRanges.data();
        useSize = AdjustUiFontSize(size);
    }
    void* font = g_origAddFontTTF(atlas, edx, usePath, useSize, cfg, useRanges);
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
