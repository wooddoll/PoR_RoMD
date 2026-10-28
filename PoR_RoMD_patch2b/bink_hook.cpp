#include "bink_hook.h"
#include "sbv_parser.h"
#include "subtitle_renderer.h"
#include "help_util.h"

#include <MinHook.h>
#include <shlwapi.h> // PathFindFileNameA 등을 사용하기 위해 포함 (필요시 shlwapi.lib 링크)
#pragma comment(lib, "Shlwapi.lib")
#include <pathcch.h>
#pragma comment(lib, "pathcch.lib")

#include <filesystem>
#include <map>
#include <string>
#include <mutex>

// Bink 1.x 구조체 핵심 필드 매핑
struct BINK {
    uint32_t Width;
    uint32_t Height;
    uint32_t Frames;
    uint32_t FrameNum;
    uint32_t LastFrameNum;
    uint32_t FrameRate;
    uint32_t FrameRateDiv;
};
typedef void* HBINK;

// Bink 원본 함수 타입 정의 (__stdcall)
typedef HBINK(__stdcall* fnBinkOpen)(const char* name, uint32_t flags);
typedef int(__stdcall* fnBinkCopyToBuffer)(HBINK bink, void* dest, uint32_t destpitch, uint32_t destheight, uint32_t destx, uint32_t desty, uint32_t flags);
typedef void(__stdcall* fnBinkClose)(HBINK bink);

static fnBinkOpen fpBinkOpen = nullptr;
static fnBinkCopyToBuffer fpBinkCopyToBuffer = nullptr;
static fnBinkClose fpBinkClose = nullptr;

// 동영상 핸들별 자막 목록 매핑
static std::mutex g_SubtitleMutex;
static std::map<HBINK, std::vector<SubtitleEntry>> g_SubtitlesMap;

// 동영상 경로에서 확장자를 뺀 '순수 파일명' 추출
// 예: "movies\\intro.bik" -> "intro"
std::wstring GetBaseFileNameWithoutExt(const char* fullPath) 
{
    if (!fullPath) 
        return L"";

    // 1) 경로에서 파일명 부분만 추출 (Windows API)
    const char* fileName = PathFindFileNameA(fullPath);

    // 2) ANSI -> WCHAR 변환
    int len = MultiByteToWideChar(CP_ACP, 0, fileName, -1, NULL, 0);
    if (len <= 0) {
        return L"";
    }

    std::wstring wFileName(len, 0);
    MultiByteToWideChar(CP_ACP, 0, fileName, -1, &wFileName[0], len);
    if (!wFileName.empty() && wFileName.back() == L'\0') {
        wFileName.pop_back();
    }

    // 3) 확장자(.bik 등) 제거
    size_t lastDot = wFileName.find_last_of(L'.');
    if (lastDot != std::wstring::npos) {
        wFileName = wFileName.substr(0, lastDot);
    }

    return wFileName;
}

// 3. 최종 자막 파일 경로 생성
std::wstring BuildSubtitlePath(const char* videoFileName) 
{
    //std::wstring gameDir = GetGameDirectory();
    std::wstring baseName = GetBaseFileNameWithoutExt(videoFileName);
    if (baseName.empty()) 
        return L"";

    // [설정] 기본 자막 폴더 이름 (원하는 폴더명으로 변경 가능)
    // 예: "subtitles" 또는 "자막"
    std::wstring subFolder = g_Config.GetString(L"Settings", L"SubtitlePath", L"Data_kr\\Bink");
    
    // 상대 경로 처리: 만약 "subtitles"처럼 상대경로라면 게임 폴더 뒤에 붙임
    std::wstring fullSubPath;
    if (PathIsRelativeW(subFolder.c_str())) {
        fullSubPath = CombinePath(g_Config.currentExePath, subFolder);
    }
    else {
        fullSubPath = subFolder;
    }

    return CombinePath(fullSubPath, baseName + L".sbv");
}

