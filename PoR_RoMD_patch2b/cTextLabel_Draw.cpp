#include <windows.h>
#include <ddraw.h>
#include <string.h>
#include <stddef.h>

/*
DrawText (0x00442590) 훅 코드

cTextManager::DrawTextLines(0x43BC80)와 같은 구조의 고정 색 텍스트 렌더러입니다. 같은 클래스는 아닙니다. 폰트가 +0x164에 있어서 cTextManager(+0x1A0)와 오프셋이 다르기 때문에 구조체를 분리했습니다.

호출 규약: 끝이 RET 0x4이지만 이 스택 인자를 읽는 코드는 없습니다. 훅은 __fastcall(self, edx, int)로 선언해야 스택이 맞습니다. 호출부(0x623038의 vtable 호출)가 실제로 무엇을 push하는지는 확인해 두세요.
글자색: 하드코딩이 아니라 +0x168의 WORD입니다. DrawTextLines는 FUN_00436180으로 만든 고정색이었습니다.
좌표: 줄 시작 x는 +0x1A8, 줄 높이는 +0x144입니다. DrawTextLines의 5px, +0x140과 다릅니다.
32bpp 경로: 원본 버그가 있습니다. 글리프 위치를 무시하고 포인터가 글자마다 누적되므로 16bpp만 정상입니다. 원본 그대로 복원했습니다.
빈 줄: 줄 문자열이 ""이면 건너뛰고 penY만 줄 높이만큼 내려갑니다.

*/
/* ------------------------------------------------------------------ */
/*  구조체 (오프셋은 전부 어셈블리 기준)                                   */
/* ------------------------------------------------------------------ */
struct cFont { void** vtable; };
struct cRenderTarget { void** vtable; };       // vtable +0x54 : 대상 서피스 반환

struct cTextLabel {                            // 이름 임시 (vtable 0x00623038)
    void* vftable;                    // +0x000
    char           pad_004[0x140];             // +0x004
    int            m_nLineHeight;              // +0x144
    char           pad_148[0x10];              // +0x148
    int            m_nLineCount;               // +0x158
    char** m_ppLines;                  // +0x15C
    char           pad_160[4];                 // +0x160
    cFont* m_pFont;                    // +0x164
    WORD           m_wColor;                   // +0x168 글자색 (16bit 픽셀)
    char           pad_16A[0x0A];              // +0x16A
    cRenderTarget* m_pTarget;                  // +0x174
    char           pad_178[0x30];              // +0x178
    int            m_nLeft;                    // +0x1A8 줄 시작 x
};
static_assert(offsetof(cTextLabel, m_nLineHeight) == 0x144, "");
static_assert(offsetof(cTextLabel, m_nLineCount) == 0x158, "");
static_assert(offsetof(cTextLabel, m_ppLines) == 0x15C, "");
static_assert(offsetof(cTextLabel, m_pFont) == 0x164, "");
static_assert(offsetof(cTextLabel, m_wColor) == 0x168, "");
static_assert(offsetof(cTextLabel, m_pTarget) == 0x174, "");
static_assert(offsetof(cTextLabel, m_nLeft) == 0x1A8, "");

/* ------------------------------------------------------------------ */
/*  폰트 / 타깃 vtable 헬퍼 (이전 코드와 동일)                             */
/* ------------------------------------------------------------------ */
typedef int(__fastcall* tFontCharFn)(cFont*, void*, int);
typedef RECT* (__fastcall* tFontRectFn)(cFont*, void*, int);
typedef void* (__fastcall* tFontSurfFn)(cFont*, void*);
typedef IDirectDrawSurface7* (__fastcall* tTargetSurfFn)(cRenderTarget*, void*);

static __forceinline int   Font_GetCharWidth(cFont* f, char ch) { return ((tFontCharFn)f->vtable[0x20 / 4])(f, 0, (int)(unsigned char)ch); }
static __forceinline RECT* Font_GetCharRect(cFont* f, char ch) { return ((tFontRectFn)f->vtable[0x24 / 4])(f, 0, (int)ch); }   // movsx
static __forceinline void* Font_GetSurface(cFont* f) { return ((tFontSurfFn)f->vtable[0x28 / 4])(f, 0); }
static __forceinline IDirectDrawSurface7* Target_GetSurface(cRenderTarget* t)
{
    return ((tTargetSurfFn)t->vtable[0x54 / 4])(t, 0);
}

