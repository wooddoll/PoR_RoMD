#include "por2.h"

BOOL __cdecl cTextLabel_DrawText_Hook(cTextLabel* self, IDirectDrawSurface7* pDst)
{
    IDirectDrawSurface7* pSrc = self->m_pFont->GetSurface();;

    if (pDst == NULL || pSrc == NULL)
        return FALSE;

    DDSURFACEDESC2 ddsdDst;
    memset(&ddsdDst, 0, sizeof(ddsdDst));
    ddsdDst.dwSize = sizeof(DDSURFACEDESC2);
    /* ---- 대상 Lock (실패 시 Unlock 없이 FALSE) ---- */
    if (pDst->Lock(NULL, &ddsdDst, 0, NULL) != 0)
        return FALSE;

    if (ddsdDst.ddpfPixelFormat.dwRGBBitCount != 16) { /* 16bpp: 정상 경로 */
        pDst->Unlock(NULL);
        return FALSE;
    }

    BYTE* dstBase = (BYTE*)ddsdDst.lpSurface;
    int   dstPitchW = (int)((unsigned)ddsdDst.lPitch >> 1);

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
        const int text_len  = (int)strlen(pLine);

        int offsetX = self->m_nLeft;
        for (int chIdx = 0; chIdx < text_len;)
        {
            int chCode = getCharCode(pLine, chIdx);
            RECT* rc = self->m_pFont->GetCharRect(chCode);
            if (rc) {
                int   chW = rc->right - rc->left;
                int   chH = rc->bottom - rc->top;
                if (chW > 0 && chH > 0)
                {
                    const WORD* s = (const WORD*)srcBase + (rc->top * srcPitchW + rc->left);
                    WORD*       d =       (WORD*)dstBase + (offsetY * dstPitchW + offsetX);
                    for (int iy = 0; iy < chH; ++iy) {
                        for (int ix = 0; ix < chW; ++ix) {
                            if (s[ix] != 0)                  // 0 = 투명, 나머지는 전부 글자색
                                d[ix] = self->m_wColor;
                        }
                        s += srcPitchW;
                        d += dstPitchW;
                    }
                }
            }
            offsetX += self->m_pFont->GetCharWidth(chCode);
            chIdx += (0xFF < chCode) ? 2 : 1;   // 2바이트면 +2, 아니면 +1
        }

        offsetY += self->m_nLineHeight;// 줄 끝: x 리셋, y 전진
    }

    /* Unlock 순서: 대상 → 폰트 */
    pDst->Unlock(NULL);
    pSrc->Unlock(NULL);
    return TRUE;
}
