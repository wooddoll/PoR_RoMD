#pragma once

void LoadOriginalDll();
void ApplyMemoryPatches(uintptr_t baseAddress);      // 후킹할 함수 선언부
void InstallBinkHooks();
void RemoveBinkHooks();