/* ------------------------------------------------------------------ */
/*  본체                                                                */
/*  원본: RET 4 → 훅은 (this, edx, int) 로 선언해야 스택이 맞는다          */
/* ------------------------------------------------------------------ */
BOOL __fastcall cTextLabel_DrawText_Hook(cTextLabel* self, void* /*edx*/, int /*unusedStackArg*/)
{
    DDSURFACEDESC2 ddsd;
    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize = 0x7C;

    IDirectDrawSurface7* pDst = Target_GetSurface(self->m_pTarget);
    IDirectDrawSurface7* pSrc = (IDirectDrawSurface7*)Font_GetSurface(self->m_pFont);

    int lineIdx = 0;
    int penX = self->m_nLeft;
    int penY = 0;

    /* ---- 대상 Lock (실패 시 Unlock 없이 FALSE) ---- */
    if (pDst->Lock(NULL, &ddsd, 0, NULL) != 0)
        return FALSE;

    BYTE* dstBase = (BYTE*)ddsd.lpSurface;
    int   dstPitchW = (int)((unsigned)ddsd.lPitch >> 1);

    int mode = 0;                                  // 원본은 초기화 안 된 스택 값
    switch (ddsd.ddpfPixelFormat.dwRGBBitCount) {
    case 8:  mode = 1; break;
    case 16: mode = 2; break;
    case 24: mode = 3; break;
    case 32: mode = 4; break;
    }

    /* ---- 폰트 Lock (같은 ddsd 재사용, 결과 미검사 — 원본 동작) ---- */
    pSrc->Lock(NULL, &ddsd, 0, NULL);
    BYTE* srcBase = (BYTE*)ddsd.lpSurface;
    int   srcPitchW = (int)((unsigned)ddsd.lPitch >> 1);

    /* 32bpp 경로 전용 러닝 포인터 (원본 버그: 글자마다 누적, rect/penX/penY 무시) */
    BYTE* dstRun = dstBase;
    BYTE* srcRun = srcBase;

    for (; lineIdx < self->m_nLineCount; lineIdx++)
    {
        const char* line = self->m_ppLines[lineIdx];

        for (int ci = 0; line[ci] != '\0'; ci++)
        {
            char  ch = line[ci];
            RECT* rc = Font_GetCharRect(self->m_pFont, ch);     // NULL 검사 없음 (원본)
            int   w = rc->right - rc->left;
            int   h = rc->bottom - rc->top;

            if (w > 0)
            {
                if (mode == 2)                                  /* 16bpp: 정상 경로 */
                {
                    if (h > 0)
                    {
                        const WORD* s = (const WORD*)srcBase + (rc->top * srcPitchW + rc->left);
                        WORD* d = (WORD*)dstBase + (penY * dstPitchW + penX);
                        for (int y = 0; y < h; y++) {
                            for (int x = 0; x < w; x++)
                                if (s[x] != 0)                  // 0 = 투명, 나머지는 전부 글자색
                                    d[x] = self->m_wColor;
                            s += srcPitchW;
                            d += dstPitchW;
                        }
                    }
                }
                else if (mode == 4 && h > 0)                    /* 32bpp: 원본 버그 그대로 */
                {
                    for (int y = 0; y < h; y++) {
                        BYTE* d = dstRun;
                        BYTE* s = srcRun;
                        for (int x = 0; x < w; x++) {
                            if (*(DWORD*)s != 0)
                                *(DWORD*)d = (DWORD)self->m_wColor;
                            d += 4; s += 4;
                        }
                        dstRun += dstPitchW * 2;
                        srcRun += srcPitchW * 2;
                    }
                }
            }

            penX += Font_GetCharWidth(self->m_pFont, ch);
        }

        penX = self->m_nLeft;                                  // 줄 끝: x 리셋, y 전진
        penY += self->m_nLineHeight;
    }

    /* Unlock 순서: 대상 → 폰트 */
    pDst->Unlock(NULL);
    pSrc->Unlock(NULL);
    return TRUE;
}

/*

설치

XREF가 00623038(*) 하나뿐인 vtable 데이터 참조입니다. 직접 CALL하는 곳이 없으므로 vtable 슬롯 교체가 가장 안전합니다.

// (1) vtable 슬롯 교체
DWORD old;
VirtualProtect((LPVOID)0x00623038, 4, PAGE_READWRITE, &old);
*(void**)0x00623038 = (void*)&cTextLabel_DrawText_Hook;
VirtualProtect((LPVOID)0x00623038, 4, old, &old);

// (2) 인라인 훅 (MinHook) 
typedef BOOL(__fastcall* tDrawText)(cTextLabel*, void*, int);
tDrawText g_origDrawText = NULL;
MH_CreateHook((LPVOID)0x00442590, &cTextLabel_DrawText_Hook, (LPVOID*)&g_origDrawText);
MH_EnableHook((LPVOID)0x00442590);


2바이트 문자

이 함수는 줄 문자열을 ci++로 1바이트씩 돌면서 GetCharRect, GetCharWidth를 호출합니다. 2바이트 출력을 고치셨다면 이 함수도 같은 방식으로 고쳐야 합니다. 이 코드에서 바꿀 곳은 두 군데입니다.

Font_GetCharRect(self->m_pFont, ch)
Font_GetCharWidth(self->m_pFont, ch), 그리고 ci++

앞의 DrawTextLines, CalculateTextExtents, DrawMenu에서 lead byte와 trail byte를 합쳐 글리프를 조회하는 방식이 있다면 알려 주세요. 그 방식을 이 함수에도 반영하겠습니다. 이 함수는 줄바꿈이나 폭 계산을 하지 않고 이미 나뉜 줄을 그리기만 하므로, 자르는 위치 문제는 없습니다.

확인할 것
스택 인자: RET 4의 인자가 실제로 쓰이는지는 이 함수만으로는 알 수 없습니다. 함수 안에서는 읽지 않으니 훅에서도 무시해도 됩니다.
GetCharRect가 NULL일 때: 원본은 검사하지 않습니다. 없는 글리프에서 크래시가 나면 공백(0x20) 대체를 넣으세요.
+0x1A8과 +0x144: 이 값들을 누가 채우는지는 모릅니다. 줄 높이는 CalculateTextExtents 같은 측정 함수가 채울 가능성이 있습니다. 같은 클래스의 측정 함수를 찾으려면 +0x144에 쓰는 함수를 검색해 보세요.
*/