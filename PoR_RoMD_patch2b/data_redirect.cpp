#include "data_redirect.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include <string>
#include <unordered_map>

#include "help_util.h"

namespace {

typedef HANDLE(WINAPI* tCreateFileA)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
typedef DWORD(WINAPI* tGetFileAttributesA)(LPCSTR);
typedef HANDLE(WINAPI* tFindFirstFileA)(LPCSTR, LPWIN32_FIND_DATAA);
typedef BOOL(WINAPI* tFindNextFileA)(HANDLE, LPWIN32_FIND_DATAA);
typedef BOOL(WINAPI* tFindClose)(HANDLE);

tCreateFileA        o_CreateFileA = nullptr;
tGetFileAttributesA o_GetFileAttributesA = nullptr;
tFindFirstFileA     o_FindFirstFileA = nullptr;
tFindNextFileA      o_FindNextFileA = nullptr;
tFindClose          o_FindClose = nullptr;

std::string g_gameDir;        // "D:\Games\Pool of Radiance"
std::string g_gameDirLower;
std::string g_altName;        // "Data_kr"
std::string g_altNameLower;
std::string g_altDir;         // "D:\Games\Pool of Radiance\Data_kr"

SRWLOCK g_lock = SRWLOCK_INIT;
std::unordered_map<std::string, std::string> g_cache;     // 소문자 전체 경로 -> 대체 경로 ("" = 없음)
std::unordered_map<HANDLE, std::string> g_findDirs;       // FindFirstFileA 핸들 -> 검색 폴더 ("...\")

std::string ToLowerAscii(std::string s)
{
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    }
    return s;
}

bool HasWildcard(const char* p) { return strpbrk(p, "*?") != nullptr; }

bool GetFull(const char* path, std::string& out)
{
    char buf[MAX_PATH];
    DWORD n = GetFullPathNameA(path, MAX_PATH, buf, nullptr);
    if (n == 0 || n >= MAX_PATH)
        return false;
    out = buf;
    return true;
}

