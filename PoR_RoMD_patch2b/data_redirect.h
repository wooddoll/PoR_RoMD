#pragma once

#include <string>

/* ------------------------------------------------------------------ */
/*  Data_kr redirect                                                    */
/*  por2main.exe 의 IAT(CreateFileA / GetFileAttributesA /             */
/*  FindFirstFileA / FindNextFileA / FindClose)를 바꿔서,               */
/*  <게임폴더>\Data\<경로> 또는 <게임폴더>\<파일> 을 읽을 때            */
/*  <게임폴더>\Data_kr\<경로> 에 같은 파일이 있으면 그 파일을 대신 연다. */
/*  원본 Data 폴더는 건드리지 않는다.                                    */
/* ------------------------------------------------------------------ */

// DllMain(DLL_PROCESS_ATTACH)에서 호출. 게임 코드가 파일을 읽기 전에 설치해야 한다.
// language.ini [Settings] DataPath (기본 "Data_kr"), DataRedirect=0 이면 끔.
bool InstallDataRedirect();

// path(상대/절대)에 대응하는 Data_kr 파일이 있으면 out 에 넣고 true.
bool DataRedirect_Resolve(const char* path, std::string& out);
