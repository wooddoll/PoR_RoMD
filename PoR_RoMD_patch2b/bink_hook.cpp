#include "bink_hook.h"
#include "sbv_parser.h"
#include "subtitle_renderer.h"
#include "MinHook.h"
#include <shlwapi.h> // PathFindFileNameA 등을 사용하기 위해 포함 (필요시 shlwapi.lib 링크)
#include <filesystem>
#include <map>
#include <string>

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
static std::map<HBINK, std::vector<SubtitleEntry>> g_SubtitlesMap;

// 게임 실행 폴더(exe가 있는 경로) 가져오기
static std::wstring GetGameDirectory() {
    wchar_t exePath[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    PathRemoveFileSpecW(exePath); // exe 파일명 제거하여 폴더 경로만 남김
    return std::wstring(exePath);
}

// 동영상 경로에서 확장자를 뺀 '순수 파일명' 추출
// 예: "movies\\intro.bik" -> "intro"
std::wstring GetBaseFileNameWithoutExt(const char* fullPath) {
    if (!fullPath) return L"";

    // 1) 경로에서 파일명 부분만 추출 (Windows API)
    const char* fileName = PathFindFileNameA(fullPath);

    // 2) ANSI -> WCHAR 변환
    int len = MultiByteToWideChar(CP_ACP, 0, fileName, -1, NULL, 0);
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
std::wstring BuildSubtitlePath(const char* videoFileName) {
    std::wstring gameDir = GetGameDirectory();
    std::wstring baseName = GetBaseFileNameWithoutExt(videoFileName);
    if (baseName.empty()) 
        return L"";

    // [설정] 기본 자막 폴더 이름 (원하는 폴더명으로 변경 가능)
    // 예: "subtitles" 또는 "자막"
    std::wstring subFolder = L"Data_kr\\Bink";

    // 필요하다면 version.ini 파일에서 사용자 지정 경로를 읽어올 수도 있습니다.
    std::wstring iniPath = gameDir + L"\\korean.ini";
    wchar_t customDir[MAX_PATH] = { 0 };
    GetPrivateProfileStringW(L"Settings", L"SubtitlePath", L"", customDir, MAX_PATH, iniPath.c_str());
    if (wcslen(customDir) > 0) {
        subFolder = customDir;
    }

    // 상대 경로 처리: 만약 "subtitles"처럼 상대경로라면 게임 폴더 뒤에 붙임
    std::wstring fullSubPath;
    if (PathIsRelativeW(subFolder.c_str())) {
        fullSubPath = gameDir + L"\\" + subFolder + L"\\" + baseName + L".sbv";
    }
    else {
        fullSubPath = subFolder + L"\\" + baseName + L".sbv";
    }

    return fullSubPath;
}

// 후킹 함수: BinkOpen
static HBINK __stdcall DetourBinkOpen(const char* name, uint32_t flags) {
    HBINK bink = fpBinkOpen(name, flags);
    if (bink && name) {
        // 지정된 폴더에서 동영상이름.sbv 경로 생성
        std::wstring sbvPath = BuildSubtitlePath(name);

        std::vector<SubtitleEntry> entries;
        if (SbvParser::LoadFromFile(sbvPath, entries)) {
            g_SubtitlesMap[bink] = entries;
            // 디버깅용 로그 (필요시 OutputDebugString으로 확인 가능)
            OutputDebugStringW((L"[Subtitle] Loaded: " + sbvPath + L"\n").c_str());
        }
        else {
            OutputDebugStringW((L"[Subtitle] Not found: " + sbvPath + L"\n").c_str());
        }
    }
    return bink;
}

// BINKCOPYALL 플래그 정의
#define BINKCOPYALL 0x80000000L

// 후킹 함수: BinkCopyToBuffer
static int __stdcall DetourBinkCopyToBuffer(
    HBINK bink, void* dest, uint32_t destpitch, uint32_t destheight,
    uint32_t destx, uint32_t desty, uint32_t flags)
{
    // [중요] 변경된 부분만 복사하지 않고 화면 전체를 무조건 새로 덮어쓰도록 강제합니다.
    // 이를 통해 이전 프레임에 그려진 자막 잔상이 완벽하게 지워집니다.
    flags |= BINKCOPYALL;

    // 1. 원본 동영상 프레임 복사
    int result = fpBinkCopyToBuffer(bink, dest, destpitch, destheight, destx, desty, flags);

    // 2. 자막 렌더링
    if (bink && dest) {
        auto it = g_SubtitlesMap.find(bink);
        if (it != g_SubtitlesMap.end() && !it->second.empty()) {
            BINK* binkStruct = (BINK*)bink;

            if (binkStruct->FrameRate > 0) {
                uint64_t currentMs = (uint64_t)((double)binkStruct->FrameNum * 1000.0 *
                    (double)binkStruct->FrameRateDiv / (double)binkStruct->FrameRate);

                std::wstring subText = SbvParser::GetCurrentSubtitle(it->second, currentMs);
                if (!subText.empty()) {
                    // destx, desty, destpitch(int32_t로 캐스팅) 전달
                    SubtitleRenderer::RenderSubtitleToBuffer(
                        dest, (int32_t)destpitch, binkStruct->Width, binkStruct->Height,
                        destx, desty, flags, subText);
                }
            }
        }
    }
    return result;
}

// 후킹 함수: BinkClose
static void __stdcall DetourBinkClose(HBINK bink) {
    if (bink) {
        g_SubtitlesMap.erase(bink);
        SubtitleRenderer::ClearCache(); // 캐시 메모리 정리
    }
    fpBinkClose(bink);
}

// GetProcAddress로 심볼 찾기 (맹글링 여부 대응)
static FARPROC GetBinkProc(HMODULE hMod, const char* name, const char* mangled) {
    FARPROC proc = GetProcAddress(hMod, mangled);
    if (!proc) proc = GetProcAddress(hMod, name);
    return proc;
}

void InstallBinkHooks() {
    HMODULE hBink = GetModuleHandleA("binkw32.dll");
    if (!hBink) {
        return;
    }

    FARPROC pOpen = GetBinkProc(hBink, "BinkOpen", "_BinkOpen@8");
    FARPROC pCopy = GetBinkProc(hBink, "BinkCopyToBuffer", "_BinkCopyToBuffer@28");
    FARPROC pClose = GetBinkProc(hBink, "BinkClose", "_BinkClose@4");

    if (MH_Initialize() == MH_OK) {
        if (pOpen) MH_CreateHook(pOpen, &DetourBinkOpen, (LPVOID*)&fpBinkOpen);
        if (pCopy) MH_CreateHook(pCopy, &DetourBinkCopyToBuffer, (LPVOID*)&fpBinkCopyToBuffer);
        if (pClose) MH_CreateHook(pClose, &DetourBinkClose, (LPVOID*)&fpBinkClose);
        MH_EnableHook(MH_ALL_HOOKS);
    }
}

void RemoveBinkHooks() {
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
}