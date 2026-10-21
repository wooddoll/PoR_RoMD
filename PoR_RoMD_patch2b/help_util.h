#pragma once

#include <plog/Log.h>
#include <plog/Initializers/RollingFileInitializer.h>

#include <vector>
#include <string>

#include "ini_config.h"

void resetLogLevel();

void OutputDebugStringFormat(const char* format, ...);
const char* OutputEucKrChar(unsigned char byte1, unsigned char byte2);

bool HookFunction(uintptr_t targetAddr, uintptr_t hookFuncAddr);
void patchBytes(uintptr_t targetAddr, const void* data, size_t bytes);
