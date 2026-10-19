#include "bink_hook.h"
#include "sbv_parser.h"
#include "subtitle_renderer.h"
#include "MinHook.h"
#include <shlwapi.h> // PathFindFileNameA 등을 사용하기 위해 포함 (필요시 shlwapi.lib 링크)
#pragma comment(lib, "Shlwapi.lib")

#include "help_util.h"

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
        {
            std::lock_guard<std::mutex> lock(g_SubtitleMutex);
            g_SubtitlesMap.erase(bink);
        }

        SubtitleRenderer::ClearCache(); // 캐시 메모리 정리
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

    // 지정된 폴더에서 동영상이름.sbv 경로 생성
    std::wstring sbvPath = BuildSubtitlePath(name);
    if (sbvPath.empty()) {
        return bink;
    }
    std::vector<SubtitleEntry> entries;
    if (SbvParser::LoadFromFile(sbvPath, entries)) 
    {
        {
            std::lock_guard<std::mutex> lock(g_SubtitleMutex);
            g_SubtitlesMap[bink] = std::move(entries);
        }
        // 디버깅용 로그 (필요시 OutputDebugString으로 확인 가능)
        OutputDebugStringW((L"[Subtitle] Loaded: " + sbvPath + L"\n").c_str());
    }
    else {
        OutputDebugStringW((L"[Subtitle] Not found: " + sbvPath + L"\n").c_str());
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
        OutputDebugStringW(L"[binkw32.dll] module not loaded yet.\n");
        return;
    }

    FARPROC pOpen = GetBinkProc(hBinkOriginalDll, "BinkOpen", "_BinkOpen@8");
    FARPROC pCopy = GetBinkProc(hBinkOriginalDll, "BinkCopyToBuffer", "_BinkCopyToBuffer@28");
    FARPROC pClose = GetBinkProc(hBinkOriginalDll, "BinkClose", "_BinkClose@4");

    MH_STATUS initStatus = MH_Initialize();
    if (initStatus != MH_OK && initStatus != MH_ERROR_ALREADY_INITIALIZED) {
        OutputDebugStringW(L"[binkw32.dll] MH_Initialize() fail.\n");
        return;
    }


    if (pOpen) {
        MH_STATUS st = MH_CreateHook(pOpen, &DetourBinkOpen, (LPVOID*)&fpBinkOpen);
        if (st != MH_OK) {
            OutputDebugStringA("[binkw32.dll] BinkOpen hook failed.\n");
        }
        if (pOpen) {
            MH_EnableHook(pOpen);
        }
    }
    
    if (pCopy) {
        MH_STATUS st = MH_CreateHook(pCopy, &DetourBinkCopyToBuffer, (LPVOID*)&fpBinkCopyToBuffer);
        if (st != MH_OK) {
            OutputDebugStringA("[binkw32.dll] BinkCopyToBuffer hook failed.\n");
        }
        if (pCopy) {
            MH_EnableHook(pCopy);
        }
    }
    
    if (pClose) {
        MH_STATUS st = MH_CreateHook(pClose, &DetourBinkClose, (LPVOID*)&fpBinkClose);
        if (st != MH_OK) {
            OutputDebugStringA("[binkw32.dll] BinkClose hook failed.\n");
        }
        if (pClose) {
            MH_EnableHook(pClose);
        }
    }

    g_BinkHookInitialized = true;

    OutputDebugStringW(L"[binkw32.dll] function hooked.\n");
}

void RemoveBinkHooks() 
{
    if(!g_BinkHookInitialized)
        return;

    MH_DisableHook(pClose);
    MH_DisableHook(pCopy);
    MH_DisableHook(pOpen);

    fpBinkOpen = nullptr;
    fpBinkCopyToBuffer = nullptr;
    fpBinkClose = nullptr;

    {
        std::lock_guard<std::mutex> subLock(g_SubtitleMutex);
        g_SubtitlesMap.clear();
    }

    SubtitleRenderer::ClearCache();

    g_BinkHookInitialized = false;
}
