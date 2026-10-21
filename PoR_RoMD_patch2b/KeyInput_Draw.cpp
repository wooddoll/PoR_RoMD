/*
CKeyInput::Draw (0x0043A4D0) 복원 코드

어셈블리 기준으로 복원했습니다. 이 함수는 텍스트 입력창을 그리는 가상 함수입니다. XREF가 00622e24(*)로 호출이 아닌 데이터 참조이기 때문입니다.

글자는 DirectDraw Blt로 한 글자씩 백버퍼에 찍습니다.
박스 폭을 넘으면 줄바꿈하거나, 한 줄 모드에서는 문자열 끝부분만 오른쪽 정렬로 보여줍니다.
커서는 Direct3D로 2px 폭의 반투명 사각형을 그리고, 알파를 시간 비례로 증감시켜 깜빡이게 합니다.
*/

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
static __forceinline RECT* GlyphRect(cFont* f, char ch)
{
    RECT* rc = f->GetCharRect(ch);
    if (!rc) 
        rc = f->GetCharRect(' ');

    return rc;
}

/* ------------------------------------------------------------------ */
/*  FUN_004366e0 : 배경 이미지를 클리핑해서 그림 (thiscall, 인자 없음)        */
/*  0x84 / 0x88 = 위젯의 X / Y 오프셋을 돌려주는 가상 함수로 보임 (확인 필요)  */
/* ------------------------------------------------------------------ */
typedef int(__fastcall* tVirtGet)(CKeyInput*, void*);
typedef void(__fastcall* tImageDraw)(void*, void*, int x, int y, int arg, RECT* src, int flag);

static BOOL CKeyInput_DrawBackground(CKeyInput* self)
{
    if (!self->m_bVisible || !self->m_pImage || !self->m_nImageArg)
        return FALSE;

    void** vt = *(void***)self;
    int ox = ((tVirtGet)vt[0x84 / 4])(self, 0);      // 원본은 사용할 때마다 호출하지만 값은 같다고 가정
    int oy = ((tVirtGet)vt[0x88 / 4])(self, 0);

    RECT vis, tmp, src;
    IntersectRect(&vis, &self->m_rcClip, &self->m_rcScreen);

    tmp.left = ox + self->m_rcFrame.left;
    tmp.top = oy + self->m_rcFrame.top;
    tmp.right = ox + self->m_rcFrame.right;
    tmp.bottom = oy + self->m_rcFrame.bottom;
    IntersectRect(&src, &self->m_rcClip, &tmp);
    src.top -= oy;  src.bottom -= oy;
    src.left -= ox; src.right -= ox;

    if (src.bottom >= 0 && src.right >= 0 &&
        vis.bottom >= 0 && vis.top < 600 && vis.left < 800 && vis.right >= 0)
    {
        void** imgVt = *(void***)self->m_pImage;
        ((tImageDraw)imgVt[0x1C / 4])(self->m_pImage, 0, vis.left, vis.top, self->m_nImageArg, &src, 1);
        return TRUE;
    }
    return FALSE;
}

/* ------------------------------------------------------------------ */
/*  FUN_0043aba0 : 커서 사각형 그리기 (cdecl, 인자 5개)                     */
/*  호출부가 ADD ESP,0x14 로 정리하므로 5개. 디컴파일은 마지막 color 를 놓침     */
/* ------------------------------------------------------------------ */
struct TLVertex { float x, y, z, rhw; DWORD color; };      // 0x14 바이트

static void __cdecl DrawCursorQuad(int x, int y, int w, int h, DWORD color)
{
    IDirect3DDevice3* dev = g_pGame->pGfx->pD3DDevice;
    float x0 = (float)x, y0 = (float)y;
    float x1 = (float)(x + w), y1 = (float)(y + h);

    TLVertex v[4];
    memset(v, 0, sizeof(v));                        // z = 0, rhw = 0 (원본 그대로)
    v[0].x = x0; v[0].y = y0;
    v[1].x = x1; v[1].y = y0;
    v[2].x = x0; v[2].y = y1;
    v[3].x = x1; v[3].y = y1;
    for (int i = 0; i < 4; i++) v[i].color = color; // ※ 원본에서 확인 필요 (아래 참고)

    WORD idx[6] = { 0, 1, 2, 1, 3, 2 };
    dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, D3DFVF_XYZRHW | D3DFVF_DIFFUSE,
        v, 4, idx, 6, D3DDP_DONOTCLIP | D3DDP_DONOTLIGHT);   // flags 0x14
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
    fx.dwSize = 100;
    back->Blt(&dst, glyph, src, DDBLT_KEYSRC | DDBLT_WAIT, &fx);       // flags 0x1008000
}

