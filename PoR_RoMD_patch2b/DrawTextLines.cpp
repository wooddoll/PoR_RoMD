#include "por2.h"

#include <string.h>
#define WIN32_LEAN_AND_MEAN // 불필요한 Windows 헤더 제외 (선택 사항)
#include <windows.h>        // COM 및 기본 Windows API 정의 (ddraw.h보다 먼저 포함되어야 함)
#include <ddraw.h>          // IDirectDrawSurface7 및 DirectDraw 핵심 헤더

/* ------------------------------------------------------------------ */
/*  본체                                                                */
/*  원본: RET 0x4 → 스택 인자 1개를 callee 가 정리한다 (함수 안에서는 안 읽음) */
/*  따라서 훅은 __fastcall(this, edx, int) 로 선언해야 스택이 맞는다.         */
/* ------------------------------------------------------------------ */
#define TEXT_LEFT_MARGIN  5
#define TEXT_COLOR_A      0x29
#define TEXT_COLOR_B      0x9F
#define TEXT_COLOR_C      0xDF

BOOL __fastcall cTextBox_DrawTextLines_Hook(cTextManager* self, void* /*edx*/, int /*unusedStackArg*/)
{
    IDirectDrawSurface7* pDst = self->m_pTarget->GetSurface();
    IDirectDrawSurface7* pSrc = self->m_pFont->GetSurface();

    if (pDst == NULL || pSrc == NULL)
        return FALSE;

    //============================

    DDSURFACEDESC2 ddsdDst;
    memset(&ddsdDst, 0, sizeof(ddsdDst));
    ddsdDst.dwSize = sizeof(DDSURFACEDESC2);
    /* ---- 대상 서피스 Lock (실패 시 아무것도 Unlock 하지 않고 반환) ---- */
    if (pDst->Lock(NULL, &ddsdDst, 0, NULL) != 0)
        return FALSE;

    BYTE* dstBase = (BYTE*)ddsdDst.lpSurface;
    const int dstPitchW = (int)((unsigned)ddsdDst.lPitch >> 1);

    int mode = 0;   // 원본은 초기화 안 된 스택 변수. 1=8bpp 2=16bpp 3=24bpp 4=32bpp
    switch (ddsdDst.ddpfPixelFormat.dwRGBBitCount) {
    case 8:  mode = 1; break;
    case 16: mode = 2; break;
    case 24: mode = 3; break;
    case 32: mode = 4; break;
    default: break;                 // mode 는 건드리지 않음
    }

    /* ---- 폰트 서피스 Lock ---- */
    DDSURFACEDESC2 ddsdSrc;
    if (pSrc->Lock(NULL, &ddsdSrc, 0, NULL) != 0) {
        pDst->Unlock(NULL);
        return FALSE;
    }

    BYTE* srcBase = (BYTE*)ddsdSrc.lpSurface;
    const int srcPitchW = (int)((unsigned)ddsdSrc.lPitch >> 1);

    //----------------
    const WORD textColor = game_MakeColor16(TEXT_COLOR_A, TEXT_COLOR_B, TEXT_COLOR_C);
    int yOffset = 0;
    for (int lineIdx = 0; lineIdx < self->m_nLineCountA; lineIdx++)
    {
        int xOffset = TEXT_LEFT_MARGIN;
        int charIdx = 0;

        while (self->m_ppLinesA[lineIdx][charIdx] != '\0')
        {
            int chCode = getCharCode(self->m_ppLinesA[lineIdx], charIdx);
            RECT* rc = self->m_pFont->GetCharRect(chCode);

            int width = rc->right - rc->left;
            int height = rc->bottom - rc->top;

            /* 16bpp 에서만 그린다 (다른 모드는 폭 전진만 한다) */
            if (0 < width && mode == 2 && 0 < height) 
            {
                const WORD* srcRow = (const WORD*)srcBase + (rc->top * srcPitchW + rc->left);
                WORD* dstRow = (WORD*)dstBase + (yOffset * dstPitchW + xOffset);

                for (int iy = 0; iy < height; iy++) {
                    for (int ix = 0; ix < width; ix++) {
                        if (srcRow[ix] != 0)         // 0 = 투명
                            dstRow[ix] = textColor;
                    }
                    srcRow += srcPitchW;
                    dstRow += dstPitchW;
                }
            }

            xOffset += self->m_pFont->GetCharWidth(chCode);
            charIdx += (0xFF < chCode) ? 2 : 1;   // 2바이트면 +2, 아니면 +1
        }

        yOffset += self->m_nLineHeight;
    }

    /* Unlock 순서: 대상 → 폰트 */
    pDst->Unlock(NULL);
    pSrc->Unlock(NULL);

    return TRUE;
}