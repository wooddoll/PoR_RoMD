#include "por2.h"

#include <shlwapi.h> // PathFindFileNameA 등을 사용하기 위해 포함 (필요시 shlwapi.lib 링크)
#pragma comment(lib, "Shlwapi.lib")

#include <MinHook.h>
#include "help_util.h"

RECT g_dummyZeroRect = { 0, 0, 0, 0 };

BOOL __fastcall cTextLayout_WordWrap_Hook(cTextLayout* self, void* /*edx*/);
BOOL __fastcall cTextLayout_RenderShadow_Hook(cTextLayout* self, void* /*edx*/);
BOOL __fastcall cTextManager_CalculateTextExtents_Hook(cTextManager* self, void* /*edx*/);
BOOL __fastcall cTextManager_DrawTextLines_Hook(cTextManager* self, void* /*edx*/, int /*unusedStackArg*/);

BOOL __fastcall CKeyInput_Draw_Hook(CKeyInput* self, void* /*edx*/);
int __cdecl Input_Hook(int /*a1*/, int /*a2*/, int ch);

typedef BOOL (__fastcall* tWordWrap)(cTextLayout*, void*);
tWordWrap g_origWordWrap = NULL;

typedef BOOL (__fastcall* tRenderShadow)(cTextLayout*, void*);
tRenderShadow g_origRenderShadow = NULL;

typedef BOOL (__fastcall* tCalcExtents)(cTextManager*, void*);
tCalcExtents g_origCalcExtents = NULL;

typedef BOOL (__fastcall* tDrawTextLines)(cTextManager*, void*, int);
tDrawTextLines g_origDrawTextLines = NULL;

typedef BOOL (__fastcall* tCKeyInput_Draw)(CKeyInput*, void*);
tCKeyInput_Draw g_origCKeyInput_Draw = NULL;

#define USING_MINHOOK   0
//------- for debug
#if USING_MINHOOK
BOOL __fastcall _cTextLayout_WordWrap_Hook(cTextLayout* self, void* /*edx*/) {
    if(g_origWordWrap == NULL) {
        PLOG_ERROR << "[PoR2] g_origWordWrap is NULL!";
        return FALSE;
    }
    //BOOL ret = g_origWordWrap(self, NULL);
    BOOL ret = cTextLayout_WordWrap_Hook(self, NULL);
    return ret;
}
BOOL __fastcall _cTextLayout_RenderShadow_Hook(cTextLayout* self, void* /*edx*/) {
    if(g_origRenderShadow == NULL) {
        PLOG_ERROR << "[PoR2] g_origRenderShadow is NULL!";
        return FALSE;
    }
    //BOOL ret = g_origRenderShadow(self, NULL);
    BOOL ret = cTextLayout_RenderShadow_Hook(self, NULL);
    return ret;
}
BOOL __fastcall _cTextManager_CalculateTextExtents_Hook(cTextManager* self, void* /*edx*/) {
    if(g_origCalcExtents == NULL) {
        PLOG_ERROR << "[PoR2] g_origCalcExtents is NULL!";
        return FALSE;
    }
    //BOOL ret = g_origCalcExtents(self, NULL);
    BOOL ret = cTextManager_CalculateTextExtents_Hook(self, NULL);
    return ret;
}
BOOL __fastcall _cTextBox_DrawTextLines_Hook(cTextManager* self, void* /*edx*/, int /*unusedStackArg*/) {
    if(g_origDrawTextLines == NULL) {
        PLOG_ERROR << "[PoR2] g_origDrawTextLines is NULL!";
        return FALSE;
    }
    //BOOL ret = g_origDrawTextLines(self, NULL, /*unusedStackArg*/0);
    BOOL ret = cTextManager_DrawTextLines_Hook(self, NULL, /*unusedStackArg*/0);
    return ret;
}
#endif
//-------

