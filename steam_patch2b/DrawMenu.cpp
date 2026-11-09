#include "por2.h"
#include <vector>

#define MENU_MAX_TEXT_W     140     // 이보다 넓으면 "..." 로 자름
#define MENU_LEFT_MARGIN    5
#define MENU_ARROW_MARGIN   5

#define TEMP_BUFF_SIZE      256
// ---- FUN_595293b0 ----
int __cdecl cTextManager_DrawMenuTrunc_Hook(cTextManager* self, int lineIdx, char* buf, char* flag)
{
    strncpy_s(buf, TEMP_BUFF_SIZE, self->m_ppLinesB[lineIdx], TEMP_BUFF_SIZE-1);
    buf[TEMP_BUFF_SIZE-1] = 0;
    *flag = 0;

    /* ---- 폭 누적: 140px 초과 시 마지막 3글자를 "..." 로 교체 ---- */
    const int text_len  = (int)strlen(buf);
    std::vector<int> pos;
    int lineW = 0, chIdx = 0;

    while (chIdx < text_len) {
        int chCode = getCharCode(buf, chIdx);
        lineW += self->m_pFont->GetCharWidth(chCode);
        if (lineW > MENU_MAX_TEXT_W) {                                  // 원본: DAT_59613578 과 비교
            int n = (int)pos.size() - 3;
            int p = pos.empty() ? 0 : pos[n > 0 ? n : 0];
            memcpy(buf + p, "...", 4);                  // "..." + NUL
            *flag = 1;
            return p + 3;
        }
        pos.push_back(chIdx);
        chIdx += (0xFF < chCode) ? 2 : 1;
    }
    return chIdx;
}

// ---- FUN_5952ac20 ----
BOOL __cdecl cTextManager_MenuDraw_Hook(cTextManager* self)
{
    if (!self->m_pPartBottom) 
        return 0;

    IDirectDrawSurface7* pDst = self->m_pPartBottom->GetTargetSurface();
    IDirectDrawSurface7* pSrc = self->m_pFont->GetSurface();

    DDSURFACEDESC2 ddsdDst;
    memset(&ddsdDst, 0, sizeof(ddsdDst));
    ddsdDst.dwSize = sizeof(DDSURFACEDESC2);
    if (pDst->Lock(NULL, &ddsdDst, 0, NULL) != 0)
        return FALSE;

    DDSURFACEDESC2 ddsdSrc;
    memset(&ddsdSrc, 0, sizeof(ddsdSrc));
    ddsdSrc.dwSize = sizeof(DDSURFACEDESC2);
    if (pSrc->Lock(NULL, &ddsdSrc, 0, NULL) != 0) {
        pDst->Unlock(NULL);
        return FALSE;
    }

    if(ddsdDst.ddpfPixelFormat.dwRGBBitCount != 16 || ddsdSrc.lpSurface)
        return FALSE;

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

        char trunc;
        int n = cTextManager_DrawMenuTrunc_Hook(self, line, buf, &trunc);
        if (trunc) 
            self->m_pTruncFlags[line] = 1;

        int penX = MENU_LEFT_MARGIN;
        for (int chIdx = 0; chIdx < n; ) 
        {
            int chCode = getCharCode(buf, chIdx);
            RECT* rc = self->m_pFont->GetCharRect(chCode);
            if (rc) {
                int chW = rc->right - rc->left;
                int chH = rc->bottom - rc->top;

                if (chCode == '>') 
                    penX = self->m_nWidth30 - chW - MENU_ARROW_MARGIN;

                if (chW > 0 && chH > 0) {    
                    const WORD* s = srcBase + rc->top * srcPitchW + rc->left;
                          WORD* d = dstBase + penY * dstPitchW + penX;

                    for (int iy = 0; iy < chH; ++iy) {
                        for (int ix = 0; ix < chW; ++ix) {
                            WORD px = s[ix];
                            if (px) 
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

    pDst->Unlock(NULL);
    pSrc->Unlock(NULL);
    return 1;
}

// 5. china_pass_594a68d0 후킹 함수
cGlyphResult* __cdecl china_pass_Hook(cGlyphResult* out_result, cFont* pFont, const char* text, int textLen) 
{
    if (!out_result) {
        return NULL;
    }
    if (!text || textLen < 1 || !pFont) {
        out_result->charCode = 0;
        out_result->byteLength = 0;
        out_result->width = 0;
        out_result->height = 0;
        out_result->originalText[0] = '\0';
        return out_result;
    }

    // 텍스트가 2바이트 이상 남아있고, 한글인 경우
    if (textLen >= 2 && IsHangul((unsigned char)text[0], (unsigned char)text[1])) 
    {
        out_result->charCode = ((unsigned char)text[0] << 8) | (unsigned char)text[1];
        out_result->byteLength = 2;
        out_result->originalText[0] = text[0];
        out_result->originalText[1] = text[1];
        out_result->originalText[2] = '\0';
    } else {
        out_result->charCode = text[0];
        out_result->byteLength = 1;
        out_result->originalText[0] = text[0];
        out_result->originalText[1] = '\0';
    }
    
    RECT* pRect = pFont->GetCharRect(out_result->charCode);
    if (pRect != nullptr) {
        out_result->width = pRect->right - pRect->left;
        out_result->height = pRect->bottom - pRect->top;
    } else {
        out_result->width = 0;
        out_result->height = 0;
    }

    return out_result;
}