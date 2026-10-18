#include <windows.h>
#include "bink_hook.h"
#include "hooks.h"
#include <string>

DWORD WINAPI PatchThread(LPVOID lpParam){
    UNREFERENCED_PARAMETER(lpParam);
    ApplyMemoryPatches();
    return 0;
}

DWORD WINAPI InitBinkHookThread(LPVOID lpParam) {
    UNREFERENCED_PARAMETER(lpParam);
    InstallBinkHooks();
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        LoadOriginalDll();

        openLogFile();
        CreateThread(NULL, 0, PatchThread, NULL, 0, NULL);
        CreateThread(NULL, 0, InitBinkHookThread, NULL, 0, NULL);
        break;
	case DLL_PROCESS_DETACH:
        closeLogFile();
		break;
    }
    return TRUE;
}