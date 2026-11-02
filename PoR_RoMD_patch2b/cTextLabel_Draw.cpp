#include "por2.h"


/* ------------------------------------------------------------------ */
/*  본체                                                                */
/*  원본: RET 4 → 훅은 (this, edx, int) 로 선언해야 스택이 맞는다          */
/* ------------------------------------------------------------------ */
BOOL __fastcall cTextLabel_DrawText_Hook(cTextLabel* self, void* /*edx*/, int /*unusedStackArg*/)
{
    IDirectDrawSurface7* pDst = self->m_pTarget->GetTargetSurface();
    IDirectDrawSurface7* pSrc = self->m_pFont->GetSurface();;

    if (pDst == NULL || pSrc == NULL)
        return FALSE;

    DDSURFACEDESC2 ddsdDst;
    memset(&ddsdDst, 0, sizeof(ddsdDst));
    ddsdDst.dwSize = sizeof(DDSURFACEDESC2);
    /* ---- 대상 Lock (실패 시 Unlock 없이 FALSE) ---- */
    if (pDst->Lock(NULL, &ddsdDst, 0, NULL) != 0)
        return FALSE;

    BYTE* dstBase = (BYTE*)ddsdDst.lpSurface;
    int   dstPitchW = (int)((unsigned)ddsdDst.lPitch >> 1);

    int mode = 0;                                  // 원본은 초기화 안 된 스택 값
    switch (ddsdDst.ddpfPixelFormat.dwRGBBitCount) {
    case 8:  mode = 1; break;
    case 16: mode = 2; break;
    case 24: mode = 3; break;
    case 32: mode = 4; break;
    }

    DDSURFACEDESC2 ddsdSrc;
    memset(&ddsdSrc, 0, sizeof(ddsdSrc));
    ddsdSrc.dwSize = sizeof(DDSURFACEDESC2);
    /* ---- 폰트 Lock ---- */
    if (pSrc->Lock(NULL, &ddsdSrc, 0, NULL) != 0) {
        pDst->Unlock(NULL);
        return FALSE;
    }
    BYTE* srcBase = (BYTE*)ddsdSrc.lpSurface;
    int   srcPitchW = (int)((unsigned)ddsdSrc.lPitch >> 1);

    int offsetY = 0;
    for (int lineIdx = 0; lineIdx < self->m_nLineCount; ++lineIdx)
    {
        const char* pLine = self->m_ppLines[lineIdx];
        int offsetX = self->m_nLeft;
        int chIdx = 0;
        do {
            int chCode = getCharCode(pLine, chIdx);
            RECT* rc = self->m_pFont->GetCharRect(chCode);     // NULL 검사 없음 (원본)
            int   chW = rc->right - rc->left;
            int   chH = rc->bottom - rc->top;

            if (mode == 2 && chW > 0 && chH > 0)                                  /* 16bpp: 정상 경로 */
            {
                const WORD* s = (const WORD*)srcBase + (rc->top * srcPitchW + rc->left);
                WORD* d = (WORD*)dstBase + (offsetY * dstPitchW + offsetX);
                for (int iy = 0; iy < chH; ++iy) {
                    for (int ix = 0; ix < chW; ++ix)
                        if (s[ix] != 0)                  // 0 = 투명, 나머지는 전부 글자색
                            d[ix] = self->m_wColor;
                    s += srcPitchW;
                    d += dstPitchW;
                }
            }

            offsetX += self->m_pFont->GetCharWidth(chCode);

            chIdx += (0xFF < chCode) ? 2 : 1;   // 2바이트면 +2, 아니면 +1
        } while (pLine[chIdx] != '\0');

        offsetY += self->m_nLineHeight;// 줄 끝: x 리셋, y 전진
    }

    /* Unlock 순서: 대상 → 폰트 */
    pDst->Unlock(NULL);
    pSrc->Unlock(NULL);
    return TRUE;
}

/*
DrawText (0x00442590) 훅 코드

cTextManager::DrawTextLines(0x43BC80)와 같은 구조의 고정 색 텍스트 렌더러입니다. 같은 클래스는 아닙니다. 폰트가 +0x164에 있어서 cTextManager(+0x1A0)와 오프셋이 다르기 때문에 구조체를 분리했습니다.

호출 규약: 끝이 RET 0x4이지만 이 스택 인자를 읽는 코드는 없습니다. 훅은 __fastcall(self, edx, int)로 선언해야 스택이 맞습니다. 호출부(0x623038의 vtable 호출)가 실제로 무엇을 push하는지는 확인해 두세요.
글자색: 하드코딩이 아니라 +0x168의 WORD입니다. DrawTextLines는 FUN_00436180으로 만든 고정색이었습니다.
좌표: 줄 시작 x는 +0x1A8, 줄 높이는 +0x144입니다. DrawTextLines의 5px, +0x140과 다릅니다.
32bpp 경로: 원본 버그가 있습니다. 글리프 위치를 무시하고 포인터가 글자마다 누적되므로 16bpp만 정상입니다. 원본 그대로 복원했습니다.
빈 줄: 줄 문자열이 ""이면 건너뛰고 penY만 줄 높이만큼 내려갑니다.

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
*/