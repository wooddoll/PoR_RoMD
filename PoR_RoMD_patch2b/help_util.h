#pragma once
#include <map>
#include <vector>
#include <string>

void OutputDebugStringFormat(const char* format, ...);
bool LoadSettings(const wchar_t* filePath);
const char* OutputEucKrChar(unsigned char byte1, unsigned char byte2);

// 1. INI 전체 데이터를 담을 구조체 정의
struct IniConfig {
    // 구조: [Section][Key] = Value
    std::map<std::wstring, std::map<std::wstring, std::wstring>> data;

	// 편의를 위한 값 가져오기 헬퍼 함수
	std::wstring GetString(const std::wstring& section, const std::wstring& key, const std::wstring& default_val = L"") {
		if (data.find(section) != data.end() && data[section].find(key) != data[section].end()) {
			return data[section][key];
		}
		return default_val;
	}

	int GetInt(const std::wstring& section, const std::wstring& key, int default_val = 0) {
		if (data.find(section) != data.end() && data[section].find(key) != data[section].end()) {
			return std::stoi(data[section][key]);
		}
		return default_val;
	}

	bool GetBool(const std::wstring& section, const std::wstring& key, bool default_val = false) {
		if (data.find(section) != data.end() && data[section].find(key) != data[section].end()) {
			std::wstring val = data[section][key];

			// 소문자로 변환하여 비교 (True, TRUE, true 모두 처리)
			for (auto& c : val)
				c = std::tolower(c);

			if (val == L"true" || val == L"1" || val == L"yes" || val == L"on") {
				return true;
			}
			if (val == L"false" || val == L"0" || val == L"no" || val == L"off") {
				return false;
			}
		}
		return default_val;
    }
};


// 2. 전역 변수 선언
extern IniConfig g_Config;