void Apply_Por2_Hooks()
{
#if USING_MINHOOK
    MH_STATUS initStatus = MH_Initialize();
    if (initStatus != MH_OK && initStatus != MH_ERROR_ALREADY_INITIALIZED) {
        PLOG_ERROR << "[PoR2] MH_Initialize() fail.";
        return;
    }

    MH_STATUS st = MH_CreateHook((LPVOID)ptrWordWrap, &_cTextLayout_WordWrap_Hook, (LPVOID*)&g_origWordWrap);
    if (st == MH_OK) {
        MH_EnableHook((LPVOID)ptrWordWrap);
    } else {
        PLOG_ERROR << "[PoR2] cTextLayout_WordWrap_Hook hook failed.";
    }

    MH_CreateHook((LPVOID)ptrRenderShadow, &_cTextLayout_RenderShadow_Hook, (LPVOID*)&g_origRenderShadow);
    if (st == MH_OK) {
        MH_EnableHook((LPVOID)ptrRenderShadow);
    } else {
        PLOG_ERROR << "[PoR2] cTextLayout_RenderShadow_Hook hook failed.";
    }
    
    MH_CreateHook((LPVOID)ptrCalculateTextExtents, &_cTextManager_CalculateTextExtents_Hook, (LPVOID*)&g_origCalcExtents);
    if (st == MH_OK) {
        MH_EnableHook((LPVOID)ptrCalculateTextExtents);
    } else {
        PLOG_ERROR << "[PoR2] cTextManager_CalculateTextExtents_Hook hook failed.";
    }
    
    MH_CreateHook((LPVOID)ptrDrawTextLines, &_cTextBox_DrawTextLines_Hook, (LPVOID*)&g_origDrawTextLines);
    if (st == MH_OK) {
        MH_EnableHook((LPVOID)ptrDrawTextLines);    
    } else {
        PLOG_ERROR << "[PoR2] cTextManager_DrawTextLines_Hook hook failed.";
    }

    PLOG_DEBUG << "[PoR2] function hooked.";
#else

#if 0
    uint8_t testWordWrap[128];
	memcpy(testWordWrap, reinterpret_cast<void*>(ptrWordWrap), sizeof(testWordWrap));
    uint8_t testRenderShadow[128];
    memcpy(testRenderShadow, reinterpret_cast<void*>(ptrRenderShadow), sizeof(testRenderShadow));
    uint8_t testCalculateTextExtents[128];
    memcpy(testCalculateTextExtents, reinterpret_cast<void*>(ptrCalculateTextExtents), sizeof(testCalculateTextExtents));
    uint8_t testDrawTextLines[128];
    memcpy(testDrawTextLines, reinterpret_cast<void*>(ptrDrawTextLines), sizeof(testDrawTextLines));
    FILE* fp = nullptr;
    fopen_s(&fp, "hook_test.bin", "wb");
    if (fp) {
        fwrite(testWordWrap, 1, sizeof(testWordWrap), fp);
        fwrite(testRenderShadow, 1, sizeof(testRenderShadow), fp);
        fwrite(testCalculateTextExtents, 1, sizeof(testCalculateTextExtents), fp);
        fwrite(testDrawTextLines, 1, sizeof(testDrawTextLines), fp);
        fclose(fp);
	}
#endif
    HookFunction(ptrWordWrap, reinterpret_cast<uintptr_t>(cTextLayout_WordWrap_Hook));
    HookFunction(ptrRenderShadow, reinterpret_cast<uintptr_t>(cTextLayout_RenderShadow_Hook));
    HookFunction(ptrCalculateTextExtents, reinterpret_cast<uintptr_t>(cTextManager_CalculateTextExtents_Hook));
    HookFunction(ptrDrawTextLines, reinterpret_cast<uintptr_t>(cTextManager_DrawTextLines_Hook));
    
    patchDWORD(0x00622E24, reinterpret_cast<uintptr_t>(CKeyInput_Draw_Hook));   // vtable 슬롯 교체
    PatchRelativeCall(0x0049EDCE, reinterpret_cast<uintptr_t>(Input_Hook));
#endif
}

