#pragma once

void LoadOriginalDll();

void openLogFile();
void closeLogFile();
// 후킹할 함수 선언부
void ApplyMemoryPatches();