/* ------------------------------------------------------------------ */
/*  본체  (원본: thiscall, RET 인자 없음)                                  */
/* ------------------------------------------------------------------ */
BOOL __fastcall CKeyInput_Draw_Hook(CKeyInput* self, void* /*edx*/)
{
    if (self->m_nBgFlag)
        CKeyInput_DrawBackground(self);

    if (!self->m_pszText || !self->m_bVisible)
        return TRUE;

    char* text = self->m_pszText;
    IDirectDrawSurface7* pGlyph = self->m_pFont->GetSurface();
    IDirectDrawSurface7* pBack = g_pGame->pGfx->pBackBuffer;

    /* ---- 전체 폭 측정 ---- */
    int len = (int)strlen(text);
    int totalWidth = 0;
    for (int i = 0; i < len; i++) {
        RECT* rc = GlyphRect(self->m_pFont, text[i]);
        totalWidth += rc->right - rc->left;
    }

    const int fontHeight = self->m_pFont->GetCharHeight('A');
    double* cursorIncBlink = (double*)0x00621FC0;
    double* cursorDecBlink = (double*)0x00622BB8;

    int currX = 0;           // [esp+0x1c] 커서 x 에도 쓰임
    int currY = 0;           // [esp+0x18] 커서 y 에도 쓰임

    if (self->m_bRightAlign && totalWidth > self->m_nBoxW)
    {
        /* ===== 한 줄 모드: 끝에서부터 역방향으로, 오른쪽 정렬 ===== */
        int remain = self->m_nBoxW - 2;
        int idx = len - 1;
        currX = remain;                       // 루프 중 갱신되지 않음 → 커서는 박스 오른쪽 끝

        do {
            RECT* rc = GlyphRect(self->m_pFont, text[idx]);
            int width = rc->right - rc->left;
            int height = rc->bottom - rc->top;
            remain -= width;
            BltGlyph(pBack, pGlyph, rc,
                self->m_nX + self->m_nPadX + remain,
                self->m_nY + self->m_nPadY, width, height);
            idx--;
        } while (remain > 0 && idx >= 0);
        /* 이 경로에서는 줄바꿈 캐시 정리를 건너뛴다 */
    }
    else
    {
        /* ===== 일반 모드: 박스 안에서 줄바꿈하며 왼쪽부터 ===== */
        int  lastSpace = -1;                 // [esp+0x28]
        int* p = self->m_aBreakCache;

        if (len < self->m_nStopIdx)
            self->m_nStopIdx = -1;

        if (len != 0)
        {
			for (int i = 0; i < len; ++i) 
            {
                if (self->m_nStopIdx != -1 && i >= self->m_nStopIdx)
                    break;

                if (IsSpaceCh(text[i]))
                    lastSpace = i;

                RECT* rc = GlyphRect(self->m_pFont, text[i]);
                int width = rc->right - rc->left;
                int height = rc->bottom - rc->top;

                if (i == *p)
                {
                    /* 캐시된 줄바꿈 위치: 여기서부터 끝까지가 현재 줄에 안 들어가면 줄바꿈 재현 */
                    int  acc = 0;
                    bool overflow = false;
                    for (int j = i; j < len; j++) {
                        RECT* r2 = GlyphRect(self->m_pFont, text[j]);
                        acc += r2->right - r2->left;
                        if (acc + currX > self->m_nBoxW) { 
                            overflow = true; 
                            break; 
                        }
                    }
                    if (overflow) {
                        currY += height;
                        currX = 0;
                        i = *p;          // (= i)
                        p++;
                        continue;       // 공백 글자는 그리지 않고 건너뜀
                    }
                    *p = -1;                 // 다 들어가므로 캐시 해제 후 정상 처리
                }

                if (currX + width > self->m_nBoxW)
                {
                    /* 폭 초과 → 새 줄 */
                    currY += height;
                    if (currY + height > self->m_nBoxH) {
                        currY -= height;                       // 세로도 넘침: 여기서 중단
                        self->m_nStopIdx = i;
                    }
                    else {
                        currX = 0;
                        self->m_nStopIdx = -1;
                        if (lastSpace != -1) {
                            i = lastSpace;               // 마지막 공백으로 되돌아감
                            lastSpace = -1;
                            *p++ = i;                    // 캐시에 기록
                        }
                        else {
                            i--;                         // 공백이 없으면 같은 글자를 새 줄에서 재시도
                        }
                    }
                }
                else
                {
                    BltGlyph(pBack, pGlyph, rc,
                        self->m_nX + self->m_nPadX + currX,
                        self->m_nY + self->m_nPadY + currY, width, height);
                    currX += width;
                }
            }
        }

        /* 캐시 중 문자열 길이 이상인 항목 무효화 */
        for (int k = 0; k < 20; k++) {
            if (len <= self->m_aBreakCache[k]) {
                self->m_aBreakCache[k] = -1;
            }
        }
    }

    /* ===== 커서 ===== */
    if (self->m_bCursorOn)
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

        DrawCursorQuad(self->m_nPadX + self->m_nX + currX,
            self->m_nPadY + self->m_nY + currY,
            2, fontHeight, color);

        /* 알파 페이드: 방향 0 = 증가, 1 = 감소 */
        if (!self->m_bCursorFalling) {
            self->m_nCursorAlpha += (int)((double)dt * *cursorIncBlink);
            if (self->m_nCursorAlpha >= 0xFF) {
                self->m_nCursorAlpha = 0xFF;
                self->m_bCursorFalling = 1;
            }
        }
        else {
            self->m_nCursorAlpha += (int)((double)dt * *cursorDecBlink);
            if (self->m_nCursorAlpha <= 0) {
                self->m_nCursorAlpha = 0;
                self->m_bCursorFalling = 0;
            }
        }

        dev->EndScene();
        dev->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, FALSE);
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

