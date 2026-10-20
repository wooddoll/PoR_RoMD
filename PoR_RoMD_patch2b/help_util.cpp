#include "help_util.h"

#include <iostream>
#include <filesystem>

#define WIN32_LEAN_AND_MEAN // 불필요한 Windows 헤더 제외 (선택 사항)
#include <windows.h>
#include <shlwapi.h> // PathFindFileNameA 등을 사용하기 위해 포함 (필요시 shlwapi.lib 링크)
#pragma comment(lib, "Shlwapi.lib")

IniConfig g_Config; // 실제 전역 객체 할당

bool LoadAllIniSettings(const std::wstring& filePath)
{
    if (g_Config.isLoaded && g_Config.loadedFilePath == filePath) {
        return true;
    }

    if (filePath.empty()) {
        return false;
    }

    DWORD attr = GetFileAttributesW(filePath.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY)) {
        return false;
    }

    std::map<std::wstring, std::map<std::wstring, std::wstring>> tempData;

    const DWORD BUFFER_SIZE = 8192;
    std::vector<wchar_t> sectionBuffer(BUFFER_SIZE, L'\0');

    DWORD sectionLen = GetPrivateProfileStringW(
        NULL,
        NULL,
        L"",
        sectionBuffer.data(),
        BUFFER_SIZE,
        filePath.c_str()
    );

    if (sectionLen == 0) {
        return false;
    }

    if (sectionLen >= BUFFER_SIZE - 2) {
        return false;
    }

    wchar_t* pSection = sectionBuffer.data();

    while (*pSection != L'\0') {
        std::wstring sectionName(pSection);

        std::vector<wchar_t> keyBuffer(BUFFER_SIZE, L'\0');

        DWORD keyLen = GetPrivateProfileStringW(
            sectionName.c_str(),
            NULL,
            L"",
            keyBuffer.data(),
            BUFFER_SIZE,
            filePath.c_str()
        );

        if (keyLen >= BUFFER_SIZE - 2) {
            return false;
        }

        wchar_t* pKey = keyBuffer.data();

        while (*pKey != L'\0') {
            std::wstring keyName(pKey);

            std::vector<wchar_t> valBuffer(BUFFER_SIZE, L'\0');

            DWORD valueLen = GetPrivateProfileStringW(
                sectionName.c_str(),
                keyName.c_str(),
                L"",
                valBuffer.data(),
                BUFFER_SIZE,
                filePath.c_str()
            );

            if (valueLen >= BUFFER_SIZE - 1) {
                return false;
            }

            std::wstring valueStr(valBuffer.data());

            tempData[sectionName][keyName] = valueStr;

            pKey += keyName.length() + 1;
        }

        pSection += sectionName.length() + 1;
    }

    g_Config.data = std::move(tempData);
    g_Config.loadedFilePath = filePath;
    g_Config.isLoaded = true;

    return true;
}

bool ReloadAllIniSettings(const std::wstring& filePath)
{
    g_Config.isLoaded = false;
    g_Config.loadedFilePath.clear();
    g_Config.data.clear();

    return LoadAllIniSettings(filePath);
}

// 실행 파일(exe)이 있는 폴더 경로 가져오기
std::wstring GetExeDirectory()
{
    wchar_t exePath[MAX_PATH] = { 0 };

    DWORD len = GetModuleFileNameW(NULL, exePath, MAX_PATH);

    if (len == 0 || len >= MAX_PATH) {
        return L"";
    }

    PathRemoveFileSpecW(exePath);

    return std::wstring(exePath);
}

std::wstring CombinePath(const std::wstring& dir, const std::wstring& file) {
    // std::filesystem::path가 /, \, ./, ../ 등을 알아서 올바르게 결합합니다.
    std::filesystem::path pathDir(dir);
    std::filesystem::path pathFile(file);

    return (pathDir / pathFile).wstring();
}


bool LoadSettings(const wchar_t* filePath)
{
    if (filePath == nullptr || filePath[0] == L'\0') {
        return false;
    }

    std::wstring currDir = GetExeDirectory();

    if (currDir.empty()) {
        return false;
    }

    g_Config.currentExePath = currDir;

    std::wstring iniPath = CombinePath(currDir, filePath);

    return LoadAllIniSettings(iniPath);
}

char _eucKrStr_[3];
const char* OutputEucKrChar(unsigned char byte1, unsigned char byte2) 
{
    // 2바이트 데이터와 문자열의 끝을 알리는 널 문자(\0)를 포함한 배열 생성
    _eucKrStr_[0] = static_cast<char>(byte1);
    _eucKrStr_[1] = static_cast<char>(byte2);
    _eucKrStr_[2] = '\0'; // 널 종료 문자 필수

    // OutputDebugStringA는 시스템의 기본 코드 페이지(한국어 윈도우는 CP949/EUC-KR)로 해석합니다.
    return _eucKrStr_;
}

void OutputDebugStringFormat(const char* format, ...)
{
    char str[256];
    va_list args;
    va_start(args, format);
    vsprintf_s(str, sizeof(str), format, args);
    OutputDebugStringA(str);
    va_end(args);
}

//////////////////============================================================
// 32비트 전용: 5바이트 점프 훅 함수 (MinHook 대체)
bool HookFunction(uintptr_t targetAddr, uintptr_t hookFuncAddr)
{
    DWORD oldProtect;
    if (!VirtualProtect(reinterpret_cast<void*>(targetAddr), 5, PAGE_EXECUTE_READWRITE, &oldProtect))
        return false;

    // E9 [상대 변위 4바이트] : JMP hookFuncAddr
    *reinterpret_cast<uint8_t*>(targetAddr) = 0xE9;
    *reinterpret_cast<int32_t*>(targetAddr + 1) = static_cast<int32_t>(hookFuncAddr - (targetAddr + 5));

    VirtualProtect(reinterpret_cast<void*>(targetAddr), 5, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(targetAddr), 5);
    return true;
}

