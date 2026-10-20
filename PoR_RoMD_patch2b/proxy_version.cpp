#include <mutex>

#define WIN32_LEAN_AND_MEAN // 불필요한 Windows 헤더 제외 (선택 사항)
#include <windows.h>

#include "help_util.h"

// 1. 원본 함수의 타입(원형) 정의
typedef BOOL(WINAPI* pfnGetFileVersionInfoA)(LPCSTR, DWORD, DWORD, LPVOID);
typedef DWORD(WINAPI* pfnGetFileVersionInfoSizeA)(LPCSTR, LPDWORD);
typedef BOOL(WINAPI* pfnVerQueryValueA)(LPCVOID, LPCSTR, LPVOID*, PUINT);

typedef BOOL(WINAPI* pfnGetFileVersionInfoW)(LPCWSTR, DWORD, DWORD, LPVOID);
typedef DWORD(WINAPI* pfnGetFileVersionInfoSizeW)(LPCWSTR, LPDWORD);
typedef BOOL(WINAPI* pfnVerQueryValueW)(LPCVOID, LPCWSTR, LPVOID*, PUINT);

// 2. 실제 함수 주소를 담을 포인터 변수
pfnGetFileVersionInfoA     Original_GetFileVersionInfoA = nullptr;
pfnGetFileVersionInfoSizeA Original_GetFileVersionInfoSizeA = nullptr;
pfnVerQueryValueA          Original_VerQueryValueA = nullptr;

pfnGetFileVersionInfoW     Original_GetFileVersionInfoW = nullptr;
pfnGetFileVersionInfoSizeW Original_GetFileVersionInfoSizeW = nullptr;
pfnVerQueryValueW          Original_VerQueryValueW = nullptr;


template <typename T>
bool LoadProc(HMODULE module, const char* name, T& outFunc)
{
    outFunc = reinterpret_cast<T>(GetProcAddress(module, name));

    if (outFunc == nullptr) {
        //OutputDebugStringA("[version.dll proxy] GetProcAddress failed: ");
        //OutputDebugStringA(name);
        //OutputDebugStringA("\n");
        PLOG_ERROR << "[version.dll proxy] GetProcAddress failed: " << name;
        return false;
    }

    return true;
}

static std::once_flag g_LoadOriginalDllFlag;
static HMODULE hOriginalDll = nullptr;

// 시스템 디렉터리에서 진짜 version.dll을 찾아 이름으로 주소를 로드
void LoadOriginalDll()
{
    if (hOriginalDll != nullptr) {
        return;
    }

    wchar_t sysPath[MAX_PATH] = {};
    UINT len = GetSystemDirectoryW(sysPath, MAX_PATH);

    if (len == 0 || len >= MAX_PATH) {
        //OutputDebugStringA("[version.dll proxy] GetSystemDirectoryW failed.\n");
		PLOG_ERROR << "[version.dll proxy] GetSystemDirectoryW failed.";
        return;
    }

    if (wcscat_s(sysPath, MAX_PATH, L"\\version.dll") != 0) {
        //OutputDebugStringA("[version.dll proxy] wcscat_s failed.\n");
		PLOG_ERROR << "[version.dll proxy] wcscat_s failed.";
        return;
    }

    hOriginalDll = LoadLibraryW(sysPath);

    if (hOriginalDll == nullptr) {
        //OutputDebugStringA("[version.dll proxy] LoadLibraryW failed: system version.dll\n");
		PLOG_ERROR << "[version.dll proxy] LoadLibraryW failed: system version.dll";
        return;
    }

    bool ok = true;

    ok &= LoadProc(hOriginalDll, "GetFileVersionInfoA",     Original_GetFileVersionInfoA);
    ok &= LoadProc(hOriginalDll, "GetFileVersionInfoSizeA", Original_GetFileVersionInfoSizeA);
    ok &= LoadProc(hOriginalDll, "VerQueryValueA",          Original_VerQueryValueA);

    ok &= LoadProc(hOriginalDll, "GetFileVersionInfoW",     Original_GetFileVersionInfoW);
    ok &= LoadProc(hOriginalDll, "GetFileVersionInfoSizeW", Original_GetFileVersionInfoSizeW);
    ok &= LoadProc(hOriginalDll, "VerQueryValueW",          Original_VerQueryValueW);

    if (ok) {
        //OutputDebugStringA("[version.dll proxy] original version.dll loaded successfully.\n");
		PLOG_DEBUG << "[version.dll proxy] original version.dll loaded successfully.";
    }
    else {
        //OutputDebugStringA("[version.dll proxy] original version.dll loaded, but some functions failed.\n");
		PLOG_ERROR << "[version.dll proxy] original version.dll loaded, but some functions failed.";
    }
}



// 3. 게임이 호출할 대리 함수 (받아서 원본으로 그대로 넘겨줌)
// 대리 함수 - ANSI (A)
extern "C" BOOL WINAPI GetFileVersionInfoA(LPCSTR lptstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData)
{
    std::call_once(g_LoadOriginalDllFlag, LoadOriginalDll);
    if (Original_GetFileVersionInfoA) 
        return Original_GetFileVersionInfoA(lptstrFilename, dwHandle, dwLen, lpData);
    return FALSE;
}

extern "C" DWORD WINAPI GetFileVersionInfoSizeA(LPCSTR lptstrFilename, LPDWORD lpdwHandle)
{
    std::call_once(g_LoadOriginalDllFlag, LoadOriginalDll);
    if (Original_GetFileVersionInfoSizeA) 
        return Original_GetFileVersionInfoSizeA(lptstrFilename, lpdwHandle);
    return 0;
}

extern "C" BOOL WINAPI VerQueryValueA(LPCVOID pBlock, LPCSTR lpSubBlock, LPVOID* lplpBuffer, PUINT puLen)
{
    std::call_once(g_LoadOriginalDllFlag, LoadOriginalDll);
    if (Original_VerQueryValueA) 
        return Original_VerQueryValueA(pBlock, lpSubBlock, lplpBuffer, puLen);
    return FALSE;
}

// 대리 함수 - Unicode (W)
extern "C" BOOL WINAPI GetFileVersionInfoW(LPCWSTR lptstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData)
{
    std::call_once(g_LoadOriginalDllFlag, LoadOriginalDll);
    if (Original_GetFileVersionInfoW) 
        return Original_GetFileVersionInfoW(lptstrFilename, dwHandle, dwLen, lpData);
    return FALSE;
}

extern "C"  DWORD WINAPI GetFileVersionInfoSizeW(LPCWSTR lptstrFilename, LPDWORD lpdwHandle)
{
    std::call_once(g_LoadOriginalDllFlag, LoadOriginalDll);
    if (Original_GetFileVersionInfoSizeW) 
        return Original_GetFileVersionInfoSizeW(lptstrFilename, lpdwHandle);
    return 0;
}

extern "C"  BOOL WINAPI VerQueryValueW(LPCVOID pBlock, LPCWSTR lpSubBlock, LPVOID* lplpBuffer, PUINT puLen)
{
    std::call_once(g_LoadOriginalDllFlag, LoadOriginalDll);
    if (Original_VerQueryValueW) 
        return Original_VerQueryValueW(pBlock, lpSubBlock, lplpBuffer, puLen);
    return FALSE;
}
