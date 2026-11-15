// ---------------------------------------------------------------------------
// F5 빠른 저장 ("QUICKSAVE"), F6 빠른 불러오기, 휴식 후 자동 저장 ("AUTOSAVE")
//
// 게임 저장 함수 CGame::SaveFile (exe 0x48B9A0, __thiscall):
//   this = *g (g = *(int**)0x724FF8), (이름, 레벨 = g[3], 지울 예전 파일 = 0, 자동 = 0)
//   게임 저장 메뉴(FUN_004776d0 case 0x29)와 같은 인자. 같은 이름의 세이브가 있으면 찾아서 지우고 새로 쓴다
//   → 이름마다 한 칸만 계속 덮어쓴다.
// 부르는 곳: sneg FUN_561ff8b0 (sneg+0x4F8B0) = SNEG 자동저장 검사. 매 프레임 게임 스레드의 입력 처리 끝
//   (FUN_561f8610)에서 불린다 → 이 함수를 훅해 원본 다음에 키와 휴식 종료를 확인한다.
// 저장 조건은 SNEG 자동저장과 같게: 싱글플레이, 탐험 상태(0x6AC960 == 3), 진행 중인 일 없음(g[1], g[2], 0x6E7868 == -1).
// 결과 표시: SNEG 가 진형 저장 알림에 쓰는 화면 메시지 함수 (exe 0x440610, this = *(*(0x727290) + 0x2C)).
//
// 게임 메시지 큐: FUN_0047e4c0 (this = *(0x727294), 18 인자) 로 넣고, 받는 쪽이 FUN_0047e5a0 으로 꺼낸다.
//   인자 1,2 = 보내는 쪽, 3,4 = 받는 쪽 (게임 = 6,0), 5 = 사건 번호, 6 = 값 (+0x20), 13 = 1 (바로 처리 등록).
//   게임 사건 처리는 exe 0x483260 의 큰 switch (사건 - 5 → 0x4862C0 표 → 0x486278 주소표).
// 불러오기: 불러오기 메뉴(FUN_004719f0) 의 '불러오기' 버튼이 "bas_%u.sav" 의 번호를 사건 0x28 의 값으로 보낸다
//   → 게임(0x484B81)이 그 번호의 세이브를 읽는다. F6 은 이름이 "QUICKSAVE" 인 세이브를 찾아 같은 사건을 보낸다.
// 휴식: 사건 0x2F, 값 1 = 휴식 시작 (0x483BA8, 휴식 그림·resting.wav), 값 0 = 휴식 끝 (0x483EB9, 시간 흐름·회복).
//   → 메시지 큐에 넣는 함수를 훅해 (0x2F, 0) 을 보면, 처리가 끝나고 저장 조건이 맞을 때 자동 저장한다.
// ---------------------------------------------------------------------------
#include "por2.h"

#include <MinHook.h>
#include <cstdio>
#include "help_util.h"

