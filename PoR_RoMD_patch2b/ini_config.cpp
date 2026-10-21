#include "ini_config.h"

#include <vector>
#include <filesystem>

#define WIN32_LEAN_AND_MEAN // Windows 헤더 최소화 (컴파일 속도)
#include <windows.h>
#include <shlwapi.h> // PathRemoveFileSpecW 함수 사용을 위한 헤더 (필요시 shlwapi.lib 링크)
#pragma comment(lib, "Shlwapi.lib")

IniConfig g_Config; // 전역 설정 객체 할당

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

// 실행 파일(exe)이 있는 폴더 경로 반환
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