
#include <string>

#define WIN32_LEAN_AND_MEAN // 불필요한 Windows 헤더 제외 (선택 사항)
#include <windows.h>
#include <winternl.h>
#include <iostream>

#include "hooks.h"
#include "help_util.h"



// 진단용: 치명적 예외가 나면 주소/모듈/레지스터/스택을 crash.log 에 기록
static LONG CALLBACK CrashLogVEH(PEXCEPTION_POINTERS ep)
{
    DWORD code = ep->ExceptionRecord->ExceptionCode;
    if (code != 0xC0000005 && code != 0xC0000409 && code != 0xC00000FD && code != 0xC0000094)
        return EXCEPTION_CONTINUE_SEARCH;

    static LONG busy = 0;
    if (InterlockedExchange(&busy, 1)) return EXCEPTION_CONTINUE_SEARCH;

    char buf[4096];
    int n = 0;
    auto modOf = [](void* addr, char* out, size_t sz) {
        HMODULE m = nullptr;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)addr, &m) && m) {
            char path[MAX_PATH] = {};
            GetModuleFileNameA(m, path, MAX_PATH);
            const char* base = strrchr(path, '\\');
            wsprintfA(out, "%s+0x%X", base ? base + 1 : path, (unsigned)((BYTE*)addr - (BYTE*)m));
        } else lstrcpyA(out, "?");
    };
    char mod[300];
    modOf(ep->ExceptionRecord->ExceptionAddress, mod, sizeof(mod));
    CONTEXT* c = ep->ContextRecord;
    n += wsprintfA(buf + n, "EXCEPTION %08X at %p (%s) tid=%u info0=%p info1=%p\r\n",
        code, ep->ExceptionRecord->ExceptionAddress, mod, GetCurrentThreadId(),
        (void*)ep->ExceptionRecord->ExceptionInformation[0], (void*)ep->ExceptionRecord->ExceptionInformation[1]);
#ifdef _M_IX86
    n += wsprintfA(buf + n, "EAX=%08X EBX=%08X ECX=%08X EDX=%08X ESI=%08X EDI=%08X EBP=%08X ESP=%08X\r\n",
        c->Eax, c->Ebx, c->Ecx, c->Edx, c->Esi, c->Edi, c->Ebp, c->Esp);
    DWORD* sp = (DWORD*)c->Esp;
    for (int i = 0; i < 64 && n < 3800; i++) {
        DWORD v;
        if (IsBadReadPtr(sp + i, 4)) break;
        v = sp[i];
        char m2[300];
        modOf((void*)(uintptr_t)v, m2, sizeof(m2));
        if (m2[0] != '?') n += wsprintfA(buf + n, "  [esp+%02X] %08X %s\r\n", i * 4, v, m2);
    }
#endif
    HANDLE h = CreateFileA("crash.log", FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD w; WriteFile(h, buf, n, &w, nullptr);
        WriteFile(h, "----\r\n", 6, &w, nullptr);
        CloseHandle(h);
    }
    InterlockedExchange(&busy, 0);
    return EXCEPTION_CONTINUE_SEARCH;
}

void ApplyDDrawPatches(uintptr_t baseAddress);   // sneg_hook.cpp

DWORD WINAPI InitHookThread(LPVOID lpParam)
{
    UNREFERENCED_PARAMETER(lpParam);
    AddVectoredExceptionHandler(1, CrashLogVEH);

#if 1
    plog::init(plog::debug, "error.log"); // Step2: initialize the logger
#else
    static plog::DebugOutputAppender<plog::TxtFormatter> debugOutputAppender;
    plog::init(plog::info, &debugOutputAppender);
#endif
    LoadOriginalDll();
    InstallBinkHooks();
    patchLazyDLL(L"sneg.dll", ApplySNEGPatches);
    patchLazyDLL(L"ddraw.dll", ApplyDDrawPatches);
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