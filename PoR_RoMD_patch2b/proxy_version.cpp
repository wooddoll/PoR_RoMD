#include <windows.h>
#include <shlwapi.h>

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

HMODULE hOriginalDll = nullptr;

// 시스템 디렉터리에서 진짜 version.dll을 찾아 이름으로 주소를 로드
void LoadOriginalDll() 
{
    if (hOriginalDll != nullptr)
        return;

    char sysPath[MAX_PATH];
    GetSystemDirectoryA(sysPath, MAX_PATH);
    strcat_s(sysPath, MAX_PATH, "\\version.dll");

    HMODULE hOriginalDll = LoadLibraryA(sysPath);

    if (hOriginalDll != nullptr) 
    {
        // 이름(String)을 기반으로 원본 함수의 주소를 동적으로 획득
        // ANSI (A) 함수 주소 획득
        Original_GetFileVersionInfoA = (pfnGetFileVersionInfoA)GetProcAddress(hOriginalDll, "GetFileVersionInfoA");
        Original_GetFileVersionInfoSizeA = (pfnGetFileVersionInfoSizeA)GetProcAddress(hOriginalDll, "GetFileVersionInfoSizeA");
        Original_VerQueryValueA = (pfnVerQueryValueA)GetProcAddress(hOriginalDll, "VerQueryValueA");

        // Unicode (W) 함수 주소 획득
        Original_GetFileVersionInfoW = (pfnGetFileVersionInfoW)GetProcAddress(hOriginalDll, "GetFileVersionInfoW");
        Original_GetFileVersionInfoSizeW = (pfnGetFileVersionInfoSizeW)GetProcAddress(hOriginalDll, "GetFileVersionInfoSizeW");
        Original_VerQueryValueW = (pfnVerQueryValueW)GetProcAddress(hOriginalDll, "VerQueryValueW");
    }

    OutputDebugStringW(L"[version.dll] original dll Loaded.\n");
}

// 3. 게임이 호출할 대리 함수 (받아서 원본으로 그대로 넘겨줌)
// 대리 함수 - ANSI (A)
extern "C" BOOL WINAPI GetFileVersionInfoA(LPCSTR lptstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData)
{
    LoadOriginalDll();
    if (Original_GetFileVersionInfoA) 
        return Original_GetFileVersionInfoA(lptstrFilename, dwHandle, dwLen, lpData);
    return FALSE;
}

extern "C" DWORD WINAPI GetFileVersionInfoSizeA(LPCSTR lptstrFilename, LPDWORD lpdwHandle)
{
    LoadOriginalDll();
    if (Original_GetFileVersionInfoSizeA) 
        return Original_GetFileVersionInfoSizeA(lptstrFilename, lpdwHandle);
    return 0;
}

extern "C" BOOL WINAPI VerQueryValueA(LPCVOID pBlock, LPCSTR lpSubBlock, LPVOID* lplpBuffer, PUINT puLen)
{
    LoadOriginalDll();
    if (Original_VerQueryValueA) 
        return Original_VerQueryValueA(pBlock, lpSubBlock, lplpBuffer, puLen);
    return FALSE;
}

// 대리 함수 - Unicode (W)
extern "C" BOOL WINAPI GetFileVersionInfoW(LPCWSTR lptstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData)
{
    if (Original_GetFileVersionInfoW) 
        return Original_GetFileVersionInfoW(lptstrFilename, dwHandle, dwLen, lpData);
    return FALSE;
}

extern "C"  DWORD WINAPI GetFileVersionInfoSizeW(LPCWSTR lptstrFilename, LPDWORD lpdwHandle)
{
    LoadOriginalDll();
    if (Original_GetFileVersionInfoSizeW) 
        return Original_GetFileVersionInfoSizeW(lptstrFilename, lpdwHandle);
    return 0;
}

extern "C"  BOOL WINAPI VerQueryValueW(LPCVOID pBlock, LPCWSTR lpSubBlock, LPVOID* lplpBuffer, PUINT puLen)
{
    LoadOriginalDll();
    if (Original_VerQueryValueW) 
        return Original_VerQueryValueW(pBlock, lpSubBlock, lplpBuffer, puLen);
    return FALSE;
}
