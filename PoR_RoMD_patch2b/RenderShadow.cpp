#include "por2.h"

#include <string.h>
#define WIN32_LEAN_AND_MEAN // 불필요한 Windows 헤더 제외 (선택 사항)
#include <windows.h>        // COM 및 기본 Windows API 정의 (ddraw.h보다 먼저 포함되어야 함)
#include <ddraw.h>          // IDirectDrawSurface7 및 DirectDraw 핵심 헤더

#define _Text (self->m_pszText)

BOOL __fastcall cTextLayout_RenderShadow_Hook(cTextLayout* self, void* /*edx*/)
{
    IDirectDrawSurface7* pDst = self->GetTargetSurface();
    IDirectDrawSurface7* pSrc = self->m_pFont->GetSurface();

    if (pDst == NULL || pSrc == NULL)
        return FALSE;

    /* ---- 대상 서피스 Lock ---- */
    DDSURFACEDESC2 ddsdDst;
    memset(&ddsdDst, 0, sizeof(ddsdDst));
    ddsdDst.dwSize = sizeof(DDSURFACEDESC2);
    if (pDst->Lock(NULL, &ddsdDst, 0, NULL) != 0)
        return FALSE;

    WORD* dstBase = (WORD*)ddsdDst.lpSurface;
    const int dstPitchW = (int)((unsigned)ddsdDst.lPitch >> 1);

    /* ---- 폰트(아틀라스) 서피스 Lock ---- */
    DDSURFACEDESC2 ddsdSrc;
    memset(&ddsdSrc, 0, sizeof(ddsdSrc));
    ddsdSrc.dwSize = sizeof(DDSURFACEDESC2);
    if (pSrc->Lock(NULL, &ddsdSrc, 0, NULL) != 0)
    {
        pDst->Unlock(NULL);
        return FALSE;
    }

    WORD* srcBase = (WORD*)ddsdSrc.lpSurface;
    const int srcPitchW = (int)((unsigned)ddsdSrc.lPitch >> 1);

    int charIdx = 0;
    int yOffset = 0;     // 현재 줄의 y 시작 (줄 높이 누적)
    for(int lineIdx = 0; lineIdx < self->m_nLineCount; ++lineIdx)
    {
        int xOffset = 0;
        if (self->m_nAlign == 1) {
            xOffset = (self->m_nMaxLineWidth - self->m_pLineWidths[lineIdx]) / 2;
        }
#if 0
        const char sometext[] = "Disrupt Undead";
        if (strncmp(self->m_pszText + charIdx, sometext, strlen(sometext)) == 0) {
			__nop();
        }
#endif
        int chCode = getCharCode(_Text, charIdx);

        if (charIdx <= self->m_pLineEndIndices[lineIdx])
        {
            int dstRowOffset = dstPitchW * yOffset;
            do {
                RECT* rc = self->m_pFont->GetCharRect(chCode);
                int width = rc->right - rc->left;
                int height = rc->bottom - rc->top;

                const WORD* srcRow = srcBase + (rc->top * srcPitchW + rc->left);
                WORD* dstRow = dstBase + (dstRowOffset + xOffset);

                for (int iy = 0; iy < height; ++iy) {
                    for (int ix = 0; ix < width; ++ix) {
                        WORD px = srcRow[ix];
                        if (px == 0)
                            continue;                       // 투명

                        if (px == 0xFFFF)
                            px = self->m_wTextColor;        // 흰색 → 글자색

                        dstRow[ix] = px;

                        if (self->m_bShadow)                // 그림자: (+2, +2)
                            dstRow[ix + dstPitchW * 2 + 2] = self->m_wShadowColor;
                    }
                    srcRow += srcPitchW;
                    dstRow += dstPitchW;
                }

                xOffset += self->m_pFont->GetCharWidth(chCode);
                charIdx += (0xFF < chCode) ? 2 : 1;   // 2바이트면 +2, 아니면 +1
                chCode = getCharCode(_Text, charIdx);
            } while (chCode != '\0' && charIdx <= self->m_pLineEndIndices[lineIdx]);
        }

        /* 줄 끝의 공백/개행 건너뛰기 (원본: 현재 글자가 공백일 때만 진입) */
        if (IsSpaceCh(chCode))
        {
            do {
                chCode = getCharCode(_Text, ++charIdx);
                if (chCode == '\0')
                    break;
            } while (IsSpaceCh(chCode));
        }

        /* 다음 줄 */
        yOffset += self->m_nLineHeight;
    }

    pSrc->Unlock(NULL);
    pDst->Unlock(NULL);

    return TRUE;
}
#undef _Text


