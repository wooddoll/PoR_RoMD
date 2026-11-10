#pragma once

void LoadOriginalDll();
void InstallBinkHooks(uintptr_t baseAddress = 0);
void RemoveBinkHooks();
void ApplyMemoryPatches(uintptr_t baseAddress = 0);      // 후킹할 함수 선언부
void ApplySNEGPatches(uintptr_t baseAddress);