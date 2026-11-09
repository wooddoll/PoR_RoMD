
#include <string>

#define WIN32_LEAN_AND_MEAN // 불필요한 Windows 헤더 제외 (선택 사항)
#include <windows.h>
#include <winternl.h>
#include <iostream>

#include "hooks.h"
#include "help_util.h"


// 전역 변수
PVOID g_Cookie = NULL;
bool g_IsPatched = false; // 중복 패치 방지 플래그
const char* SNEG_DLL_NAME_A = "sneg.dll";
const wchar_t* SNEG_DLL_NAME_W = L"sneg.dll";

typedef struct _LDR_DLL_LOADED_NOTIFICATION_DATA {
    ULONG Flags;
    PCUNICODE_STRING FullDllName;
    PCUNICODE_STRING BaseDllName;
    PVOID DllBase;
    ULONG SizeOfImage;
} LDR_DLL_LOADED_NOTIFICATION_DATA, *PLDR_DLL_LOADED_NOTIFICATION_DATA;

typedef union _LDR_DLL_NOTIFICATION_DATA {
    LDR_DLL_LOADED_NOTIFICATION_DATA Loaded;
} LDR_DLL_NOTIFICATION_DATA, *PLDR_DLL_NOTIFICATION_DATA;

typedef VOID (CALLBACK* PLDR_DLL_NOTIFICATION_FUNCTION)(
    ULONG NotificationReason, 
    PLDR_DLL_NOTIFICATION_DATA NotificationData, 
    PVOID Context
);
typedef NTSTATUS (NTAPI* _LdrRegisterDllNotification)(
    ULONG Flags, 
    PLDR_DLL_NOTIFICATION_FUNCTION NotificationFunction, 
    PVOID Context, 
    PVOID *Cookie);
typedef NTSTATUS (NTAPI* _LdrUnregisterDllNotification)(
    PVOID Cookie
);

void DoPatch(uintptr_t baseAddress) {
    if (g_IsPatched) return;
    g_IsPatched = true;

    // 여기서 패치 진행
    ApplyMemoryPatches(baseAddress);

    if (g_Cookie) {
        _LdrUnregisterDllNotification pLdrUnregisterDllNotification = 
            (_LdrUnregisterDllNotification)GetProcAddress(GetModuleHandleA("ntdll.dll"), "LdrUnregisterDllNotification");
        if (pLdrUnregisterDllNotification) {
            pLdrUnregisterDllNotification(g_Cookie);
            g_Cookie = NULL;
        }
    }
}

// DLL이 로드될 때마다 실행될 콜백 함수
VOID CALLBACK DllNotifyCallback(ULONG NotificationReason, PLDR_DLL_NOTIFICATION_DATA NotificationData, PVOID Context) {
    // 1 = LDR_DLL_NOTIFICATION_REASON_LOADED
    if (NotificationReason == 1) { 
        if (_wcsicmp(NotificationData->Loaded.BaseDllName->Buffer, SNEG_DLL_NAME_W) == 0) {
            uintptr_t baseAddress = reinterpret_cast<uintptr_t>(NotificationData->Loaded.DllBase);
            DoPatch(baseAddress);
        }
    }
}

// 초기화 로직 수행 함수
void patchLazyDLL() {
    // 1. 이미 로드되어 있는지 먼저 검사
    HMODULE hModule = GetModuleHandleA(SNEG_DLL_NAME_A);
    if (hModule != NULL) {
        // 이미 존재하므로 즉시 패치 진행
        DoPatch(reinterpret_cast<uintptr_t>(hModule));
        return; 
    }

    // 2. 이미 로드되어 있지 않다면 콜백 등록
    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    if (hNtdll) {
        _LdrRegisterDllNotification pLdrRegisterDllNotification = 
            (_LdrRegisterDllNotification)GetProcAddress(hNtdll, "LdrRegisterDllNotification");
        
        if (pLdrRegisterDllNotification) {
            pLdrRegisterDllNotification(0, DllNotifyCallback, NULL, &g_Cookie);
        }
    }
}

DWORD WINAPI InitHookThread(LPVOID lpParam)
{
    UNREFERENCED_PARAMETER(lpParam);

    LoadOriginalDll();
    InstallBinkHooks();

#if GLYPH_TRACKING
    plog::init(plog::debug, "error.log"); // Step2: initialize the logger
#else
    static plog::DebugOutputAppender<plog::TxtFormatter> debugOutputAppender;
    plog::init(plog::error, &debugOutputAppender);
#endif
    
    patchLazyDLL();

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