#include "por2.h"
#include <string.h>

/* ------------------------------------------------------------------ */
/*  cTextManager 의 1바이트 출력 함수 두 개 대체                          */
/*    - FUN_0043b8e0 : 메뉴 마스크 (글자 모양대로 값 1 을 찍음)           */
/*    - FUN_0043f1b0 : 가로 배치 메뉴 (칸마다 첫 단어를 가운데 정렬)      */
/*  0x43b8e0 : sneg 등록 없음 -> 게임 입구를 직접 훅                     */
/*  0x43f1b0 : sneg FUN_59529c50 이 대체 (SNEG_Load 시점에 등록됨)        */
/*             -> sneg 함수를 훅, sneg 가 안 걸었으면 게임 입구를 훅       */
/* ------------------------------------------------------------------ */

int __cdecl cTextManager_DrawMenuTrunc_Hook(cTextManager* self, int lineIdx, char* buf, char* flag);

#define MENU_LEFT_MARGIN    5
#define TEMP_BUFF_SIZE      256

#define TM_COLUMN_WIDTH(tm) (*(int*)((BYTE*)(tm) + 0x1DC))   // 가로 메뉴 칸 폭
#define TM_COLUMN_BYTES(tm) (*(int*)((BYTE*)(tm) + 0x1E0))   // 가로 메뉴 칸당 최대 바이트 수

namespace {

struct LockedSurface {
    IDirectDrawSurface7* pSurf = nullptr;
    DDSURFACEDESC2 ddsd;
    bool locked = false;

    bool Lock(IDirectDrawSurface7* s) {
        pSurf = s;
        memset(&ddsd, 0, sizeof(ddsd));
        ddsd.dwSize = sizeof(ddsd);
        locked = (s && s->Lock(NULL, &ddsd, 0, NULL) == DD_OK && ddsd.lpSurface);
        return locked;
    }
    void Unlock() {
        if (locked) pSurf->Unlock(NULL);
        locked = false;
    }
    WORD* Base() const     { return (WORD*)ddsd.lpSurface; }
    int   PitchW() const   { return (int)((unsigned)ddsd.lPitch >> 1); }
    int   Width() const    { return (int)ddsd.dwWidth; }
    int   Height() const   { return (int)ddsd.dwHeight; }
    bool  Is16bpp() const  { return ddsd.ddpfPixelFormat.dwRGBBitCount == 16; }
};

/* 글리프 하나를 (x, y) 에 찍는다. 원본 픽셀이 0 이 아니면 color 로 칠함.
   원본 함수에는 없지만 대상 서피스 밖으로 나가는 픽셀은 잘라낸다.        */
void BlitGlyphSolid(const LockedSurface& dst, const LockedSurface& src,
                    const RECT* rc, int x, int y, WORD color)
{
    const int chW = rc->right - rc->left;
    const int chH = rc->bottom - rc->top;
    if (chW <= 0 || chH <= 0)
        return;

    const int dstPitch = dst.PitchW();
    const int srcPitch = src.PitchW();
    for (int iy = 0; iy < chH; ++iy) {
        const int dy = y + iy;
        if (dy < 0 || dy >= dst.Height())
            continue;
        const WORD* s = src.Base() + (rc->top + iy) * srcPitch + rc->left;
        WORD*       d = dst.Base() + dy * dstPitch;
        for (int ix = 0; ix < chW; ++ix) {
            const int dx = x + ix;
            if (dx < 0 || dx >= dst.Width())
                continue;
            if (s[ix] != 0)
                d[dx] = color;
        }
    }
}

} // namespace