std::wstring GetSubPath(const char* videoFileName) {
    if (videoFileName == nullptr || videoFileName[0] == '\0') {
        return L"";
    }

    // 1. const char* (ANSI) -> std::wstring (Wide) 변환 처리
    // 필요한 버퍼 크기를 먼저 구합니다.
    int wideLen = MultiByteToWideChar(CP_ACP, 0, videoFileName, -1, nullptr, 0);
    if (wideLen <= 0) 
        return L"";

    std::wstring wPath(wideLen - 1, L'\0'); // \0 제외한 크기로 초기화
    MultiByteToWideChar(CP_ACP, 0, videoFileName, -1, &wPath[0], wideLen);

    // 2. Win32 API를 사용해 확장자 제거
    // 버퍼의 실제 크기인 wPath.length() + 1을 인자로 넘깁니다.
    HRESULT hr = PathCchRemoveExtension(&wPath[0], wPath.length() + 1);

    if (SUCCEEDED(hr)) {
        // API가 문자열을 \0으로 잘라냈으므로 실제 길이에 맞게 갱신합니다.
        wPath.resize(wcslen(wPath.c_str()));
    }

    return wPath;
}

// BINKCOPYALL 플래그 정의
#define BINKCOPYALL 0x80000000L

// 후킹 함수: BinkCopyToBuffer
static int __stdcall DetourBinkCopyToBuffer(
    HBINK bink, void* dest, uint32_t destpitch, uint32_t destheight,
    uint32_t destx, uint32_t desty, uint32_t flags)
{
    if (!fpBinkCopyToBuffer) {
        return 0;
    }

    // [중요] 변경된 부분만 복사하지 않고 화면 전체를 무조건 새로 덮어쓰도록 강제합니다.
    // 이를 통해 이전 프레임에 그려진 자막 잔상이 완벽하게 지워집니다.
    flags |= BINKCOPYALL;

    // 1. 원본 동영상 프레임 복사
    int result = fpBinkCopyToBuffer(bink, dest, destpitch, destheight, destx, desty, flags);

    // 2. 자막 렌더링
    if (bink && dest && 0 <= result)
    {
        BINK* binkStruct = nullptr;
        std::wstring subText;
        {
            std::lock_guard<std::mutex> lock(g_SubtitleMutex);

            auto it = g_SubtitlesMap.find(bink);
            if (it != g_SubtitlesMap.end() && !it->second.empty()) {
                binkStruct = (BINK*)bink;

                if (binkStruct->FrameRate > 0) {
                    uint64_t currentMs = (uint64_t)((double)binkStruct->FrameNum * 1000.0 *
                        (double)binkStruct->FrameRateDiv / (double)binkStruct->FrameRate);

                    subText = SbvParser::GetCurrentSubtitle(it->second, currentMs);
                }
            }
        }
        if (!subText.empty()) {
            // destx, desty, destpitch(int32_t로 캐스팅) 전달
            SubtitleRenderer::RenderSubtitleToBuffer(
                dest, (int32_t)destpitch, binkStruct->Width, binkStruct->Height,
                destx, desty, flags, subText);
        }
    }
    return result;
}

// 후킹 함수: BinkClose
static void __stdcall DetourBinkClose(HBINK bink)
{
    if (bink)
    {
        bool noMoreSubtitles = false;
        {
            std::lock_guard<std::mutex> lock(g_SubtitleMutex);
            g_SubtitlesMap.erase(bink);
            noMoreSubtitles = g_SubtitlesMap.empty();
        }

        if (noMoreSubtitles)
            SubtitleRenderer::ClearCache();
    }

    if (fpBinkClose) {
        fpBinkClose(bink);
    }
}

// GetProcAddress로 심볼 찾기 (맹글링 여부 대응)
static FARPROC GetBinkProc(HMODULE hMod, const char* name, const char* mangled) 
{
    FARPROC proc = GetProcAddress(hMod, mangled);

    if (!proc) 
        proc = GetProcAddress(hMod, name);

    return proc;
}


