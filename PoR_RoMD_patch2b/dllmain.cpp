#include <windows.h>

#include "bink_hook.h"
#include "hooks.h"
#include "help_util.h"


#include <string>

DWORD WINAPI InitHookThread(LPVOID lpParam)
{
    UNREFERENCED_PARAMETER(lpParam);

    LoadOriginalDll();
    InstallBinkHooks();

    plog::init(plog::debug, "korean.log"); // Step2: initialize the logger

    if (LoadSettings(L"korean.ini")) {
        //OutputDebugStringA("[version.dll proxy] korean.ini loaded.\n");
        PLOG_DEBUG << "[version.dll proxy] korean.ini loaded.";
    }
    else {
        //OutputDebugStringA("[version.dll proxy] korean.ini load failed.\n");
        PLOG_ERROR << "[version.dll proxy] korean.ini load failed.";
    }
    

    ApplyMemoryPatches();

    return 0;
}


BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        {
            HANDLE hThread = CreateThread(NULL, 0, InitHookThread, NULL, 0, NULL);
            if (hThread)
                CloseHandle(hThread); // 핸들 누수 방지
        }
        break;
	case DLL_PROCESS_DETACH:
        RemoveBinkHooks();
		break;
    }
    return TRUE;
}