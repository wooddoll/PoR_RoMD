
#include <string>

#define WIN32_LEAN_AND_MEAN // 불필요한 Windows 헤더 제외 (선택 사항)
#include <windows.h>

#include "hooks.h"
#include "help_util.h"

DWORD WINAPI InitHookThread(LPVOID lpParam)
{
    UNREFERENCED_PARAMETER(lpParam);

    LoadOriginalDll();
    InstallBinkHooks();

    static plog::DebugOutputAppender<plog::TxtFormatter> debugOutputAppender;
    //plog::init(plog::error, "error.log"); // Step2: initialize the logger
    plog::init(plog::debug, &debugOutputAppender);
    /*
    bool ret = LoadSettings(L"language.ini");
    resetLogLevel();

    if (ret) {
        //OutputDebugStringA("[version.dll proxy] korean.ini loaded.\n");
        PLOG_DEBUG << "[version.dll proxy] language.ini loaded.";
    }
    else {
        //OutputDebugStringA("[version.dll proxy] korean.ini load failed.\n");
        PLOG_ERROR << "[version.dll proxy] language.ini load failed.";
    }
    */
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