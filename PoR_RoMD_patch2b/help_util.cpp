#include <windows.h>
#include <iostream>
#include <shlwapi.h> // PathFindFileNameA 등을 사용하기 위해 포함 (필요시 shlwapi.lib 링크)
#include <filesystem>
#include "help_util.h"


IniConfig g_Config; // 실제 전역 객체 할당

bool LoadAllIniSettings(const std::wstring& filePath) {
    g_Config.data.clear();

    const DWORD BUFFER_SIZE = 32768; // 32KB 충분한 버퍼 공간
    std::vector<wchar_t> sectionBuffer(BUFFER_SIZE, 0);

    // 1. 모든 섹션 이름 가져오기 (lpAppName에 NULL 전달)
    DWORD sectionLen = GetPrivateProfileStringW(NULL, NULL, L"", sectionBuffer.data(), BUFFER_SIZE, filePath.c_str());
    if (sectionLen == 0) return false;

    // 섹션 버퍼 순회 (\0으로 구분되어 있고 맨 끝은 \0\0으로 끝남)
    wchar_t* pSection = sectionBuffer.data();
    while (*pSection != L'\0') {
        std::wstring sectionName(pSection);
        std::vector<wchar_t> keyBuffer(BUFFER_SIZE, 0);

        // 2. 해당 섹션의 모든 키 이름 가져오기 (lpKeyName에 NULL 전달)
        DWORD keyLen = GetPrivateProfileStringW(sectionName.c_str(), NULL, L"", keyBuffer.data(), BUFFER_SIZE, filePath.c_str());

        wchar_t* pKey = keyBuffer.data();
        while (*pKey != L'\0') {
            std::wstring keyName(pKey);
            std::vector<wchar_t> valBuffer(BUFFER_SIZE, 0);

            // 3. 실제 값(Value) 가져오기
            GetPrivateProfileStringW(sectionName.c_str(), keyName.c_str(), L"", valBuffer.data(), BUFFER_SIZE, filePath.c_str());
            std::wstring valueStr(valBuffer.data());

            // 전역 구조체에 저장
            g_Config.data[sectionName][keyName] = valueStr;

            pKey += keyName.length() + 1; // 다음 키로 이동
        }

        pSection += sectionName.length() + 1; // 다음 섹션으로 이동
    }

    return true;
}

// 게임 실행 폴더(exe가 있는 경로) 가져오기
static std::wstring GetGameDirectory() 
{
    wchar_t exePath[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    PathRemoveFileSpecW(exePath); // exe 파일명 제거하여 폴더 경로만 남김
    return std::wstring(exePath);
}

bool LoadSettings(const wchar_t* filePath) 
{
    std::wstring gameDir = GetGameDirectory();
    std::wstring iniPath = gameDir + filePath;

    return LoadAllIniSettings(iniPath);
}

char _eucKrStr_[3];
const char* OutputEucKrChar(unsigned char byte1, unsigned char byte2) {
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