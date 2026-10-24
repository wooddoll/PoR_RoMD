#include "por2.h"

#include <string.h>
#define WIN32_LEAN_AND_MEAN // 불필요한 Windows 헤더 제외 (선택 사항)
#include <windows.h>
#define DIRECTDRAW_VERSION 0x0700
#define DIRECT3D_VERSION   0x0600
#include <ddraw.h>
#include <d3d.h>

/* ------------------------------------------------------------------ */
/*  구조체 (모든 오프셋은 어셈블리에서 확인)                              */
/* ------------------------------------------------------------------ */

struct GfxContext {                  // [DAT_00727290 + 0x18]
    char                 pad_00[0x34];
    IDirectDrawSurface7* pBackBuffer;   // +0x34
    char                 pad_38[0x08];
    IDirect3DDevice3*    pD3DDevice;    // +0x40
};

struct GameGlobal {                  // *(void**)0x00727290, 런타임에 채워짐
    char        pad_00[4];
    DWORD       dwTick;              // +0x04 매 프레임 갱신되는 ms 시각
    char        pad_08[0x10];
    GfxContext* pGfx;                // +0x18
};
#define g_pGame (*(GameGlobal**)0x00727290)

/* GetCharRect 가 NULL 이면 공백 글리프로 대체 (원본 동작) */
static __forceinline RECT* GlyphRect(cFont* f, int ch)
{
    RECT* rc = f->GetCharRect(ch);
    if (!rc) {
        rc = f->GetCharRect(' ');
    }

    return rc;
}

/* ------------------------------------------------------------------ */
/*  FUN_004366e0 : 배경 이미지를 클리핑해서 그림 (thiscall, 인자 없음)        */
/*  0x84 / 0x88 = 위젯의 X / Y 오프셋을 돌려주는 가상 함수로 보임 (확인 필요)  */
/* ------------------------------------------------------------------ */
typedef void(__stdcall* tImageDraw)(void* image, int x, int y, int arg, 
                                    const RECT* src, int flag);

static BOOL CKeyInput_DrawBackground(CWidgetBase* self)
{
    if (!self->m_bVisible || !self->m_pImage || !self->m_nImageArg)
        return FALSE;

    IDirectDrawSurface7* back = g_pGame->pGfx->pBackBuffer;
    DDSURFACEDESC2 ddsd;
    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    back->GetSurfaceDesc(&ddsd);
    const int Width  = (int)ddsd.dwWidth;
    const int Height = (int)ddsd.dwHeight;

    int ox = self->GetOffsetX();
    int oy = self->GetOffsetY();

    RECT vis, tmp, src;
    IntersectRect(&vis, &self->m_rcClip, &self->m_rcScreen);

    tmp.left = ox + self->m_rcFrame.left;
    tmp.top = oy + self->m_rcFrame.top;
    tmp.right = ox + self->m_rcFrame.right;
    tmp.bottom = oy + self->m_rcFrame.bottom;
    IntersectRect(&src, &self->m_rcClip, &tmp);
    src.top -= oy;  
    src.bottom -= oy;
    src.left -= ox; 
    src.right -= ox;

    if (src.bottom >= 0 && src.right >= 0 &&
        vis.bottom >= 0 && vis.top < Height && vis.left < Width && vis.right >= 0)
    {
        void** imgVt = *(void***)self->m_pImage;
        ((tImageDraw)imgVt[0x1C / 4])(self->m_pImage, vis.left, vis.top, self->m_nImageArg, &src, 1);
        return TRUE;
    }
    return FALSE;
}


/* ------------------------------------------------------------------ */
/*  FUN_0043aba0 : 커서 사각형 그리기 (cdecl, 인자 5개)                     */
/*  호출부가 ADD ESP,0x14 로 정리하므로 5개. 디컴파일은 마지막 color 를 놓침     */
/* ------------------------------------------------------------------ */
struct TLVertex { 
    float x, y, z, rhw; 
    DWORD color; 
};      // 0x14 바이트

