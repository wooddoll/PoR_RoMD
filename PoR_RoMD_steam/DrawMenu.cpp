#include "por2.h"
#include <vector>

#define MENU_MAX_TEXT_W     140     // 이보다 넓으면 "..." 로 자름 (원본 0x8C)
#define MENU_LEFT_MARGIN    5
#define MENU_ARROW_MARGIN   5

#define TEMP_BUFF_SIZE      256

// ---- FUN_595293b0 / 텍스트 폭 측정 및 "..." 생략 처리 ----
int __cdecl cTextManager_DrawMenuTrunc_Hook(cTextManager* self, int lineIdx, char* buf, char* flag)
{
    if (!self || !self->m_ppLinesB || !self->m_ppLinesB[lineIdx] || !buf) {
        if (flag) *flag = 0;
        return 0;
    }

    strncpy_s(buf, TEMP_BUFF_SIZE, self->m_ppLinesB[lineIdx], TEMP_BUFF_SIZE - 1);
    buf[TEMP_BUFF_SIZE - 1] = '\0';
    if (flag) *flag = 0;

    const int text_len = (int)strlen(buf);
    std::vector<int> charPositions;
    int lineW = 0;
    int chIdx = 0;

    while (chIdx < text_len) {
        int chCode = getCharCode(buf, chIdx);
        int chWidth = self->m_pFont ? self->m_pFont->GetCharWidth(chCode) : 0;

        lineW += chWidth;
        if (lineW > MENU_MAX_TEXT_W) {
            // 폭 140px 초과 시: 뒤에서부터 말줄임표(...) 대체 위치 결정
            int replacePos = 0;
            if (!charPositions.empty()) {
                // 최소 3글자 이상 기록되어 있으면 뒤에서 3글자 전 위치, 아니면 0번 위치
                int targetIdx = (int)charPositions.size() >= 3 ? (int)charPositions.size() - 3 : 0;
                replacePos = charPositions[targetIdx];
            }

            // "..." (3글자 + NUL) 기록
            buf[replacePos] = '.';
            buf[replacePos + 1] = '.';
            buf[replacePos + 2] = '.';
            buf[replacePos + 3] = '\0';

            if (flag) *flag = 1;
            return replacePos + 3;
        }

        charPositions.push_back(chIdx);
        chIdx += (0xFF < chCode) ? 2 : 1;
    }

    return chIdx;
}

// ---- DrawMenu_59529ea0 (0x000C9EA0) / DrawMenu_0043b4f0 대체 훅 ----
BOOL __fastcall cTextManager_MenuDraw_Hook(cTextManager* self, void* /*edx*/)
{
    if (!self || !self->m_pPartBottom || !self->m_pFont)
        return FALSE;

    IDirectDrawSurface7* pDst = self->m_pPartBottom->GetTargetSurface();
    IDirectDrawSurface7* pSrc = self->m_pFont->GetSurface();

    if (!pDst || !pSrc)
        return FALSE;

    DDSURFACEDESC2 ddsdDst;
    memset(&ddsdDst, 0, sizeof(ddsdDst));
    ddsdDst.dwSize = sizeof(DDSURFACEDESC2);
    if (pDst->Lock(NULL, &ddsdDst, DDLOCK_WAIT, NULL) != DD_OK)
        return FALSE;

    DDSURFACEDESC2 ddsdSrc;
    memset(&ddsdSrc, 0, sizeof(ddsdSrc));
    ddsdSrc.dwSize = sizeof(DDSURFACEDESC2);
    if (pSrc->Lock(NULL, &ddsdSrc, DDLOCK_WAIT, NULL) != DD_OK) {
        pDst->Unlock(NULL);
        return FALSE;
    }

    // 16비트 서피스 여부 검사
    if (ddsdDst.ddpfPixelFormat.dwRGBBitCount != 16 || !ddsdDst.lpSurface || !ddsdSrc.lpSurface) {
        pSrc->Unlock(NULL);
        pDst->Unlock(NULL);
        return FALSE;
    }

    WORD* dstBase = (WORD*)ddsdDst.lpSurface;
    WORD* srcBase = (WORD*)ddsdSrc.lpSurface;
    int dstPitchW = (int)((unsigned)ddsdDst.lPitch >> 1);
    int srcPitchW = (int)((unsigned)ddsdSrc.lPitch >> 1);

    char buf[TEMP_BUFF_SIZE];
    int penY = 0;

    for (int line = 0; line < self->m_nLineCountB; ++line)
    {
        WORD color = (self->m_pSelFlags && self->m_pSelFlags[line])
            ? self->m_wSelColor : self->m_wNormalColor;

        char trunc = 0;
        int n = cTextManager_DrawMenuTrunc_Hook(self, line, buf, &trunc);
        if (trunc && self->m_pTruncFlags)
            self->m_pTruncFlags[line] = 1;

        int penX = MENU_LEFT_MARGIN;
        for (int chIdx = 0; chIdx < n; )
        {
            int chCode = getCharCode(buf, chIdx);
            RECT* rc = self->m_pFont->GetCharRect(chCode);
            if (rc) {
                int chW = rc->right - rc->left;
                int chH = rc->bottom - rc->top;

                // 메뉴 하위 표시 기호 '>'는 우측 정렬
                if (chCode == '>')
                    penX = self->m_nWidth30 - chW - MENU_ARROW_MARGIN;

                if (chW > 0 && chH > 0 && penX >= 0) {
                    const WORD* s = srcBase + rc->top * srcPitchW + rc->left;
                    WORD* d = dstBase + penY * dstPitchW + penX;

                    for (int iy = 0; iy < chH; ++iy) {
                        for (int ix = 0; ix < chW; ++ix) {
                            WORD px = s[ix];
                            if (px != 0)
                                d[ix] = (px == 0xFFFF) ? color : px;
                        }
                        s += srcPitchW;
                        d += dstPitchW;
                    }
                }
                penX += chW;
            }
            chIdx += (0xFF < chCode) ? 2 : 1;
        }
        penY += self->m_nLineHeight;
    }

    pSrc->Unlock(NULL);
    pDst->Unlock(NULL);
    return TRUE;
}