bool IsRegularFile(const std::string& p)
{
    DWORD a = o_GetFileAttributesA(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

/* <게임>\Data\X -> <게임>\Data_kr\X, <게임>\file -> <게임>\Data_kr\file
   그 밖의 하위 폴더(저장 파일 등)와 Data_kr 자신은 대상이 아니다. */
bool MapToAlt(const std::string& full, std::string& alt)
{
    const std::string low = ToLowerAscii(full);
    const size_t n = g_gameDirLower.size();
    if (low.size() <= n + 1 || low.compare(0, n, g_gameDirLower) != 0 || low[n] != '\\')
        return false;

    std::string rel = full.substr(n + 1);
    const std::string relLow = low.substr(n + 1);

    if (relLow.compare(0, g_altNameLower.size() + 1, g_altNameLower + "\\") == 0)
        return false;                                   // 이미 Data_kr 안
    if (relLow.compare(0, 5, "data\\") == 0)
        rel = rel.substr(5);                            // Data\X -> X
    else if (rel.find('\\') != std::string::npos)
        return false;                                   // 루트 파일만 (sfDat.dat 등)

    alt = g_altDir + "\\" + rel;
    return true;
}

bool Resolve(const char* path, std::string& out)
{
    if (path == nullptr || path[0] == '\0' || g_altDir.empty() || HasWildcard(path))
        return false;

    std::string full;
    if (!GetFull(path, full))
        return false;
    const std::string key = ToLowerAscii(full);

    AcquireSRWLockShared(&g_lock);
    auto it = g_cache.find(key);
    const bool cached = it != g_cache.end();
    std::string val = cached ? it->second : std::string();
    ReleaseSRWLockShared(&g_lock);

    if (!cached) {
        std::string alt;
        if (MapToAlt(full, alt) && IsRegularFile(alt))
            val = alt;
        AcquireSRWLockExclusive(&g_lock);
        g_cache[key] = val;
        ReleaseSRWLockExclusive(&g_lock);
        if (!val.empty())
            PLOG_DEBUG << "[DataRedirect] " << full << " -> " << val;
    }

    if (val.empty())
        return false;
    out = val;
    return true;
}

/* 와일드카드 검색 결과의 크기/시간을 Data_kr 파일 기준으로 바꾼다.
   (목록을 보고 버퍼를 잡은 뒤 읽는 로더가 있어도 크기가 맞도록) */
void FixFindData(const std::string& dir, WIN32_FIND_DATAA* fd)
{
    if (fd == nullptr || (fd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        return;
    std::string alt;
    if (!Resolve((dir + fd->cFileName).c_str(), alt))
        return;
    WIN32_FILE_ATTRIBUTE_DATA ad;
    if (GetFileAttributesExA(alt.c_str(), GetFileExInfoStandard, &ad)) {
        fd->nFileSizeHigh = ad.nFileSizeHigh;
        fd->nFileSizeLow = ad.nFileSizeLow;
        fd->ftLastWriteTime = ad.ftLastWriteTime;
    }
}

// ---------------------------------------------------------------- hooks
HANDLE WINAPI Hook_CreateFileA(LPCSTR name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES sa,
                               DWORD disposition, DWORD flags, HANDLE templ)
{
    // 읽기 전용으로 기존 파일을 여는 경우만 (저장 파일 쓰기 등은 그대로)
    const DWORD writeBits = GENERIC_WRITE | GENERIC_ALL | FILE_WRITE_DATA | FILE_APPEND_DATA | DELETE;
    std::string alt;
    if ((access & writeBits) == 0 && disposition == OPEN_EXISTING && Resolve(name, alt))
        return o_CreateFileA(alt.c_str(), access, share, sa, disposition, flags, templ);
    return o_CreateFileA(name, access, share, sa, disposition, flags, templ);
}

DWORD WINAPI Hook_GetFileAttributesA(LPCSTR name)
{
    std::string alt;
    if (Resolve(name, alt))
        return o_GetFileAttributesA(alt.c_str());
    return o_GetFileAttributesA(name);
}

HANDLE WINAPI Hook_FindFirstFileA(LPCSTR pattern, LPWIN32_FIND_DATAA fd)
{
    std::string alt;
    if (Resolve(pattern, alt))                          // 파일 하나 (존재/크기 확인용, CRT _stat 등)
        return o_FindFirstFileA(alt.c_str(), fd);

    HANDLE h = o_FindFirstFileA(pattern, fd);
    if (h == INVALID_HANDLE_VALUE || pattern == nullptr || !HasWildcard(pattern) || g_altDir.empty())
        return h;

    std::string full;
    if (!GetFull(pattern, full))
        return h;
    const size_t slash = full.find_last_of('\\');
    if (slash == std::string::npos)
        return h;
    const std::string dir = full.substr(0, slash + 1);
    std::string probe;
    if (!MapToAlt(dir + "x", probe))                    // 대상 폴더가 아니면 그대로
        return h;

    AcquireSRWLockExclusive(&g_lock);
    g_findDirs[h] = dir;
    ReleaseSRWLockExclusive(&g_lock);
    FixFindData(dir, fd);
    return h;
}

BOOL WINAPI Hook_FindNextFileA(HANDLE h, LPWIN32_FIND_DATAA fd)
{
    BOOL r = o_FindNextFileA(h, fd);
    if (r) {
        AcquireSRWLockShared(&g_lock);
        auto it = g_findDirs.find(h);
        std::string dir = (it != g_findDirs.end()) ? it->second : std::string();
        ReleaseSRWLockShared(&g_lock);
        if (!dir.empty())
            FixFindData(dir, fd);
    }
    return r;
}

BOOL WINAPI Hook_FindClose(HANDLE h)
{
    AcquireSRWLockExclusive(&g_lock);
    g_findDirs.erase(h);
    ReleaseSRWLockExclusive(&g_lock);
    return o_FindClose(h);
}

// ---------------------------------------------------------------- IAT
/* module 의 import 중 dll!func 슬롯을 hook 으로 바꾼다.
   OriginalFirstThunk 의 이름으로 찾고, 없으면 GetProcAddress 주소로 찾는다. */
bool PatchImport(HMODULE module, const char* dll, const char* func, void* hook, void** orig)
{
    BYTE* base = reinterpret_cast<BYTE*>(module);
    auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    const IMAGE_DATA_DIRECTORY& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (dir.VirtualAddress == 0)
        return false;

    void* target = reinterpret_cast<void*>(GetProcAddress(GetModuleHandleA(dll), func));

    for (auto imp = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress); imp->Name; ++imp) {
        if (_stricmp(reinterpret_cast<const char*>(base + imp->Name), dll) != 0)
            continue;
        auto iat = reinterpret_cast<IMAGE_THUNK_DATA*>(base + imp->FirstThunk);
        auto names = imp->OriginalFirstThunk
            ? reinterpret_cast<IMAGE_THUNK_DATA*>(base + imp->OriginalFirstThunk) : nullptr;

        for (int i = 0; iat[i].u1.Function; ++i) {
            bool match = false;
            if (names && !(names[i].u1.Ordinal & IMAGE_ORDINAL_FLAG)) {
                auto ibn = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + names[i].u1.AddressOfData);
                match = strcmp(reinterpret_cast<const char*>(ibn->Name), func) == 0;
            }
            else if (target) {
                match = reinterpret_cast<void*>(iat[i].u1.Function) == target;
            }
            if (!match)
                continue;

            *orig = reinterpret_cast<void*>(iat[i].u1.Function);
            return patchDWORD(reinterpret_cast<uintptr_t>(&iat[i].u1.Function),
                              static_cast<uint32_t>(reinterpret_cast<uintptr_t>(hook)));
        }
    }
    return false;
}

} // namespace

bool DataRedirect_Resolve(const char* path, std::string& out)
{
    return Resolve(path, out);
}

bool InstallDataRedirect()
{
    char exePath[MAX_PATH] = {};
    DWORD n = GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    if (n == 0 || n >= MAX_PATH)
        return false;
    char* lastSlash = strrchr(exePath, '\\');
    if (lastSlash == nullptr)
        return false;
    *lastSlash = '\0';
    g_gameDir = exePath;
    g_gameDirLower = ToLowerAscii(g_gameDir);

    // DllMain 안이므로 IniConfig 대신 GetPrivateProfile* 로 직접 읽는다.
    const std::string ini = g_gameDir + "\\language.ini";
    if (GetPrivateProfileIntA("Settings", "DataRedirect", 1, ini.c_str()) == 0)
        return false;
    char name[MAX_PATH] = {};
    GetPrivateProfileStringA("Settings", "DataPath", "Data_kr", name, MAX_PATH, ini.c_str());
    g_altName = name[0] ? name : "Data_kr";
    for (char& c : g_altName) {
        if (c == '/') c = '\\';
    }
    while (!g_altName.empty() && g_altName.back() == '\\')
        g_altName.pop_back();
    g_altNameLower = ToLowerAscii(g_altName);
    g_altDir = g_gameDir + "\\" + g_altName;

    // 대체 폴더가 없으면 아무것도 하지 않는다 (원본 그대로 실행)
    DWORD attr = GetFileAttributesA(g_altDir.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        g_altDir.clear();
        return false;
    }

    // o_ 포인터는 먼저 실제 함수로 채워 둔다 (PatchImport 가 IAT 값으로 덮어씀)
    HMODULE k32 = GetModuleHandleA("KERNEL32.dll");
    o_CreateFileA = reinterpret_cast<tCreateFileA>(GetProcAddress(k32, "CreateFileA"));
    o_GetFileAttributesA = reinterpret_cast<tGetFileAttributesA>(GetProcAddress(k32, "GetFileAttributesA"));
    o_FindFirstFileA = reinterpret_cast<tFindFirstFileA>(GetProcAddress(k32, "FindFirstFileA"));
    o_FindNextFileA = reinterpret_cast<tFindNextFileA>(GetProcAddress(k32, "FindNextFileA"));
    o_FindClose = reinterpret_cast<tFindClose>(GetProcAddress(k32, "FindClose"));

    HMODULE exe = GetModuleHandleA(nullptr);
    bool ok = true;
    ok &= PatchImport(exe, "KERNEL32.dll", "CreateFileA", reinterpret_cast<void*>(Hook_CreateFileA), reinterpret_cast<void**>(&o_CreateFileA));
    ok &= PatchImport(exe, "KERNEL32.dll", "GetFileAttributesA", reinterpret_cast<void*>(Hook_GetFileAttributesA), reinterpret_cast<void**>(&o_GetFileAttributesA));
    ok &= PatchImport(exe, "KERNEL32.dll", "FindFirstFileA", reinterpret_cast<void*>(Hook_FindFirstFileA), reinterpret_cast<void**>(&o_FindFirstFileA));
    ok &= PatchImport(exe, "KERNEL32.dll", "FindNextFileA", reinterpret_cast<void*>(Hook_FindNextFileA), reinterpret_cast<void**>(&o_FindNextFileA));
    ok &= PatchImport(exe, "KERNEL32.dll", "FindClose", reinterpret_cast<void*>(Hook_FindClose), reinterpret_cast<void**>(&o_FindClose));
    return ok;
}
