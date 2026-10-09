#include <windows.h>
#include "MinHook.h"

// 원본 시스템 d3d8.dll의 Direct3DCreate8 함수 포인터 타입
typedef void* (WINAPI* tDirect3DCreate8)(UINT SDKVersion);
tDirect3DCreate8 oDirect3DCreate8 = NULL;

// 후킹할 함수 선언부
void InstallHooks();

// 진짜 시스템 d3d8.dll 로드 및 Direct3DCreate8 포워딩
extern "C" void* WINAPI Fake_Direct3DCreate8(UINT SDKVersion)
{
    if (!oDirect3DCreate8)
    {
        HMODULE hOrigD3D8 = LoadLibraryA(".\\d3d8.original.dll");
        if (hOrigD3D8) {
            oDirect3DCreate8 = (tDirect3DCreate8)GetProcAddress(hOrigD3D8, "Direct3DCreate8");
        }

        // 안전한 진입 타이밍: 후킹 설치!
        InstallHooks();
    }

    if (oDirect3DCreate8) {
        return oDirect3DCreate8(SDKVersion);
    }
    return NULL;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        break;
    case DLL_PROCESS_DETACH:
        MH_Uninitialize();
        break;
    }
    return TRUE;
}