// 1. 새로운 폰트 경로 (길이에 제한 없음!)
static char g_szNewFontPath[MAX_PATH] = "./Data_kr/Fonts/font%d";
static char g_szNewDialgouePath[MAX_PATH] = "./Data_kr/Dialogue/%s";

static char g_szNew_data_str_Path[MAX_PATH] = "Data_kr\\STR\\"; // 0051bd4e
static char g_szNew_DATA_STR_Path[MAX_PATH] = ".\\Data_kr\\STR";    // 0052d80d
static char g_szNewTEEPath[MAX_PATH] = "Data_kr\\STR\\TEE.str"; // 0052f772
static char g_szNewMonstersPath[MAX_PATH] = "Data_kr\\STR\\monsters.str";   // 0052f493


void ReadIniFilePath() 
{
    std::wstring fullSubPath;

    std::wstring fontFolder = g_Config.GetString(L"Settings", L"FontPath", L"./Data_kr/Fonts");
    if (PathIsRelativeW(fontFolder.c_str())) {
        fullSubPath = CombinePath(g_Config.currentExePath, fontFolder);
    }
    else {
        fullSubPath = fontFolder;
    }
    fontFolder = CombinePath(fullSubPath, L"font%d");
	WideCharToMultiByte(CP_UTF8, 0, fontFolder.c_str(), -1, g_szNewFontPath, sizeof(g_szNewFontPath), NULL, NULL);

    std::wstring dialogueFolder = g_Config.GetString(L"Settings", L"DialoguePath", L"./Data_kr/Dialogue");
    if (PathIsRelativeW(dialogueFolder.c_str())) {
        fullSubPath = CombinePath(g_Config.currentExePath, dialogueFolder);
    }
    else {
        fullSubPath = dialogueFolder;
    }
    dialogueFolder = CombinePath(fullSubPath, L"%s");
    WideCharToMultiByte(CP_UTF8, 0, dialogueFolder.c_str(), -1, g_szNewDialgouePath, sizeof(g_szNewDialgouePath), NULL, NULL);
}

void PatchResourcePath()
{
    //ReadIniFilePath();

    // 1. 0x0043ffd1 주소의 4바이트 포인터를 문자열 주소로 교체!
    patchDWORD(0x0043ffd1, reinterpret_cast<uintptr_t>(g_szNewFontPath));

    uintptr_t newPathAddr = reinterpret_cast<uintptr_t>(g_szNewDialgouePath);
    uintptr_t pDialpath[] = { 
        //0x005b2b86, 0x005b2ba5, 0x005b35d6, 0x005b35f5,     // 이곳은 all.idx, all.db를 읽기 위한 부분
        0x005b3832, 0x005b39d2, 0x005b3a31,0x005b3a6f, 0x005b3e3a, 0x005b44f1, 0x005b4541 
    };
    int elementCount = sizeof(pDialpath) / sizeof(pDialpath[0]);
    // 2. pDialpath[] 주소들 4바이트 포인터를 문자열 주소로 교체!
    for (int i = 0; i < elementCount; ++i) {
        patchBytes(pDialpath[i], &newPathAddr, 4);
    }

	// monsters.str 경로도 교체
    patchDWORD(0x0052f493, reinterpret_cast<uintptr_t>(g_szNewMonstersPath));

    // TEE.str 경로도 교체
    patchDWORD(0x0052f772, reinterpret_cast<uintptr_t>(g_szNewTEEPath));

    //0x006d5fa4 : data\\str\\  , 사용하는 곳은 0x0051bd4f
    //0x006d8814 : .\\Data\\STR , 사용하는 곳은 0x0052d80d, ./data/str/rand.str을 읽기 위한 것인것 같다.
    patchDWORD(0x0051bd4e, reinterpret_cast<uintptr_t>(g_szNew_data_str_Path));
    patchDWORD(0x0052d80d, reinterpret_cast<uintptr_t>(g_szNew_DATA_STR_Path));
	//0x006d8888 : .\\Data\\Rand\\STR , 사용하는 곳은 0x0052a8c2
}

void ApplyMemoryPatches() {
    //PatchResourcePath();
    Apply_Por2_Hooks();
}