/* ------------------------------------------------------------------ */
/*  FUN_0043b8e0 대체 (thiscall, 인자 없음, RET)                         */
/*  원본: m_ppLinesB 각 줄을 x=5 부터, 줄마다 m_nLineHeight 씩 내려가며    */
/*        글자 모양대로 m_pPartBottom 에 값 1 을 찍는다. 폭 140 을 넘으면  */
/*        원본 문자열 자체에 "..." 를 써 넣었다.                          */
/*  변경: 문자열은 건드리지 않고, DrawMenu 훅과 같은                       */
/*        cTextManager_DrawMenuTrunc_Hook 으로 복사본을 잘라서 쓴다.      */
/*        (그래야 마스크와 DrawMenu 가 그린 글자가 같은 위치에 맞는다)     */
/*  원본처럼 '>' 오른쪽 정렬은 하지 않음. 16bpp 만 처리 (원본 32bpp 경로는 */
/*  좌표 계산이 없어서 사실상 동작하지 않음).                              */
/* ------------------------------------------------------------------ */
BOOL __fastcall cTextManager_DrawMenuMask_Hook(cTextManager* self, void* /*edx*/)
{
    if (!self || !self->m_pPartBottom || !self->m_pFont || !self->m_ppLinesB)
        return FALSE;

    LockedSurface dst, src;
    if (!dst.Lock(self->m_pPartBottom->GetTargetSurface()))
        return FALSE;
    if (!src.Lock(self->m_pFont->GetSurface())) {
        dst.Unlock();
        return FALSE;
    }

    if (dst.Is16bpp()) {
        char buf[TEMP_BUFF_SIZE];
        int penY = 0;
        for (int line = 0; line < self->m_nLineCountB; ++line) {
            char trunc = 0;
            const int n = cTextManager_DrawMenuTrunc_Hook(self, line, buf, &trunc);

            int penX = MENU_LEFT_MARGIN;
            for (int chIdx = 0; chIdx < n; ) {
                const int chCode = getCharCode(buf, chIdx);
                BlitGlyphSolid(dst, src, self->m_pFont->GetCharRect(chCode), penX, penY, 1);
                penX += self->m_pFont->GetCharWidth(chCode);
                chIdx += (0xFF < chCode) ? 2 : 1;
            }
            penY += self->m_nLineHeight;
        }
    }

    dst.Unlock();
    src.Unlock();
    return TRUE;
}

/* ------------------------------------------------------------------ */
/*  FUN_0043f1b0 대체 (thiscall, 스택 인자 1개 미사용, RET 4)             */
/*  원본: 항목 i 마다 첫 단어(공백/NUL 또는 칸당 최대 바이트 수까지)의     */
/*        폭 W 를 구하고 x = 칸폭*i + 칸폭/2 - W/2, y = 0 에 출력.         */
/*        선택된 항목은 m_wSelColor, 나머지는 m_wNormalColor.             */
/*  변경: EUC-KR 2바이트 글자 단위로 읽고, 글자가 최대 바이트 경계에       */
/*        걸리면 자르지 않고 그 앞에서 멈춘다.                            */
/* ------------------------------------------------------------------ */
BOOL __fastcall cTextManager_DrawHMenu_Hook(cTextManager* self, void* /*edx*/, int /*unusedStackArg*/)
{
    if (!self || !self->m_pPartBottom || !self->m_pFont || !self->m_ppLinesB)
        return FALSE;

    LockedSurface dst, src;
    if (!dst.Lock(self->m_pPartBottom->GetTargetSurface()))
        return FALSE;
    if (!src.Lock(self->m_pFont->GetSurface())) {
        dst.Unlock();
        return FALSE;
    }

    if (dst.Is16bpp()) {
        const int colW     = TM_COLUMN_WIDTH(self);
        const int maxBytes = TM_COLUMN_BYTES(self);
        cFont* font = self->m_pFont;

        for (int item = 0; item < self->m_nLineCountB; ++item) {
            const char* text = self->m_ppLinesB[item];
            if (!text)
                continue;

            /* 1. 출력할 바이트 수와 폭 */
            int n = 0;
            int textW = 0;
            while (n < maxBytes) {
                const uint8_t c1 = static_cast<uint8_t>(text[n]);
                if (c1 == 0 || IsSpaceCh(static_cast<signed char>(c1)))
                    break;
                const int chCode = getCharCode(text, n);
                const int chLen = (0xFF < chCode) ? 2 : 1;
                if (n + chLen > maxBytes)
                    break;
                textW += font->GetCharWidth(chCode);
                n += chLen;
            }

            /* 2. 가운데 정렬 (원본과 같이 산술 시프트) */
            int penX = colW * item + (colW >> 1) - (textW >> 1);
            const WORD color = (self->m_pSelFlags && self->m_pSelFlags[item])
                ? self->m_wSelColor : self->m_wNormalColor;

            /* 3. 출력 */
            for (int chIdx = 0; chIdx < n; ) {
                const int chCode = getCharCode(text, chIdx);
                BlitGlyphSolid(dst, src, font->GetCharRect(chCode), penX, 0, color);
                penX += font->GetCharWidth(chCode);
                chIdx += (0xFF < chCode) ? 2 : 1;
            }
        }
    }

    dst.Unlock();
    src.Unlock();
    return TRUE;
}