// 후킹 함수: BinkOpen
static HBINK __stdcall DetourBinkOpen(const char* name, uint32_t flags)
{
    if (!fpBinkOpen) {
        return nullptr;
    }

    HBINK bink = fpBinkOpen(name, flags);
    if (!bink || !name) {
        return bink;
    }

    {
        std::lock_guard<std::mutex> lock(g_SubtitleMutex);
        g_SubtitlesMap.erase(bink);
    }

    //std::wstring sbvPath = BuildSubtitlePath(name);
    std::wstring sbvPath = GetSubPath(name);

    if (sbvPath.empty()) {
        return bink;
    }

    std::vector<SubtitleEntry> entries;
    if (SbvParser::LoadFromFile(sbvPath, entries))
    {
        // [추가] 자막 텍스트를 모아서 백그라운드 선렌더링 요청
        std::vector<std::wstring> texts;
        texts.reserve(entries.size());
        for (const auto& e : entries) {
            if (!e.text.empty())            // <- SubtitleEntry의 텍스트 필드명에 맞게 수정
                texts.push_back(e.text);
        }

        {
            std::lock_guard<std::mutex> lock(g_SubtitleMutex);
            g_SubtitlesMap[bink] = std::move(entries);
        }

        // [추가] BINK 구조체의 Width/Height는 렌더링 때 쓰는 값과 동일해야 캐시가 맞습니다
        const BINK* b = (const BINK*)bink;
        SubtitleRenderer::Prefetch(texts, b->Width, b->Height);

        PLOG_DEBUG << "[Subtitle] Loaded: " << sbvPath << " (" << texts.size() << " lines prefetching)";
    }
    else {
        PLOG_ERROR << "[Subtitle] Not found: " << sbvPath;
    }

    return bink;
}

static HMODULE hBinkOriginalDll = nullptr;
static bool g_BinkHookInitialized = false;

void InstallBinkHooks() 
{
    if (g_BinkHookInitialized) {
        return;
    }

    hBinkOriginalDll = GetModuleHandleA("binkw32.dll");
    if (!hBinkOriginalDll) {
        //OutputDebugStringA("[binkw32.dll] module not loaded yet.\n");
        PLOG_ERROR << "[binkw32.dll] module not loaded yet.";
        return;
    }

    FARPROC pOpen = GetBinkProc(hBinkOriginalDll, "BinkOpen", "_BinkOpen@8");
    FARPROC pCopy = GetBinkProc(hBinkOriginalDll, "BinkCopyToBuffer", "_BinkCopyToBuffer@28");
    FARPROC pClose = GetBinkProc(hBinkOriginalDll, "BinkClose", "_BinkClose@4");

    MH_STATUS initStatus = MH_Initialize();
    if (initStatus != MH_OK && initStatus != MH_ERROR_ALREADY_INITIALIZED) {
        //OutputDebugStringA("[binkw32.dll] MH_Initialize() fail.\n");
        PLOG_ERROR << "[binkw32.dll] MH_Initialize() fail.";
        return;
    }


    if (pOpen) {
        MH_STATUS st = MH_CreateHook(pOpen, &DetourBinkOpen, (LPVOID*)&fpBinkOpen);
        if (st != MH_OK) {
            //OutputDebugStringA("[binkw32.dll] BinkOpen hook failed.\n");
            PLOG_ERROR << "[binkw32.dll] BinkOpen hook failed.";
        }
        if (pOpen) {
            MH_EnableHook(pOpen);
        }
    }
    
    if (pCopy) {
        MH_STATUS st = MH_CreateHook(pCopy, &DetourBinkCopyToBuffer, (LPVOID*)&fpBinkCopyToBuffer);
        if (st != MH_OK) {
            //OutputDebugStringA("[binkw32.dll] BinkCopyToBuffer hook failed.\n");
            PLOG_ERROR << "[binkw32.dll] BinkCopyToBuffer hook failed.";
        }
        if (pCopy) {
            MH_EnableHook(pCopy);
        }
    }
    
    if (pClose) {
        MH_STATUS st = MH_CreateHook(pClose, &DetourBinkClose, (LPVOID*)&fpBinkClose);
        if (st != MH_OK) {
            //OutputDebugStringA("[binkw32.dll] BinkClose hook failed.\n");
            PLOG_ERROR << "[binkw32.dll] BinkClose hook failed.";
        }
        if (pClose) {
            MH_EnableHook(pClose);
        }
    }

    g_BinkHookInitialized = true;

    //OutputDebugStringA("[binkw32.dll] function hooked.\n");
    PLOG_DEBUG << "[binkw32.dll] function hooked.";
}

void RemoveBinkHooks()
{
    if (!g_BinkHookInitialized)
        return;

    MH_DisableHook(MH_ALL_HOOKS);

    fpBinkOpen = nullptr;
    fpBinkCopyToBuffer = nullptr;
    fpBinkClose = nullptr;

    {
        std::lock_guard<std::mutex> subLock(g_SubtitleMutex);
        g_SubtitlesMap.clear();
    }

    SubtitleRenderer::Shutdown();   // [변경] 워커 스레드 종료 + 캐시 정리
    MH_Uninitialize();

    g_BinkHookInitialized = false;
}
