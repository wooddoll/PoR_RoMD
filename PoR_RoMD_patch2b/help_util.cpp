#include "help_util.h"
#include "por2.h"

#include <iostream>
#include <filesystem>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlwapi.h> // PathFindFileNameA
#pragma comment(lib, "Shlwapi.lib")

void resetLogLevel()
{
    constexpr int default_level = 0;
    int loglevel = default_level;
    std::wstring value = g_Config.GetString(L"Settings", L"LogLevel", L"0");
    if (!value.empty()) {
        for (auto& c : value) {
            c = static_cast<wchar_t>(std::towlower(c));
        }

        if (value == L"0" || value == L"none")      loglevel = 0;
        if (value == L"1" || value == L"fatal")     loglevel = 1;
        if (value == L"2" || value == L"error")     loglevel = 2;
        if (value == L"3" || value == L"warning")   loglevel = 3;
        if (value == L"4" || value == L"info")      loglevel = 4;
        if (value == L"5" || value == L"debug")     loglevel = 5;
        if (value == L"6" || value == L"verbose")   loglevel = 6;
    }

    switch (loglevel) {
    case 0: plog::get()->setMaxSeverity(plog::none);       break;
    case 1: plog::get()->setMaxSeverity(plog::fatal);      break;
    case 2: plog::get()->setMaxSeverity(plog::error);      break;
    case 3: plog::get()->setMaxSeverity(plog::warning);    break;
    case 4: plog::get()->setMaxSeverity(plog::info);       break;
    case 5: plog::get()->setMaxSeverity(plog::debug);      break;
    case 6: plog::get()->setMaxSeverity(plog::verbose);    break;
    default: plog::get()->setMaxSeverity(plog::none);      break;
    }
}

thread_local char _eucKrStr_[3];
const char* OutputEucKrChar(unsigned char byte1, unsigned char byte2) 
{
    _eucKrStr_[0] = static_cast<char>(byte1);
    _eucKrStr_[1] = static_cast<char>(byte2);
    _eucKrStr_[2] = '\0';
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


bool HookFunction(const uintptr_t targetAddr, const uintptr_t hookFuncAddr)
{
    DWORD oldProtect;
    if (!VirtualProtect(reinterpret_cast<void*>(targetAddr), 5, PAGE_EXECUTE_READWRITE, &oldProtect))
        return false;

    *reinterpret_cast<uint8_t*>(targetAddr) = 0xE9;
    *reinterpret_cast<int32_t*>(targetAddr + 1) = static_cast<int32_t>(hookFuncAddr - (targetAddr + 5));

    VirtualProtect(reinterpret_cast<void*>(targetAddr), 5, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(targetAddr), 5);
    return true;
}

void PatchRelativeCall(uintptr_t callSite, const uintptr_t newTarget)
{
    DWORD rel = (DWORD)newTarget - (DWORD)(callSite + 5);
    DWORD old;
    VirtualProtect((LPVOID)(callSite + 1), 4, PAGE_EXECUTE_READWRITE, &old);
    *(DWORD*)(callSite + 1) = rel;
    VirtualProtect((LPVOID)(callSite + 1), 4, old, &old);
    FlushInstructionCache(GetCurrentProcess(), (LPCVOID)callSite, 5);
}



void patchBytes(const uintptr_t targetAddr, const void* data, const size_t bytes) 
{
    DWORD oldProtect;
    if (VirtualProtect(reinterpret_cast<void*>(targetAddr), bytes, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        memcpy(reinterpret_cast<void*>(targetAddr), data, bytes);
        VirtualProtect(reinterpret_cast<void*>(targetAddr), bytes, oldProtect, &oldProtect);
        FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(targetAddr), bytes);
    }
}

void patchDWORD(const uintptr_t targetAddr, const uint32_t dword)
{
    DWORD oldProtect;
    if (VirtualProtect(reinterpret_cast<void*>(targetAddr), 4, PAGE_EXECUTE_READWRITE, &oldProtect)) {
		*reinterpret_cast<uint32_t*>(targetAddr) = dword;
        VirtualProtect(reinterpret_cast<void*>(targetAddr), 4, oldProtect, &oldProtect);
        FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(targetAddr), 4);
    }
}


void GetCurrentCall(const int charCode, const uintptr_t retAddr)
{
    uint8_t c1 = static_cast<uint8_t>(charCode & 0xFF);
    uint8_t c2 = static_cast<uint8_t>((charCode >> 8) & 0xFF);
    const char* extChar = OutputEucKrChar(c1, c2);

    if (retAddr >= ptrWordWrap && retAddr <= ptrWordWrap_end) {
        PLOG_DEBUG << "[WordWrap] retAddr=0x" << std::hex << retAddr << ", charCode=0x" << std::hex << (int)c1 << ",0x" << std::hex << (int)c2 << ", " << extChar;
    }
    else if (retAddr >= ptrRenderShadow && retAddr <= ptrRenderShadow_end) {
        PLOG_DEBUG << "[RenderShadow] retAddr=0x" << std::hex << retAddr << ", charCode=0x" << std::hex << (int)c1 << ",0x" << std::hex << (int)c2 << ", " << extChar;
    }
    else if (retAddr >= ptrCalculateTextExtents && retAddr <= ptrCalculateTextExtents_end) {
        PLOG_DEBUG << "[CalculateTextExtents] retAddr=0x" << std::hex << retAddr << ", charCode=0x" << std::hex << (int)c1 << ",0x" << std::hex << (int)c2 << ", " << extChar;
    }
    else if (retAddr >= ptrDrawTextLines && retAddr <= ptrDrawTextLines_end) {
        PLOG_DEBUG << "[DrawTextLines] retAddr=0x" << std::hex << retAddr << ", charCode=0x" << std::hex << (int)c1 << ",0x" << std::hex << (int)c2 << ", " << extChar;
    }
    else if (retAddr >= ptrSprintfMeasure && retAddr <= ptrSprintfMeasure_end) {
        PLOG_DEBUG << "[SprintfMeasure] retAddr=0x" << std::hex << retAddr << ", charCode=0x" << std::hex << (int)c1 << ",0x" << std::hex << (int)c2 << ", " << extChar;
    }
    else if (retAddr >= ptrcheck_FUN_00438830 && retAddr <= ptrcheck_FUN_00438830_end) {
        PLOG_DEBUG << "[check_FUN_00438830] retAddr=0x" << std::hex << retAddr << ", charCode=0x" << std::hex << (int)c1 << ",0x" << std::hex << (int)c2 << ", " << extChar;
    }
    else {
        PLOG_ERROR << "[!!!ELSE!!!] retAddr=0x" << std::hex << retAddr << ", charCode=0x" << std::hex << (int)c1 << ",0x" << std::hex << (int)c2 << ", " << extChar;
    }
}