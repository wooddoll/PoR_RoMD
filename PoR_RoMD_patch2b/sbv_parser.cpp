#include "sbv_parser.h"

#include <fstream>
#include <sstream>

#define WIN32_LEAN_AND_MEAN // ???? Windows ?? ?? (?? ??)
#include <windows.h>

uint64_t SbvParser::ParseTimeToMs(const std::string& timeStr) {
    int h = 0, m = 0, s = 0, ms = 0;
    if (sscanf_s(timeStr.c_str(), "%d:%d:%d.%d", &h, &m, &s, &ms) == 4) {
        return (uint64_t)h * 3600000 + (uint64_t)m * 60000 + (uint64_t)s * 1000 + ms;
    }
    return 0;
}

std::wstring SbvParser::Utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
    std::wstring wstr(sizeNeeded, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstr[0], sizeNeeded);
    return wstr;
}

bool SbvParser::LoadFromFile(const std::wstring& filePath, std::vector<SubtitleEntry>& outEntries) {
    outEntries.clear();
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) return false;

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    // UTF-8 BOM Á¦°Å
    if (content.size() >= 3 && (unsigned char)content[0] == 0xEF &&
        (unsigned char)content[1] == 0xBB && (unsigned char)content[2] == 0xBF) {
        content = content.substr(3);
    }

    std::istringstream stream(content);
    std::string line;
    SubtitleEntry current;
    bool readingText = false;
    std::string textAccumulator;

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();

        size_t commaPos = line.find(',');
        if (commaPos != std::string::npos && line.find(':') != std::string::npos) {
            if (readingText && !textAccumulator.empty()) {
                current.text = Utf8ToWide(textAccumulator);
                outEntries.push_back(current);
                textAccumulator.clear();
            }
            std::string startStr = line.substr(0, commaPos);
            std::string endStr = line.substr(commaPos + 1);
            current.startTimeMs = ParseTimeToMs(startStr);
            current.endTimeMs = ParseTimeToMs(endStr);
            readingText = true;
        }
        else if (readingText) {
            if (line.empty()) {
                if (!textAccumulator.empty()) {
                    current.text = Utf8ToWide(textAccumulator);
                    outEntries.push_back(current);
                    textAccumulator.clear();
                    readingText = false;
                }
            }
            else {
                if (!textAccumulator.empty()) textAccumulator += "\n";
                textAccumulator += line;
            }
        }
    }
    if (readingText && !textAccumulator.empty()) {
        current.text = Utf8ToWide(textAccumulator);
        outEntries.push_back(current);
    }
    return true;
}

std::wstring SbvParser::GetCurrentSubtitle(const std::vector<SubtitleEntry>& entries, uint64_t currentMs) {
    for (const auto& item : entries) {
        if (currentMs >= item.startTimeMs && currentMs <= item.endTimeMs) {
            return item.text;
        }
    }
    return L"";
}