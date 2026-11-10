#include "por2.h"

#include <string.h>
#define WIN32_LEAN_AND_MEAN // 불필요한 Windows 헤더 제외 (선택 사항)
#include <windows.h>        // COM 및 기본 Windows API 정의 (ddraw.h보다 먼저 포함되어야 함)
#include <ddraw.h>          // IDirectDrawSurface7 및 DirectDraw 핵심 헤더

#include "help_util.h"

// IDirectDrawSurface7 vtable: Blt=0x14, Lock=0x64, Unlock=0x80
static void* g_watch = nullptr;                       // 훅에서 pDst 저장
typedef HRESULT(__stdcall* tLock)(void*, RECT*, DDSURFACEDESC2*, DWORD, HANDLE);
static tLock g_origLock = nullptr;
static HRESULT __stdcall Lock_Spy(void* s, RECT* r, DDSURFACEDESC2* d, DWORD f, HANDLE h)
{
    //if (s == g_watch) 
    {
        PLOG_DEBUG << "Lock " << std::hex << std::showbase << s << " ret=" << _ReturnAddress();
    }
    return g_origLock(s, r, d, f, h);
}

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

    /////////////////////////////////////////////////////////////////////////

    const char* text = self->m_pszText;
    const int   text_len = (int)strlen(text);

    if (!text || text_len < 1 || !self->m_pFont)
    {
        pSrc->Unlock(NULL);
        pDst->Unlock(NULL);
        return FALSE;
	}

    int charIdx = 0;
    int yOffset = 0;     // 현재 줄의 y 시작 (줄 높이 누적)
    for(int lineIdx = 0; lineIdx < self->m_nLineCount; ++lineIdx)
    {
        int xOffset = 0;
        if (self->m_nAlign == 1) {
            xOffset = (self->m_nMaxLineWidth - self->m_pLineWidths[lineIdx]) / 2;
        }

        int chCode = getCharCode(text, charIdx);

        if (charIdx < text_len && charIdx <= self->m_pLineEndIndices[lineIdx])
        {
            int dstRowOffset = dstPitchW * yOffset;
            do {
                RECT* rc = self->m_pFont->GetCharRect(chCode);
                int width = rc->right - rc->left;
                int height = rc->bottom - rc->top;

                const WORD* srcRow = srcBase + (rc->top * srcPitchW + rc->left);
                WORD*       dstRow = dstBase + (dstRowOffset + xOffset);

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
                chCode = getCharCode(text, charIdx);
            } while (chCode != '\0' && charIdx <= self->m_pLineEndIndices[lineIdx]);
        }

        /* 줄 끝의 공백/개행 건너뛰기 (원본: 현재 글자가 공백일 때만 진입) */
        while (chCode != '\0' && IsSpaceCh(chCode)) {
            chCode = getCharCode(text, ++charIdx);
        }

        /* 다음 줄 */
        yOffset += self->m_nLineHeight;
    }


    {
#if 0
        if (g_watch == nullptr) {
            uintptr_t vtable = *reinterpret_cast<uintptr_t*>(pDst);
            uintptr_t targetAddr = vtable + 0x64;
            uint32_t origAddr = *reinterpret_cast<uint32_t*>(targetAddr);
            g_origLock = reinterpret_cast<tLock>(origAddr);
            g_watch = pDst; // 관찰 대상 등록

            patchDWORD(targetAddr, reinterpret_cast<uintptr_t>(Lock_Spy));
        }

		int xOffset = 0;
        int yOffset = 0;
        int dstRowOffset = dstPitchW * yOffset;
        WORD* dstRow = dstBase + (dstRowOffset + xOffset);
        int width = 5;
        int height = 10;
        for (int iy = 0; iy < height; ++iy) {
            for (int ix = 0; ix < width; ++ix) {
                dstRow[ix] = 0xF800;
            }
            dstRow += dstPitchW;
        }
#endif
    }
    pSrc->Unlock(NULL);
    pDst->Unlock(NULL);

    return TRUE;
}