/*

typedef bool (__cdecl* tIsAltTarget)(void* surf);                   // FUN_59524530
typedef char (__cdecl* tAltRender)(cTextLayout*, void* surf);       // FUN_595249a0
typedef bool (__cdecl* tNoShadowMode)(void);                        // FUN_595815b0

#define IMG_BASE 0x59460000u
static BYTE* g_mod;
#define A(va) ((void*)(g_mod + ((va) - IMG_BASE)))

static inline bool IsSpaceByte(char c)           // 원본: signed 비교 인라인
{ return (c >= 9 && c <= 0x0D) || c == 0x20; }

BOOL __fastcall cTextLayout_RenderShadow_Hook(cTextLayout* self, void* //edx)
{
    IDirectDrawSurface7* pDst = self->GetTargetSurface();          // vtable +0x54

    // [추가 1] 대체 렌더 경로: 원본 함수를 그대로 호출
    if (((tIsAltTarget)A(0x59524530))(pDst))
        return ((tAltRender)A(0x595249A0))(self, pDst) != 0;

    IDirectDrawSurface7* pSrc = (IDirectDrawSurface7*)self->m_pFont->GetSurface();
    const char* text = self->m_pszText;
    const int   len  = (int)strlen(text);

    // [추가 2] 그림자 조건
    bool shadow = false;
    if (self->m_bShadow)
        shadow = !((tNoShadowMode)A(0x595815B0))();

    int xOffset = (self->m_nAlign == 1)
        ? (self->m_nMaxLineWidth - self->m_pLineWidths[0]) / 2 : 0;

    // [변경] ddsd 분리 + FAILED 검사
    DDSURFACEDESC2 dd = {}; dd.dwSize = 0x7C;
    if (FAILED(pDst->Lock(NULL, &dd, 0, NULL)))
        return FALSE;
    WORD* dstBase = (WORD*)dd.lpSurface;
    int   dstPitchW = (int)((unsigned)dd.lPitch >> 1);

    DDSURFACEDESC2 sd = {}; sd.dwSize = 0x7C;
    if (FAILED(pSrc->Lock(NULL, &sd, 0, NULL))) {
        pDst->Unlock(NULL);
        return FALSE;
    }
    WORD* srcBase = (WORD*)sd.lpSurface;
    int   srcPitchW = (int)((unsigned)sd.lPitch >> 1);

    int idx = 0, penY = 0;
    for (int line = 0; line < self->m_nLineCount; line++)
    {
        if (idx < len && idx <= self->m_pLineEndIndices[line])
        {
            do {
                int   code = getCharCode(text, idx);
                RECT* rc   = self->m_pFont->GetCharRect(code);

                if (rc) {                                       // [추가 3] NULL 이면 건너뜀
                    int w = rc->right - rc->left;
                    int h = rc->bottom - rc->top;
                    const WORD* s = srcBase + rc->top * srcPitchW + rc->left;
                    WORD*       d = dstBase + penY * dstPitchW + xOffset;
                    for (int y = 0; y < h; y++) {
                        for (int x = 0; x < w; x++) {
                            WORD px = s[x];
                            if (!px) continue;
                            if (px == 0xFFFF) px = self->m_wTextColor;
                            d[x] = px;
                            if (shadow)
                                d[x + dstPitchW * 2 + 2] = self->m_wShadowColor;
                        }
                        s += srcPitchW; d += dstPitchW;
                    }
                }
                xOffset += self->m_pFont->GetCharWidth(code);   // WordWrap 훅과 같은 폭 사용
                idx     += (0xFF < code) ? 2 : 1;
            } while (text[idx] && idx <= self->m_pLineEndIndices[line]);
        }

        // 줄 사이 공백 스킵 (현재 글자가 공백일 때만)
        if (IsSpaceByte(text[idx])) {
            do { idx++; } while (text[idx] && IsSpaceByte(text[idx]));
        }

        xOffset = 0;
        if (self->m_nAlign == 1 && line + 1 < self->m_nLineCount)
            xOffset = (self->m_nMaxLineWidth - self->m_pLineWidths[line + 1]) / 2;
        penY += self->m_nLineHeight;
    }

    pSrc->Unlock(NULL);
    pDst->Unlock(NULL);
    return TRUE;
}

*/