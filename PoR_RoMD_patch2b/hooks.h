#pragma once

void LoadOriginalDll();
void ApplyMemoryPatches();      // 후킹할 함수 선언부
void InstallBinkHooks();
void RemoveBinkHooks();