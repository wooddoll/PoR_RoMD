#pragma once

#include <plog/Log.h>
#include <plog/Initializers/RollingFileInitializer.h>
#include <plog/Appenders/DebugOutputAppender.h>

#include <vector>
#include <string>

void resetLogLevel(const uint32_t loglevel);

void OutputDebugStringFormat(const char* format, ...);
const char* OutputEucKrChar(unsigned char byte1, unsigned char byte2);

bool HookFunction(const uintptr_t targetAddr, const uintptr_t hookFuncAddr);
bool PatchRelativeCall(uintptr_t callSite, const uintptr_t newTarget);
bool patchBytes(const uintptr_t targetAddr, const void* data, const size_t bytes);
bool patchDWORD(const uintptr_t targetAddr, const uint32_t dword);

typedef void (*PatchFunc_t)(uintptr_t baseAddress);
bool patchLazyDLL(const wchar_t* dllName, PatchFunc_t patchFunc);