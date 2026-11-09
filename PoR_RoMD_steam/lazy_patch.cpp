#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winternl.h>
#include <atomic>
#include <string>
#include <cstdint>

// 패치 함수 포인터 타입
typedef void (*PatchFunc_t)(uintptr_t baseAddress);

typedef struct _LDR_DLL_LOADED_NOTIFICATION_DATA {
    ULONG Flags;
    PCUNICODE_STRING FullDllName;
    PCUNICODE_STRING BaseDllName;
    PVOID DllBase;
    ULONG SizeOfImage;
} LDR_DLL_LOADED_NOTIFICATION_DATA, * PLDR_DLL_LOADED_NOTIFICATION_DATA;

typedef union _LDR_DLL_NOTIFICATION_DATA {
    LDR_DLL_LOADED_NOTIFICATION_DATA Loaded;
} LDR_DLL_NOTIFICATION_DATA, * PLDR_DLL_NOTIFICATION_DATA;

typedef VOID(CALLBACK* PLDR_DLL_NOTIFICATION_FUNCTION)(
    ULONG NotificationReason,
    PLDR_DLL_NOTIFICATION_DATA NotificationData,
    PVOID Context
    );

typedef NTSTATUS(NTAPI* _LdrRegisterDllNotification)(
    ULONG Flags,
    PLDR_DLL_NOTIFICATION_FUNCTION NotificationFunction,
    PVOID Context,
    PVOID* Cookie
    );

typedef NTSTATUS(NTAPI* _LdrUnregisterDllNotification)(
    PVOID Cookie
    );

// DLL별로 독립적인 상태를 유지하는 컨텍스트 구조체
struct PatchContext {
    std::wstring targetDllName;
    PatchFunc_t patchFunc;
    std::atomic<PVOID> cookie{ nullptr };
    std::atomic<bool> isPatched{ false };

    PatchContext(const wchar_t* dllName, PatchFunc_t func)
        : targetDllName(dllName), patchFunc(func) {
    }
};

// 안전한 유니코드 문자열 비교 헬퍼
static bool IsTargetDll(PCUNICODE_STRING dllName, const std::wstring& targetName) {
    if (!dllName || !dllName->Buffer) return false;

    // UNICODE_STRING의 Length는 바이트 단위
    size_t dllCharCount = dllName->Length / sizeof(wchar_t);
    if (dllCharCount != targetName.length()) return false;

    return (_wcsnicmp(dllName->Buffer, targetName.c_str(), dllCharCount) == 0);
}

// 안전한 비동기 언레지스터 및 메모리 정리 스레드
static DWORD WINAPI CleanupThread(LPVOID param) {
    PatchContext* ctx = reinterpret_cast<PatchContext*>(param);
    if (!ctx) return 0;

    // 등록 직후 초미세 레이스로 cookie 기록이 늦어질 경우를 대비해 잠깐 대기
    for (int i = 0; i < 50 && ctx->cookie.load() == nullptr; ++i) {
        Sleep(10);
    }

    PVOID cookie = ctx->cookie.load();
    if (cookie) {
        HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
        if (hNtdll) {
            auto pLdrUnregister = (_LdrUnregisterDllNotification)GetProcAddress(hNtdll, "LdrUnregisterDllNotification");
            if (pLdrUnregister) {
                pLdrUnregister(cookie);
            }
        }
    }

    delete ctx;
    return 0;
}

// 패치 실행 및 언레지스터 트리거
static void ExecutePatch(PatchContext* ctx, uintptr_t baseAddress) {
    // 중복 실행 방지 (최초 1회만 true 반환)
    bool expected = false;
    if (!ctx->isPatched.compare_exchange_strong(expected, true)) {
        return;
    }

    // 1. 유저 정의 패치 함수 호출
    if (ctx->patchFunc) {
        ctx->patchFunc(baseAddress);
    }

    // 2. 콜백 내부(Loader Lock 소유 중)에서 바로 해제하면 데드락 위험이 있으므로
    //    별도 스레드로 위임하여 LdrUnregisterDllNotification 및 컨텍스트 메모리 정리
    HANDLE hThread = CreateThread(nullptr, 0, CleanupThread, ctx, 0, nullptr);
    if (hThread) {
        CloseHandle(hThread);
    }
}

// DLL 로드 알림 콜백
static VOID CALLBACK DllNotifyCallback(
    ULONG NotificationReason,
    PLDR_DLL_NOTIFICATION_DATA NotificationData,
    PVOID Context
) {
    // 1 = LDR_DLL_NOTIFICATION_REASON_LOADED
    if (NotificationReason == 1 && Context) {
        PatchContext* ctx = reinterpret_cast<PatchContext*>(Context);

        if (IsTargetDll(NotificationData->Loaded.BaseDllName, ctx->targetDllName)) {
            uintptr_t baseAddress = reinterpret_cast<uintptr_t>(NotificationData->Loaded.DllBase);
            ExecutePatch(ctx, baseAddress);
        }
    }
}

// 범용 지연 패치 등록 함수
bool patchLazyDLL(const wchar_t* dllName, PatchFunc_t patchFunc) {
    if (!dllName || !patchFunc) return false;

    // 1. 이미 모듈이 로드되어 있는지 먼저 검사
    HMODULE hModule = GetModuleHandleW(dllName);
    if (hModule != NULL) {
        // 이미 로드되어 있다면 등록/컨텍스트 할당 없이 즉시 패치 후 종료
        patchFunc(reinterpret_cast<uintptr_t>(hModule));
        return true;
    }

    // 2. 힙에 독립적인 컨텍스트 생성 (패치 완료 후 내부에서 자동 delete)
    PatchContext* ctx = new PatchContext(dllName, patchFunc);

    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    if (!hNtdll) {
        delete ctx;
        return false;
    }

    auto pLdrRegister = (_LdrRegisterDllNotification)GetProcAddress(hNtdll, "LdrRegisterDllNotification");
    if (!pLdrRegister) {
        delete ctx;
        return false;
    }

    PVOID localCookie = nullptr;
    NTSTATUS status = pLdrRegister(0, DllNotifyCallback, ctx, &localCookie);
    if (status != 0) { // 실패 시 정리
        delete ctx;
        return false;
    }

    // 쿠키를 저장하여 비동기 해제 스레드가 참조할 수 있도록 함
    ctx->cookie.store(localCookie);

    // 3. 등록 전후 타이밍에 로드되었을 가능성 방지 (TOCTOU 방지 더블 체크)
    HMODULE hModuleDoubleCheck = GetModuleHandleW(dllName);
    if (hModuleDoubleCheck != NULL) {
        ExecutePatch(ctx, reinterpret_cast<uintptr_t>(hModuleDoubleCheck));
    }

    return true;
}