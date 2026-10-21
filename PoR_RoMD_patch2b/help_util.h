#pragma once

#include <plog/Log.h>
#include <plog/Initializers/RollingFileInitializer.h>

#include <vector>
#include <string>

#include "ini_config.h"

void resetLogLevel();

void OutputDebugStringFormat(const char* format, ...);
const char* OutputEucKrChar(unsigned char byte1, unsigned char byte2);

bool HookFunction(const uintptr_t targetAddr, const uintptr_t hookFuncAddr);
void patchBytes(const uintptr_t targetAddr, const void* data, const size_t bytes);
void patchDWORD(const uintptr_t targetAddr, const uint32_t dword);
