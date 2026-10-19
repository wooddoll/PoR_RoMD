#pragma once

#include <map>
#include <vector>
#include <string>
#include <cwctype>     // std::towlower
#include <stdexcept>   // std::invalid_argument, std::out_of_range


bool LoadSettings(const wchar_t* filePath);

bool LoadAllIniSettings(const std::wstring& filePath);
bool ReloadAllIniSettings(const std::wstring& filePath);

void OutputDebugStringFormat(const char* format, ...);
const char* OutputEucKrChar(unsigned char byte1, unsigned char byte2);

std::wstring GetExeDirectory();
std::wstring CombinePath(const std::wstring& dir, const std::wstring& file);

// 1. INI 전체 데이터를 담을 구조체 정의
struct IniConfig 
{
	bool isLoaded = false;
	std::wstring loadedFilePath;
	std::wstring currentExePath;

    // 구조: [Section][Key] = Value
    std::map<std::wstring, std::map<std::wstring, std::wstring>> data;

	// 편의를 위한 값 가져오기 헬퍼 함수
	// 문자열 값 가져오기
	std::wstring GetString(
        const std::wstring& section,
        const std::wstring& key,
        const std::wstring& default_val = L""
    ) const {
        auto sectionIt = data.find(section);

        if (sectionIt == data.end()) {
            return default_val;
        }

        auto keyIt = sectionIt->second.find(key);

        if (keyIt == sectionIt->second.end()) {
            return default_val;
        }

        return keyIt->second;
    }

	// 정수 값 가져오기
    int GetInt(
        const std::wstring& section,
        const std::wstring& key,
        int default_val = 0
    ) const {
        std::wstring value = GetString(section, key, L"");

        if (value.empty()) {
            return default_val;
        }

        try {
            size_t idx = 0;
            int result = std::stoi(value, &idx);

            // "123abc" 같은 값 방지
            if (idx != value.length()) {
                return default_val;
            }

            return result;
        }
        catch (const std::invalid_argument&) {
            return default_val;
        }
        catch (const std::out_of_range&) {
            return default_val;
        }
    }

	// bool 값 가져오기
    bool GetBool(
        const std::wstring& section,
        const std::wstring& key,
        bool default_val = false
    ) const {
        std::wstring value = GetString(section, key, L"");

        if (value.empty()) {
            return default_val;
        }

        // 소문자로 변환
        for (auto& c : value) {
            c = static_cast<wchar_t>(std::towlower(c));
        }

        if (
            value == L"true" ||
            value == L"1" ||
            value == L"yes" ||
            value == L"on"
        ) {
            return true;
        }

        if (
            value == L"false" ||
            value == L"0" ||
            value == L"no" ||
            value == L"off"
        ) {
            return false;
        }

        return default_val;
    }
};


// 2. 전역 변수 선언
extern IniConfig g_Config;
