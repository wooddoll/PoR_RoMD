#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct SubtitleEntry {
    uint64_t startTimeMs;
    uint64_t endTimeMs;
    std::wstring text;
};

class SbvParser {
public:
    static bool LoadFromFile(const std::wstring& filePath, std::vector<SubtitleEntry>& outEntries);
    static std::wstring GetCurrentSubtitle(const std::vector<SubtitleEntry>& entries, uint64_t currentMs);
private:
    static uint64_t ParseTimeToMs(const std::string& timeStr);
    static std::wstring Utf8ToWide(const std::string& str);
};