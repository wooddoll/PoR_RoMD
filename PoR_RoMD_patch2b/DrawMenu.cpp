#include "por2.h"

#define MENU_MAX_TEXT_W   140     // 이보다 넓으면 "..." 로 자름
#define MENU_LEFT_MARGIN  5
#define MENU_ARROW_MARGIN 5


BOOL __fastcall cTextManager_DrawMenu_Hook(cTextManager* self, void* /*edx*/)
{
    IDirectDrawSurface7* pDst = self->m_pPartBottom->GetTargetSurface();
    IDirectDrawSurface7* pSrc = self->m_pFont->GetSurface();

    int lineIdx = 0;
    int penY = 0;
    int penX = MENU_LEFT_MARGIN;

    /* ---- 대상 Lock (실패 시 바로 FALSE, Unlock 없음) ---- */
    DDSURFACEDESC2 ddsdDst;
    memset(&ddsdDst, 0, sizeof(ddsdDst));
    ddsdDst.dwSize = sizeof(DDSURFACEDESC2);
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

    /* ---- 폰트 Lock ---- */
    DDSURFACEDESC2 ddsdSrc;
    memset(&ddsdSrc, 0, sizeof(ddsdSrc));
    ddsdSrc.dwSize = sizeof(DDSURFACEDESC2);
    pSrc->Lock(NULL, &ddsdSrc, 0, NULL);
    BYTE* srcBase = (BYTE*)ddsdSrc.lpSurface;
    int   srcPitchW = (int)((unsigned)ddsdSrc.lPitch >> 1);

    /* 32bpp 경로 전용 러닝 포인터 (원본 버그: 글자마다 누적되고 rect/penX/penY 무시) */
    BYTE* dstRun = dstBase;
    BYTE* srcRun = srcBase;

    for (; lineIdx < self->m_nLineCountB; lineIdx++)
    {
        penX = MENU_LEFT_MARGIN;

        WORD lineColor = self->m_pSelFlags[lineIdx] ? self->m_wSelColor
            : self->m_wNormalColor;

        /* 줄 복사 (원본은 길이 제한 없음 → 여기서는 255바이트로 제한) */
        char buf[256];
        strncpy_s(buf, sizeof(buf), self->m_ppLinesB[lineIdx], 255);
        buf[255] = 0;

        /* ---- 폭 누적: 140px 초과 시 마지막 3글자를 "..." 로 교체 ---- */
        int n = 0;                                 // 그릴 바이트 수
        if (buf[0])
        {
            int lineW = 0, charIdx = 0;
            for (;;) {
				int charCode = getCharCode(buf, charIdx);
                lineW += self->m_pFont->GetCharWidth(charCode);
                if (lineW > MENU_MAX_TEXT_W) {
                    self->m_pTruncFlags[lineIdx] = 1;
                    if (charIdx >= 3) {
                        int s = charIdx - 3;

                        {
                            int i = 0, st = 0;       // s 이하의 마지막 문자 시작 위치
							while (i <= s) { 
                                st = i; 
                                i += (IsDBCSLeadByte((BYTE)buf[i]) && buf[i + 1]) ? 2 : 1; 
                            }
                            s = st;
                        }

                        buf[s] = buf[s + 1] = buf[s + 2] = '.';
                        n = s + 3;
                    }
                    else {
                        n = charIdx;                     // 원본은 buf[-1..] 를 덮어씀 → 여기서는 생략
                    }
                    break;
                }
                charIdx += (0xFF < charCode) ? 2 : 1;   // 2바이트면 +2, 아니면 +1
                n = charIdx;
                if (!buf[charIdx])
                    break;
            }
        }

        /* ---- 글자 그리기 ---- */
        for (int charIdx = 0; charIdx < n;)
        {
            int charCode = getCharCode(buf, charIdx);
            RECT* rc = self->m_pFont->GetCharRect(charCode);
            int   left = rc->left, top = rc->top;
            int   charW = rc->right - rc->left;
            int   charH = rc->bottom - rc->top;

            if (charCode == '>')
                penX = self->m_nWidth30 - charW - MENU_ARROW_MARGIN;

            if (charW > 0)
            {
                if (mode == 2)                                     /* 16bpp: 정상 경로 */
                {
                    if (charH > 0)
                    {
                        const WORD* s = (const WORD*)srcBase + (top * srcPitchW + left);
                        WORD* d = (WORD*)dstBase + (penY * dstPitchW + penX);
                        for (int iy = 0; iy < charH; iy++) {
                            for (int ix = 0; ix < charW; ix++) {
                                WORD px = s[ix];
                                if (px != 0) {
                                    if (px == 0xFFFF) px = lineColor;
                                    d[ix] = px;
                                }
                            }
                            s += srcPitchW;
                            d += dstPitchW;
                        }
                    }
                }
            }

            penX += self->m_pFont->GetCharWidth(charCode);
            charIdx += (0xFF < charCode) ? 2 : 1;   // 2바이트면 +2, 아니면 +1
        }

        penY += self->m_nLineHeight;
    }

    /* Unlock 순서: 대상 → 폰트 */
    pDst->Unlock(NULL);
    pSrc->Unlock(NULL);
    return TRUE;
}

/*
typedef BOOL (__fastcall* tDrawMenu)(cTextManager*, void*);
tDrawMenu g_origDrawMenu = NULL;
MH_CreateHook((LPVOID)0x0043B4F0, &cTextManager_DrawMenu_Hook, (LPVOID*)&g_origDrawMenu);
MH_EnableHook((LPVOID)0x0043B4F0);

이 함수는 가상 함수(vtable 0x622C78, 0x622D40)입니다. 인라인 훅이면 두 vtable 모두 적용됩니다. vtable 슬롯을 교체한다면 두 곳 다 바꾸세요.
*/