DAT_00727290이 비어 있는 것은 정상입니다. 실행 파일 이미지에는 0으로 있다가 게임 초기화 때 객체 포인터가 채워지는 전역 변수입니다. 코드에서는 *(GameGlobal**)0x00727290처럼 호출 시점에 읽으면 됩니다. 이 함수가 쓰는 오프셋은 +0x04(tick), +0x18(그래픽 컨텍스트), 그 안의 +0x34(백버퍼), +0x40(D3D 디바이스)뿐입니다.

이전 추정에서 확정, 정정된 부분
구조체 오프셋: 이전 디컴파일 표기(pad_004[n], this[1].pad_004[n])는 부정확했습니다. 위 구조체가 어셈블리 기준입니다.
행렬: 월드, 뷰, 프로젝션 행렬은 모두 단위행렬입니다. FPU 코드는 dt × double 상수를 __ftol한 값을 알파에 더하는 것이었습니다.
FUN_004366e0: 부모 그리기가 아니라 클리핑된 배경 이미지 Blt입니다. 800×600 화면 범위를 하드코딩해서 검사합니다.
FUN_0043aba0: 커서 그리기가 맞습니다. 2px 폭, 'A'의 높이를 가진 사각형(삼각형 2개)을 그리고, cdecl 인자 5개(x, y, w, h, color)입니다.
vtable+0x14: DirectDraw Blt, flags 0x1008000은 DDBLT_KEYSRC | DDBLT_WAIT가 맞습니다. D3D 슬롯도 Device3 기준으로 일치합니다. 0x24=BeginScene, 0x28=EndScene, 0x58=SetRenderState, 0x64=SetTransform, 0x74=DrawIndexedPrimitive, 0xA0=SetTextureStageState입니다.
커서 위치: 일반 모드에서는 마지막 글자 바로 뒤이고, 한 줄 모드에서는 박스 오른쪽 끝(boxW-2)입니다.
원본의 동작과 주의점
줄바꿈 캐시(+0x15C, 20칸): 줄바꿈 때 공백 인덱스를 저장해 둡니다. 다음 프레임에 그 인덱스에 도달하면 남은 문자열 전체가 현재 줄에 안 들어가는지 보고, 그렇다면 줄바꿈을 재현합니다. 들어가면 캐시를 지웁니다. 캐시가 없는 첫 프레임에는 넘치는 단어의 앞부분이 이전 줄에 먼저 그려질 수 있는데, 다음 프레임부터 캐시로 바로잡힙니다. 이 부분은 코드 구조에서 추정한 것이고, 실제로 보이는지는 확인하지 못했습니다.
캐시 오버런: 줄이 20개를 넘으면 p가 배열을 벗어나 +0x1AC(알파) 등을 덮어씁니다. 박스 높이 제한이 있어 보통은 문제없지만, 박스가 아주 크면 위험합니다.
한 줄 모드의 클리핑 없음: 맨 왼쪽 글자는 remain <= 0이 되는 순간에도 그려지므로 박스 왼쪽 밖으로 삐져나갈 수 있습니다.
dt 초기값: m_dwLastTick은 커서가 켜져 있을 때만 갱신됩니다. 커서를 켠 첫 프레임의 dt는 매우 클 수 있고, 알파는 바로 0xFF나 0으로 클램프됩니다.
2바이트 문자: 바이트 단위로 GetCharRect를 호출하므로 CP949 같은 멀티바이트 문자는 이전 함수들과 같은 문제가 있습니다.
반환값: 항상 TRUE입니다. Blt와 D3D 호출의 반환값은 모두 무시합니다.
아직 확정하지 못한 것
FUN_0043aba0의 색상 처리: 디컴파일에는 정점 색상을 쓰는 코드가 전혀 없습니다. 호출부는 (alpha<<24)|0xFFFFFF를 5번째 인자로 넘기므로 정점 diffuse에 쓰일 가능성이 높아서, 위 코드에서는 그렇게 복원했습니다. 어셈블리를 올려주시면 확정하겠습니다.
0x00621FC0, 0x00622BB8의 double 값: 깜빡임 속도 상수입니다. 코드에서는 원본 주소를 직접 읽도록 했습니다. 값을 알려주시면 이름 붙인 상수로 바꿀 수 있습니다. 감소 쪽(0x00622BB8)은 음수일 것입니다.
vtable +0x84 / +0x88의 의미: FUN_004366e0에서 오프셋 X/Y로 쓰입니다. 정확한 이름은 이 슬롯 함수를 보고 정해야 합니다.
m_nBgFlag(+0x124): 이 값이 무엇을 뜻하는지는 아직 모릅니다. 이 필드를 쓰는 곳(XREF)을 보면 알 수 있습니다.

*/