static void __cdecl DrawCursorQuad(int x, int y, int w, int h, DWORD color)
{
    IDirect3DDevice3* dev = g_pGame->pGfx->pD3DDevice;
    float x0 = (float)x;
    float y0 = (float)y;
    float x1 = (float)(x + w);
    float y1 = (float)(y + h);

    TLVertex v[4];
    v[0].x = x0;    v[0].y = y0;   v[0].z = 0;  v[0].rhw = 1.f;
    v[1].x = x1;    v[1].y = y0;   v[1].z = 0;  v[1].rhw = 1.f;
    v[2].x = x0;    v[2].y = y1;   v[2].z = 0;  v[2].rhw = 1.f;
    v[3].x = x1;    v[3].y = y1;   v[3].z = 0;  v[3].rhw = 1.f;
    for (int i = 0; i < 4; i++) {
        v[i].color = color;
    }

    WORD idx[6] = { 0, 1, 2, 1, 3, 2 };
    dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 
                              D3DFVF_XYZRHW | D3DFVF_DIFFUSE,
                              v, 4, idx, 6, 
                              D3DDP_DONOTCLIP | D3DDP_DONOTLIGHT);   // flags 0x14
}

static void DrawCursor(CKeyInput* self, int currX, int currY, int fontHeight) 
{
    IDirect3DDevice3* dev = g_pGame->pGfx->pD3DDevice;
    dev->BeginScene();
    D3DMATRIX ident;
    memset(&ident, 0, sizeof(ident));
    ident._11 = ident._22 = ident._33 = ident._44 = 1.0f;
    dev->SetTransform(D3DTRANSFORMSTATE_WORLD, &ident);
    dev->SetTransform(D3DTRANSFORMSTATE_VIEW, &ident);
    dev->SetTransform(D3DTRANSFORMSTATE_PROJECTION, &ident);
    dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
    dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    dev->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, TRUE);
    dev->SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_SRCALPHA);
    dev->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_INVSRCALPHA);
    DWORD now = g_pGame->dwTick;
    int   dt = (int)(now - self->m_dwLastTick);
    self->m_dwLastTick = now;
    DWORD color = ((DWORD)self->m_nCursorAlpha << 24) | 0x00FFFFFF;   // 갱신 전 알파 사용
    DrawCursorQuad(self->m_nPadX + self->base.m_nX + currX, 
                   self->m_nPadY + self->base.m_nY + currY, 
                   2, fontHeight, color);

    const double IncBlink = *(double*)0x00621FC0;   // 커서 깜빡임 속도 상수
    const double DecBlink = *(double*)0x00622BB8;

    /* 알파 페이드: 방향 0 = 증가, 1 = 감소 */
    if (!self->m_bCursorFalling) {
        self->m_nCursorAlpha += (int)((double)dt * IncBlink);
        if (self->m_nCursorAlpha >= 0xFF) {
            self->m_nCursorAlpha = 0xFF;
            self->m_bCursorFalling = 1;
        }
    }
    else {
        self->m_nCursorAlpha += (int)((double)dt * DecBlink);
        if (self->m_nCursorAlpha <= 0) {
            self->m_nCursorAlpha = 0;
            self->m_bCursorFalling = 0;
        }
    }
    dev->EndScene();
    dev->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, FALSE);
}
/* ------------------------------------------------------------------ */
/*  글리프 한 개 Blt (컬러키 투명)                                        */
/* ------------------------------------------------------------------ */
static void BltGlyph(IDirectDrawSurface7* back, IDirectDrawSurface7* glyph,
                     RECT* src, int dx, int dy, int w, int h)
{
    RECT dst = { dx, dy, dx + w, dy + h };
    DDBLTFX fx;
    memset(&fx, 0, sizeof(fx));
    fx.dwSize = sizeof(DDBLTFX);
    back->Blt(&dst, glyph, src, DDBLT_KEYSRC | DDBLT_WAIT, &fx);       // flags 0x1008000
}

