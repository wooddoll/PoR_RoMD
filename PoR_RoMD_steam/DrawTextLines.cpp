#include "por2.h"
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddraw.h>

#define TEXT_LEFT_MARGIN  5
#define TEXT_COLOR_16     0x24FB    // 원본 게임 색상: RGB(41, 159, 223)

/* ------------------------------------------------------------------ */
/*  cTextManager_DrawTextLines_Hook                                    */
/*  원본: RET 0x4 (__thiscall with 1 stack argument)                   */
/* ------------------------------------------------------------------ */
BOOL __fastcall cTextManager_DrawTextLines_Hook(cTextManager* self, void* /*edx*/, int /*unusedStackArg*/)
{
    if (!self || !self->m_pTarget || !self->m_pFont || !self->m_ppLinesA)
        return FALSE;

    IDirectDrawSurface7* pDst = self->m_pTarget->GetTargetSurface();
    IDirectDrawSurface7* pSrc = self->m_pFont->GetSurface();

    if (pDst == NULL || pSrc == NULL)
        return FALSE;

    /* ---- 대상 서피스 Lock ---- */
    DDSURFACEDESC2 ddsdDst;
    memset(&ddsdDst, 0, sizeof(ddsdDst));
    ddsdDst.dwSize = sizeof(DDSURFACEDESC2);
    if (pDst->Lock(NULL, &ddsdDst, 0, NULL) != 0)
        return FALSE;

    if (ddsdDst.ddpfPixelFormat.dwRGBBitCount != 16) { /* 16bpp 만 지원 */
        pDst->Unlock(NULL);
        return FALSE;
    }

    BYTE* dstBase = (BYTE*)ddsdDst.lpSurface;
    const int dstPitchW = (int)((unsigned)ddsdDst.lPitch >> 1);

    /* ---- 폰트 서피스 Lock ---- */
    DDSURFACEDESC2 ddsdSrc;
    memset(&ddsdSrc, 0, sizeof(ddsdSrc));
    ddsdSrc.dwSize = sizeof(DDSURFACEDESC2);
    if (pSrc->Lock(NULL, &ddsdSrc, 0, NULL) != 0) {
        pDst->Unlock(NULL);
        return FALSE;
    }

    BYTE* srcBase = (BYTE*)ddsdSrc.lpSurface;
    const int srcPitchW = (int)((unsigned)ddsdSrc.lPitch >> 1);

    int yOffset = 0;
    for (int lineIdx = 0; lineIdx < self->m_nLineCountA; lineIdx++)
    {
        const char* pLine = self->m_ppLinesA[lineIdx];
        if (!pLine) {
            yOffset += self->m_nLineHeight;
            continue;
        }

        int xOffset = TEXT_LEFT_MARGIN;
        int charIdx = 0;

        while (pLine[charIdx] != '\0')
        {
            int chCode = getCharCode(pLine, charIdx);
            RECT* rc = self->m_pFont->GetCharRect(chCode);

            if (rc) {
                int charW = rc->right - rc->left;
                int charH = rc->bottom - rc->top;

                if (0 < charW && 0 < charH)
                {
                    const WORD* srcRow = (const WORD*)srcBase + (rc->top * srcPitchW + rc->left);
                    WORD* dstRow = (WORD*)dstBase + (yOffset * dstPitchW + xOffset);

                    for (int iy = 0; iy < charH; iy++) {
                        for (int ix = 0; ix < charW; ix++) {
                            if (srcRow[ix] != 0)         // 0 = 투명
                                dstRow[ix] = TEXT_COLOR_16;
                        }
                        srcRow += srcPitchW;
                        dstRow += dstPitchW;
                    }
                }
            }

            xOffset += self->m_pFont->GetCharWidth(chCode);
            charIdx += (0xFF < chCode) ? 2 : 1;   // 2바이트면 +2, 아니면 +1
        }

        yOffset += self->m_nLineHeight;
    }

    /* Unlock 순서: 대상 -> 폰트 */
    pDst->Unlock(NULL);
    pSrc->Unlock(NULL);

    return TRUE;
}