void patchBytes(uintptr_t targetAddr, const void* data, size_t bytes) 
{
    DWORD oldProtect;
    if (VirtualProtect(reinterpret_cast<void*>(targetAddr), bytes, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        memcpy(reinterpret_cast<void*>(targetAddr), data, bytes);
        VirtualProtect(reinterpret_cast<void*>(targetAddr), bytes, oldProtect, &oldProtect);
        // 패치한 메모리 영역의 CPU 캐시를 갱신합니다.
        FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(targetAddr), bytes);
    }
}


#define _LOG_   1
#if _LOG_
#define CHECK(charcode) do { GetCurrentCall(charcode, reinterpret_cast<uintptr_t>(_ReturnAddress())); } while(0)
#else
#define CHECK(charcode) ((void)0)
#endif


void GetCurrentCall(const int charCode, const uintptr_t retAddr)
{
    uint8_t c1 = static_cast<uint8_t>(charCode & 0xFF);        // 현재 바이트
    uint8_t c2 = static_cast<uint8_t>((charCode >> 8) & 0xFF); // 다음 바이트 (4바이트 읽기)


    // 1. WordWrap (0x00444890 ~ 0x00444CAB 부근): ESI가 인덱스
    if (retAddr >= 0x00444890 && retAddr <= 0x00444CAB) {
        //OutputDebugStringFormat("[WordWrap] retAddr=0x%p, charCode=0x%02X,0x%02X, %s\n", (void*)retAddr, c1, c2, OutputEucKrChar(c1, c2));
        PLOG_DEBUG << "[WordWrap] retAddr=0x" << std::hex << retAddr << ", charCode=0x" << std::hex << (int)c1 << ",0x" << std::hex << (int)c2 << ", " << OutputEucKrChar(c1, c2);
    }
    // 2. DrawTextLines (0x0043BD00 ~ 0x0043BF00 부근): EBX가 인덱스
    else if (retAddr >= 0x0043BD00 && retAddr <= 0x0043BF00) {
        //OutputDebugStringFormat("[DrawTextLines] retAddr=0x%p, charCode=0x%02X,0x%02X, %s\n", (void*)retAddr, c1, c2, OutputEucKrChar(c1, c2));
        PLOG_DEBUG << "[DrawTextLines] retAddr=0x" << std::hex << retAddr << ", charCode=0x" << std::hex << (int)c1 << ",0x" << std::hex << (int)c2 << ", " << OutputEucKrChar(c1, c2);
    }
    // 3. CalculateTextExtents: EDI가 인덱스!
    else if (retAddr >= 0x0043BF40 && retAddr <= 0x0043C00F) {
        //OutputDebugStringFormat("[CalculateTextExtents] retAddr=0x%p, charCode=0x%02X,0x%02X, %s\n", (void*)retAddr, c1, c2, OutputEucKrChar(c1, c2));
        PLOG_DEBUG << "[CalculateTextExtents] retAddr=0x" << std::hex << retAddr << ", charCode=0x" << std::hex << (int)c1 << ",0x" << std::hex << (int)c2 << ", " << OutputEucKrChar(c1, c2);
    }
    // 4. RenderShadow: EDI가 인덱스!
    else if (retAddr >= 0x00444d40 && retAddr <= 0x00445039) {
        //OutputDebugStringFormat("[RenderShadow] retAddr=0x%p, charCode=0x%02X,0x%02X, %s\n", (void*)retAddr, c1, c2, OutputEucKrChar(c1, c2));
        PLOG_DEBUG << "[RenderShadow] retAddr=0x" << std::hex << retAddr << ", charCode=0x" << std::hex << (int)c1 << ",0x" << std::hex << (int)c2 << ", " << OutputEucKrChar(c1, c2);
    }
    // 5. unknown: EDI가 인덱스!
    else if (retAddr >= 0x00476820 && retAddr <= 0x00476adc) {
        //OutputDebugStringFormat("[SprintfMeasure] retAddr=0x%p, charCode=0x%02X,0x%02X, %s\n", (void*)retAddr, c1, c2, OutputEucKrChar(c1, c2));
        PLOG_DEBUG << "[SprintfMeasure] retAddr=0x" << std::hex << retAddr << ", charCode=0x" << std::hex << (int)c1 << ",0x" << std::hex << (int)c2 << ", " << OutputEucKrChar(c1, c2);
    }
    else if (retAddr >= 0x00438830 && retAddr <= 0x00438b0f) {
        //OutputDebugStringFormat("[check_FUN_00438830] retAddr=0x%p, charCode=0x%02X,0x%02X, %s\n", (void*)retAddr, c1, c2, OutputEucKrChar(c1, c2));
        PLOG_DEBUG << "[check_FUN_00438830] retAddr=0x" << std::hex << retAddr << ", charCode=0x" << std::hex << (int)c1 << ",0x" << std::hex << (int)c2 << ", " << OutputEucKrChar(c1, c2);
    }
    else {
        //OutputDebugStringFormat("[!!!ELSE!!!] retAddr=0x%p, charCode=0x%02X,0x%02X, %s\n", (void*)retAddr, c1, c2, OutputEucKrChar(c1, c2));
        PLOG_FATAL << "[!!!ELSE!!!] retAddr=0x" << std::hex << retAddr << ", charCode=0x" << std::hex << (int)c1 << ",0x" << std::hex << (int)c2 << ", " << OutputEucKrChar(c1, c2);
    }
}