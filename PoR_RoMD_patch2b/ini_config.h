#pragma once

#include <map>
#include <string>
#include <cwctype>     // std::towlower
#include <stdexcept>   // std::invalid_argument, std::out_of_range

// 경로 유틸리티 함수
std::wstring GetExeDirectory();
std::wstring CombinePath(const std::wstring& dir, const std::wstring& file);

// INI 설정 데이터를 저장하는 구조체
struct IniConfig 
{
	bool isLoaded = false;
	std::wstring loadedFilePath;
	std::wstring currentExePath;

    // 구조: [Section][Key] = Value
    std::map<std::wstring, std::map<std::wstring, std::wstring>> data;

	// 문자열 값 조회 함수
	// 문자열 값이 없으면 기본값 반환
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

	// 정수 값 조회 함수
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

            // "123abc" 같은 값은 오류 처리
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

	// bool 값 조회 함수
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

// 전역 설정 객체
extern IniConfig g_Config;

// INI 파일 로딩 함수
bool LoadSettings(const wchar_t* filePath);
bool LoadAllIniSettings(const std::wstring& filePath);
bool ReloadAllIniSettings(const std::wstring& filePath);