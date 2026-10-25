
#include "por2.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define g_nInputMode   (*(int*)  0x006AC960)
#define g_bInputActive (*(char*) 0x006AC970)
#define g_nInputLen    (*(unsigned int*)0x00701304)
#define g_szInput      ((char*)  0x007011D8)          // 256바이트 (+0xFF = 0x007012D7)
#define g_pUIObj       (*(void**)0x00727294)

/* FUN_0047e4c0 : thiscall, 스택 인자 18개 (callee 정리) → __fastcall 로 표현 */
typedef int(__fastcall* tOnEnter)(void* self, void* edx,
    int a1, int a2, int a3, int a4, int a5, int a6,
    int a7, int a8, int a9, int a10, int a11, int a12,
    int a13, int a14, int a15, int a16, int a17, int a18);
#define game_OnEnter ((tOnEnter)0x0047E4C0)

/* 앞에서부터 훑어 마지막 글자의 시작 인덱스를 구한다.
   (뒤에서 거꾸로 lead byte 를 판정하면 오판하므로 앞에서부터) */
static int LastCharStart(const char* s, int len)
{
    int i = 0, start = 0;
    while (i < len) {
        start = i;

        uint8_t c1 = static_cast<uint8_t>(s[i] & 0xFF);     // 현재 바이트
        uint8_t c2 = static_cast<uint8_t>(s[i+1] & 0xFF);   // 다음 바이트
        if (i + 1 < len && IsHangul(c1, c2)) {
            i += 2;
        } else {
            i += 1;
        }
        //if (IsDBCSLeadByte((BYTE)s[i]) && i + 1 < len) i += 2;
        //else                                          i += 1;
    }
    return start;
}

/* 원본: cdecl, 반환 항상 0.  3번째 인자(ch) 외에는 사용하지 않음 */
int __cdecl Input_Hook(int /*a1*/, int /*a2*/, int ch)
{
    if (g_nInputMode != 4 || !g_bInputActive)
        return 0;

    if (ch == 8)                                   /* Backspace */
    {
        if (g_nInputLen == 0)
            return 0;
        g_nInputLen = (unsigned)LastCharStart(g_szInput, (int)g_nInputLen);  // 1 또는 2바이트
        g_szInput[g_nInputLen] = 0;
        return 0;
    }

    if (ch == 0x0D)                                /* Enter */
    {
        game_OnEnter(g_pUIObj, 0,
            4, 0, 8, -1, 0x17, 0, 0, 0,
            0, 0, 0, 0, 1, 0, 0, 0, 0, 0);
        g_bInputActive = 0;
        return 0;
    }

    /* 일반 바이트: 원본과 동일하게 1바이트씩 저장 */
    if (g_nInputLen >= 0x100)
        return 0;

    g_szInput[g_nInputLen++] = (char)ch;
    if (g_nInputLen == 0x100) {
        g_nInputLen = 0xFF;
        g_szInput[0xFF] = 0;                       /* 원본: DAT_007012d7 = 0 */
        return 0;
    }
    g_szInput[g_nInputLen] = 0;
    return 0;
}


/*
typedef int (__cdecl* tInput)(int, int, int);
tInput g_origInput = NULL;
MH_CreateHook((LPVOID)0x00406D60, &Input_Hook, (LPVOID*)&g_origInput);
MH_EnableHook((LPVOID)0x00406D60);
XREF가 0049edce(c) 하나뿐이라, 그 CALL의 상대 주소를 직접 패치해도 됩니다.

//
*/