namespace {

typedef char(__thiscall* tSaveFile)(void* game, const char* name, int level, const char* oldFile, uint8_t isAuto);
typedef int(__thiscall* tShowMessage)(void* self, const char* text, int x, int y, float duration,
                                      int r, int g, int b, uint8_t p8, char p9);
typedef void(__cdecl* tAutosaveTick)();
typedef uint32_t(__fastcall* tPostEvent)(void* queue, void* edx,
                                         uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t ev, uint32_t value,
                                         uint32_t p7, uint32_t p8, uint32_t p9, uint32_t p10, uint32_t p11, uint32_t p12,
                                         uint32_t p13, uint32_t p14, uint32_t p15, uint32_t p16, uint32_t p17, uint32_t p18);

const uintptr_t kSaveFile = 0x0048B9A0;
const uintptr_t kShowMessage = 0x00440610;
const uintptr_t kPostEvent = 0x0047E4C0;
const uintptr_t kEventQueue = 0x00727294;      // 메시지 큐 (this)
const uintptr_t kGameGlobals = 0x00724FF8;     // int* g
const uintptr_t kGameMode = 0x006AC960;        // 3 = 탐험
const uintptr_t kPendingAction = 0x006E7868;   // -1 = 없음
const uintptr_t kMainWnd = 0x007012E4;         // HWND
const uintptr_t kMsgOwner = 0x00727290;

const uint32_t kEvLoad = 0x28;
const uint32_t kEvRest = 0x2F;

tAutosaveTick g_origAutosaveTick = nullptr;
tPostEvent    g_origPostEvent = nullptr;
bool  g_f5Down = false;
bool  g_f6Down = false;
DWORD g_lastKey = 0;
DWORD g_restEnded = 0;      // 휴식 끝 사건을 본 시각 (0 = 없음)
DWORD g_loadPosted = 0;     // 빠른 불러오기 사건을 보낸 시각 (0 = 없음)

// 메시지는 '달리기 ON' 같은 상태 메시지와 같은 왼쪽 위 창(SNEG 가 ddraw TTF 로 다시 그림)에 나온다.
// 게임 글은 EUC-KR(949) 이므로 소스의 UTF-8 문자열을 949 로 바꿔 넘긴다.
void ShowMessage(const char* utf8)
{
    wchar_t w[128];
    char mb[256];
    if (!MultiByteToWideChar(CP_UTF8, 0, utf8, -1, w, _countof(w)) ||
        !WideCharToMultiByte(949, 0, w, -1, mb, sizeof(mb), nullptr, nullptr))
        return;
    __try {
        const uintptr_t owner = *reinterpret_cast<const uintptr_t*>(kMsgOwner);
        void* self = owner ? *reinterpret_cast<void* const*>(owner + 0x2C) : nullptr;
        if (self) {
            reinterpret_cast<tShowMessage>(kShowMessage)(self, mb, 250, 500, 10000.0f, 255, 255, 255, 0, 0);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

int* GameGlobals()
{
    int* g = *reinterpret_cast<int**>(kGameGlobals);
    if (!g || !g[0] || !g[3])
        return nullptr;
    if (*reinterpret_cast<const uint8_t*>(reinterpret_cast<uintptr_t>(g) + 0x21) != 0)   // 멀티플레이
        return nullptr;
    return g;
}

// 지금 저장해도 되는가 (SNEG 자동저장 조건과 같음)
bool CanSave(int*& g)
{
    g = GameGlobals();
    if (!g || g[1] != 0 || g[2] != 0)
        return false;
    return *reinterpret_cast<const int*>(kGameMode) == 3 && *reinterpret_cast<const int*>(kPendingAction) == -1;
}

bool SaveAs(int* g, const char* name)
{
    const char ok = reinterpret_cast<tSaveFile>(kSaveFile)(reinterpret_cast<void*>(g[0]), name, g[3], nullptr, 0);
    PLOG_DEBUG << "[quicksave] SaveFile(" << name << ") = " << static_cast<int>(ok);
    return ok != 0;
}

void QuickSave()
{
    int* g = nullptr;
    if (!CanSave(g)) {
        PLOG_DEBUG << "[quicksave] not allowed now (mode " << *reinterpret_cast<const int*>(kGameMode) << ")";
        ShowMessage(u8"지금은 빠른 저장을 할 수 없습니다");
        return;
    }
    ShowMessage(SaveAs(g, "QUICKSAVE") ? u8"빠른 저장 완료" : u8"빠른 저장 실패");
}

// 세이브 폴더: SNEG 는 게임의 ./savegames 를 %LOCALAPPDATA%\SNEG\POR2\savegames 로 돌린다. 없으면 게임 폴더 쪽.
bool SaveDir(char* out, size_t cb)
{
    char local[MAX_PATH];
    const DWORD n = GetEnvironmentVariableA("LOCALAPPDATA", local, MAX_PATH);
    if (n > 0 && n < MAX_PATH) {
        _snprintf_s(out, cb, _TRUNCATE, "%s\\SNEG\\POR2\\savegames", local);
        const DWORD attr = GetFileAttributesA(out);
        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
            return true;
    }
    lstrcpynA(out, ".\\savegames", static_cast<int>(cb));
    return true;
}

// 헤더(0x58 바이트) 맨 앞의 세이브 이름이 name 인 파일의 번호 (bas_%u.sav). 여러 개면 가장 최근 것. 없으면 false
bool FindSaveNumber(const char* name, uint32_t& number)
{
    char dir[MAX_PATH], pattern[MAX_PATH];
    SaveDir(dir, sizeof(dir));
    _snprintf_s(pattern, sizeof(pattern), _TRUNCATE, "%s\\*.sav", dir);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    bool found = false;
    FILETIME newest = {};
    do {
        unsigned int n = 0;
        if (sscanf_s(fd.cFileName, "bas_%u.sav", &n) != 1)
            continue;
        char path[MAX_PATH];
        _snprintf_s(path, sizeof(path), _TRUNCATE, "%s\\%s", dir, fd.cFileName);
        char header[0x58] = {};
        HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (f == INVALID_HANDLE_VALUE)
            continue;
        DWORD read = 0;
        const BOOL ok = ReadFile(f, header, sizeof(header), &read, nullptr);
        CloseHandle(f);
        header[sizeof(header) - 1] = 0;
        if (!ok || read < 0x50 || lstrcmpA(header, name) != 0)
            continue;
        if (!found || CompareFileTime(&fd.ftLastWriteTime, &newest) > 0) {
            newest = fd.ftLastWriteTime;
            number = n;
            found = true;
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    return found;
}

void QuickLoad()
{
    int* g = GameGlobals();
    if (!g || *reinterpret_cast<const int*>(kGameMode) != 3) {
        ShowMessage(u8"지금은 빠른 불러오기를 할 수 없습니다");
        return;
    }
    uint32_t number = 0;
    if (!FindSaveNumber("QUICKSAVE", number)) {
        PLOG_DEBUG << "[quicksave] QUICKSAVE not found";
        ShowMessage(u8"빠른 저장 파일이 없습니다");
        return;
    }
    void* queue = *reinterpret_cast<void**>(kEventQueue);
    PLOG_DEBUG << "[quicksave] quick load bas_" << number << ".sav";
    if (!queue)
        return;
    // 불러오기 메뉴와 같은 사건: 게임(6,0) 에게 0x28, 값 = 세이브 번호, 13번째 인자 = 1
    reinterpret_cast<tPostEvent>(kPostEvent)(queue, nullptr,
                                             6, 0, 6, 0, kEvLoad, number, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0);
    g_loadPosted = GetTickCount();
}

// 불러오기는 화면·메시지 창을 새로 만들므로, 끝난 뒤(게임 화면으로 돌아온 뒤) 완료 메시지를 띄운다
void CheckLoadDone()
{
    if (!g_loadPosted)
        return;
    const DWORD elapsed = GetTickCount() - g_loadPosted;
    if (elapsed < 500)
        return;
    if (elapsed > 30000) {
        g_loadPosted = 0;
        return;
    }
    int* g = nullptr;
    if (!CanSave(g))
        return;
    g_loadPosted = 0;
    ShowMessage(u8"빠른 불러오기 완료");
}

uint32_t __fastcall PostEvent_Hook(void* queue, void* edx,
                                   uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t ev, uint32_t value,
                                   uint32_t p7, uint32_t p8, uint32_t p9, uint32_t p10, uint32_t p11, uint32_t p12,
                                   uint32_t p13, uint32_t p14, uint32_t p15, uint32_t p16, uint32_t p17, uint32_t p18)
{
    if (ev == kEvRest && p3 == 6 && p4 == 0) {
        PLOG_DEBUG << "[quicksave] rest event value " << value;
        if (value == 0)
            g_restEnded = GetTickCount();
    }
    return g_origPostEvent(queue, edx, p1, p2, p3, p4, ev, value, p7, p8, p9, p10, p11, p12,
                           p13, p14, p15, p16, p17, p18);
}

// 휴식이 끝나면 회복·시간 처리 뒤 저장 조건이 맞을 때 "AUTOSAVE" 로 저장 (1.5초 기다림, 60초 안에 못 하면 포기)
void CheckRestAutosave()
{
    if (!g_restEnded)
        return;
    const DWORD elapsed = GetTickCount() - g_restEnded;
    if (elapsed < 1500)
        return;
    if (elapsed > 60000) {
        PLOG_DEBUG << "[quicksave] rest autosave skipped (no safe moment)";
        g_restEnded = 0;
        return;
    }
    int* g = nullptr;
    if (!CanSave(g))
        return;
    g_restEnded = 0;
    if (SaveAs(g, "AUTOSAVE"))
        ShowMessage(u8"휴식 후 자동 저장 완료");
}

// 키를 막 눌렀는가 (게임 창이 앞에 있을 때만)
bool KeyPressed(int vk, bool& wasDown)
{
    const bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
    const bool pressed = down && !wasDown;
    wasDown = down;
    if (!pressed)
        return false;
    const HWND wnd = *reinterpret_cast<const HWND*>(kMainWnd);
    return wnd && GetForegroundWindow() == wnd;
}

void __cdecl AutosaveTick_Hook()
{
    g_origAutosaveTick();

    CheckRestAutosave();
    CheckLoadDone();

    const bool f5 = KeyPressed(VK_F5, g_f5Down);
    const bool f6 = KeyPressed(VK_F6, g_f6Down);
    if (!f5 && !f6)
        return;
    const DWORD now = GetTickCount();
    if (now - g_lastKey < 1000)
        return;
    g_lastKey = now;
    if (f5)
        QuickSave();
    else
        QuickLoad();
}

MH_STATUS Hook(uintptr_t target, void* detour, void** orig)
{
    MH_STATUS st = MH_Initialize();
    if (st != MH_OK && st != MH_ERROR_ALREADY_INITIALIZED)
        return st;
    st = MH_CreateHook(reinterpret_cast<LPVOID>(target), detour, orig);
    if (st == MH_OK)
        st = MH_EnableHook(reinterpret_cast<LPVOID>(target));
    return st;
}

} // namespace

void InstallQuickSaveHook(uintptr_t snegBase)
{
    const uintptr_t target = snegBase + 0x4F8B0;
    static const uint8_t kPrologue[] = { 0x55, 0x8B, 0xEC, 0x81, 0xEC, 0x04, 0x01, 0x00, 0x00 };
    if (memcmp(reinterpret_cast<const void*>(target), kPrologue, sizeof(kPrologue)) != 0) {
        PLOG_ERROR << "[quicksave] sneg autosave tick prologue mismatch";
        return;
    }
    // 메시지 큐에 넣는 함수: push esi; push 4Ch; mov esi, ecx
    static const uint8_t kPostPrologue[] = { 0x56, 0x6A, 0x4C, 0x8B, 0xF1 };
    if (memcmp(reinterpret_cast<const void*>(kPostEvent), kPostPrologue, sizeof(kPostPrologue)) == 0) {
        const MH_STATUS st = Hook(kPostEvent, reinterpret_cast<void*>(&PostEvent_Hook), reinterpret_cast<void**>(&g_origPostEvent));
        PLOG_DEBUG << "[quicksave] rest event hook status=" << st;
    } else {
        PLOG_ERROR << "[quicksave] post event prologue mismatch (rest autosave off)";
    }
    const MH_STATUS st = Hook(target, reinterpret_cast<void*>(&AutosaveTick_Hook), reinterpret_cast<void**>(&g_origAutosaveTick));
    PLOG_DEBUG << "[quicksave] F5/F6 hook status=" << st;
}
