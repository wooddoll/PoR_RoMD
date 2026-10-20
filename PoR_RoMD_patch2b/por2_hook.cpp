#include "por2.h"
#include <MinHook.h>

static const RECT g_dummyZeroRect = { 0, 0, 0, 0 };

BOOL __fastcall cTextLayout_WordWrap_Hook(cTextLayout* self, void* /*edx*/);
BOOL __fastcall cTextLayout_RenderShadow_Hook(cTextLayout* self, void* /*edx*/);
BOOL __fastcall cTextManager_CalculateTextExtents_Hook(cTextManager* self, void* /*edx*/);
BOOL __fastcall cTextBox_DrawTextLines_Hook(cTextManager* self, void* /*edx*/, int /*unusedStackArg*/);

const uintptr_t ptrWordWrap = 0x00444890;
typedef BOOL (__fastcall* tWordWrap)(cTextLayout*, void*);
tWordWrap g_origWordWrap = NULL;

const uintptr_t ptrRenderShadow = 0x00444D40;
typedef BOOL (__fastcall* tRenderShadow)(cTextLayout*, void*);
tRenderShadow g_origRenderShadow = NULL;

const uintptr_t ptrCalculateTextExtents = 0x0043BF40;
typedef BOOL (__fastcall* tCalcExtents)(cTextManager*, void*);
tCalcExtents g_origCalcExtents = NULL;

const uintptr_t ptrDrawTextLines = 0x0043BC80;
typedef BOOL (__fastcall* tDrawTextLines)(cTextManager*, void*, int);
tDrawTextLines g_origDrawTextLines = NULL;

//------- for debug
BOOL __fastcall _cTextLayout_WordWrap_Hook(cTextLayout* self, void* /*edx*/) {
    if(g_origWordWrap == NULL) {
        PLOG_ERROR << "[PoR2] g_origWordWrap is NULL!";
        return FALSE;
    }
    BOOL ret = g_origWordWrap(self, NULL);
    return ret;
}
BOOL __fastcall _cTextLayout_RenderShadow_Hook(cTextLayout* self, void* /*edx*/) {
    if(g_origRenderShadow == NULL) {
        PLOG_ERROR << "[PoR2] g_origRenderShadow is NULL!";
        return FALSE;
    }
    BOOL ret = g_origRenderShadow(self, NULL);
    return ret;
}
BOOL __fastcall _cTextManager_CalculateTextExtents_Hook(cTextManager* self, void* /*edx*/) {
    if(g_origCalcExtents == NULL) {
        PLOG_ERROR << "[PoR2] g_origCalcExtents is NULL!";
        return FALSE;
    }
    BOOL ret = g_origCalcExtents(self, NULL);
    return ret;
}
BOOL __fastcall _cTextBox_DrawTextLines_Hook(cTextManager* self, void* /*edx*/, int /*unusedStackArg*/) {
    if(g_origDrawTextLines == NULL) {
        PLOG_ERROR << "[PoR2] g_origDrawTextLines is NULL!";
        return FALSE;
    }
    BOOL ret = g_origDrawTextLines(self, NULL, /*unusedStackArg*/0);
    return ret;
}
//-------

#define USING_MINHOOK   1
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
        PLOG_ERROR << "[PoR2] cTextBox_DrawTextLines_Hook hook failed.";
    }

    PLOG_DEBUG << "[PoR2] function hooked.";
#else
    HookFunction(ptrWordWrap, reinterpret_cast<uintptr_t>(cTextLayout_WordWrap_Hook));
    HookFunction(ptrRenderShadow, reinterpret_cast<uintptr_t>(cTextLayout_RenderShadow_Hook));
    HookFunction(ptrCalculateTextExtents, reinterpret_cast<uintptr_t>(cTextManager_CalculateTextExtents_Hook));
    HookFunction(ptrDrawTextLines, reinterpret_cast<uintptr_t>(cTextBox_DrawTextLines_Hook));
#endif
}

// 1. 새로운 폰트 경로 (길이에 제한 없음!)
static char g_szNewFontPath[MAX_PATH] = "./Data_kr/Fonts/font%d";
static char g_szNewDialgouePath[MAX_PATH] = "./Data_kr/Dialogue/%s";

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
    ReadIniFilePath();

    uintptr_t newPathAddr = reinterpret_cast<uintptr_t>(g_szNewFontPath);
    // 2. 0x0043ffd1 주소의 4바이트 포인터를 문자열 주소로 교체!
    patchBytes(0x0043ffd1, &newPathAddr, 4);

    newPathAddr = reinterpret_cast<uintptr_t>(g_szNewDialgouePath);
    uintptr_t pDialpath[] = { 0x005b2b86, 0x005b2ba5, 0x005b35d6, 0x005b35f5, 0x005b3832, 0x005b39d2, 0x005b3a31,0x005b3a6f, 0x005b3e3a, 0x005b44f1, 0x005b4541 };
    int elementCount = sizeof(pDialpath) / sizeof(pDialpath[0]);
    // 2. pDialpath[] 주소들 4바이트 포인터를 문자열 주소로 교체!
    for (int i = 0; i < elementCount; ++i) {
        patchBytes(pDialpath[i], &newPathAddr, 4);
    }
}

void ApplyMemoryPatches() {
    PatchResourcePath();
    Apply_Por2_Hooks();
}