/* ------------------------------------------------------------------ */
/*  본체  (원본: thiscall, RET 인자 없음)                                  */
/* ------------------------------------------------------------------ */
BOOL __fastcall CKeyInput_Draw_Hook(CKeyInput* self, void* /*edx*/)
{
    if (self->m_nBgFlag)
        CKeyInput_DrawBackground(&self->base);

    if (!self->m_pszText || !self->base.m_bVisible)
        return TRUE;

    char* _Text = self->m_pszText;
    IDirectDrawSurface7* pGlyph = self->m_pFont->GetSurface();
    IDirectDrawSurface7* pBack = g_pGame->pGfx->pBackBuffer;

    /* ---- 전체 폭 측정 ---- */
    int text_len = (int)strlen(_Text);
    int totalWidth = 0;
    int charIdx = 0;
    do {
        int chCode = getCharCode(_Text, charIdx);
        totalWidth += self->m_pFont->GetCharWidth(chCode);
        charIdx += (0xFF < chCode) ? 2 : 1;   // 2바이트면 +2, 아니면 +1
    } while(charIdx < text_len);

    const int fontHeight = self->m_pFont->GetCharHeight('A');

    int currX = 0;           // 커서 x 에도 쓰임
    int currY = 0;           // 커서 y 에도 쓰임

    if (self->m_bRightAlign && self->m_nBoxW < totalWidth)
    {
        /* ===== 한 줄 모드: 끝에서부터 역방향으로, 오른쪽 정렬 ===== */
        currX = self->m_nBoxW - 2;  // 루프 중 갱신되지 않음 → 커서는 박스 오른쪽 끝
        int remain = currX;
        charIdx = text_len - 1;
        do {
            if (0 < charIdx) { // 이전 글자가 2바이트 문자인지 검사
                uint8_t c1 = static_cast<uint8_t>(_Text[charIdx - 1]);
                uint8_t c2 = static_cast<uint8_t>(_Text[charIdx]);
                if (IsHangul(c1, c2))   // 2바이트 중의 테일바이트이면 한바이트 더 이동
                    --charIdx;
            }
            int chCode = getCharCode(_Text, charIdx);
            RECT* rc = GlyphRect(self->m_pFont, chCode);
            int fontW = rc->right - rc->left;
            int fontH = rc->bottom - rc->top;
            remain -= fontW;
            BltGlyph(pBack, pGlyph, rc,
                    self->base.m_nX + self->m_nPadX + remain,
                    self->base.m_nY + self->m_nPadY, fontW, fontH);
            
            --charIdx;
        } while(0 <= charIdx && 0 < remain);
        /* 이 경로에서는 줄바꿈 캐시 정리를 건너뛴다 */
    }
    else
    {
        /* ===== 일반 모드: 박스 안에서 줄바꿈하며 왼쪽부터 ===== */
        if (text_len < self->m_nStopIdx)
            self->m_nStopIdx = -1;

        if (text_len != 0)
        {
            int lastSpace = -1;
            int lineIdx = 0;

            charIdx = 0;
            do {
                if (self->m_nStopIdx != -1 && self->m_nStopIdx <= charIdx)
                    break;

                int chCode = getCharCode(_Text, charIdx);
                if (IsSpaceCh(chCode))
                    lastSpace = charIdx;
                
                RECT* rc = GlyphRect(self->m_pFont, chCode);
                int fontW = rc->right - rc->left;
                int fontH = rc->bottom - rc->top;

                if (charIdx == self->m_aBreakCache[lineIdx])
                {
                    /* 캐시된 줄바꿈 위치: 여기서부터 끝까지가 현재 줄에 안 들어가면 줄바꿈 재현 */
                    bool overflow = false;
                    
                    int  acc = 0;
                    int _chIdx = charIdx;
                    do {
                        int _chCode = getCharCode(_Text, _chIdx);
                        acc += self->m_pFont->GetCharWidth(_chCode);
                        if (acc + currX > self->m_nBoxW) { 
                            overflow = true; 
                            break; 
                        }
                        _chIdx += (0xFF < _chCode) ? 2 : 1;   // 2바이트면 +2, 아니면 +1
                    } while(_chIdx < text_len);

                    if (overflow && lineIdx < 19) {
                        currY += fontH;
                        currX = 0;
                        charIdx = self->m_aBreakCache[lineIdx];          // (= charIdx)
                        ++lineIdx;
                        continue;       // 공백 글자는 그리지 않고 건너뜀
                    }
                    self->m_aBreakCache[lineIdx] = -1;                 // 다 들어가므로 캐시 해제 후 정상 처리
                }

                if (self->m_nBoxW < currX + fontW)
                {
                    /* 폭 초과 → 새 줄 */
                    currY += fontH;
                    if (self->m_nBoxH < currY + fontH  || lineIdx == 19) {
                        currY -= fontH;                       // 세로도 넘침: 여기서 중단
                        self->m_nStopIdx = charIdx;
                    }
                    else {
                        currX = 0;
                        self->m_nStopIdx = -1;
                        if (lastSpace != -1) {
                            charIdx = lastSpace;               // 마지막 공백으로 되돌아감
                            lastSpace = -1;
                            self->m_aBreakCache[lineIdx] = charIdx;   // 캐시에 기록
                            ++lineIdx;
                        }
                        else {  // 공백이 없으면 같은 글자를 새 줄에서 재시도
                            --charIdx;
                            if (0 < charIdx) { // 이전 글자가 2바이트 문자인지 검사
                                uint8_t c1 = static_cast<uint8_t>(_Text[charIdx - 1]);
                                uint8_t c2 = static_cast<uint8_t>(_Text[charIdx]);
                                if (IsHangul(c1, c2))   // 2바이트 중의 테일바이트이면 한바이트 더 이동
                                    --charIdx;
                            }
                        }
                    }
                }
                else
                {
                    BltGlyph(pBack, pGlyph, rc,
                        self->base.m_nX + self->m_nPadX + currX,
                        self->base.m_nY + self->m_nPadY + currY, fontW, fontH);
                    currX += fontW;
                }

                chCode = getCharCode(_Text, charIdx);
                charIdx += (0xFF < chCode) ? 2 : 1;   // 2바이트면 +2, 아니면 +1
            } while(charIdx < text_len);
        }

        /* 캐시 중 문자열 길이 이상인 항목 무효화 */
        for (int lineIdx = 0; lineIdx < 20; ++lineIdx) {
            if (text_len <= self->m_aBreakCache[lineIdx]) {
                self->m_aBreakCache[lineIdx] = -1;
            }
        }
    }

    /* ===== 커서 ===== */
    if (self->m_bCursorOn) {
        DrawCursor(self, currX, currY, fontHeight);
    }

    return TRUE;
}

/*
훅 방법

이 함수는 vtable 슬롯(0x00622E24)에서 참조됩니다. 방법은 두 가지입니다.

// (1) 인라인 훅 (MinHook)
typedef BOOL(__fastcall* tKeyDraw)(CKeyInput*, void*);
tKeyDraw g_origKeyDraw = NULL;
MH_CreateHook((LPVOID)0x0043A4D0, &CKeyInput_Draw_Hook, (LPVOID*)&g_origKeyDraw);
MH_EnableHook((LPVOID)0x0043A4D0);

// (2) vtable 슬롯 교체: 다른 곳에서 직접 호출하지 않는다면 이쪽이 더 안전
DWORD old;
VirtualProtect((LPVOID)0x00622E24, 4, PAGE_READWRITE, &old);
*(void**)0x00622E24 = (void*)&CKeyInput_Draw_Hook;
VirtualProtect((LPVOID)0x00622E24, 4, old, &old);

*/