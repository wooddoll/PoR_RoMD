#include <windows.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  구조체                                                              */
/* ------------------------------------------------------------------ */
struct cFont_vtable;                     // 슬롯 접근만 필요하므로 void** 로 캐스팅해서 사용
struct cFont { cFont_vtable* vtable; /* ... */ };

struct cTextLayout {
    void* vftable;                 // +0x000
    char   pad_00[0x120];           // +0x004
    cFont* m_pFont;                 // +0x124
    char* m_pszText;               // +0x128
    char   pad_12C[4];              // +0x12C
    int    m_nUnknown130;           // +0x130  (==1 이면 최종 폭을 GetMaxWidth() 로 강제)
    char   pad_134[4];              // +0x134
    int    m_nLineCount;            // +0x138
    int* m_pLineEndIndices;       // +0x13C  줄별 끝 문자 인덱스
    int* m_pLineWidths;           // +0x140  줄별 픽셀 폭
    int    m_nMaxLineWidth;         // +0x144
    int    m_nTotalHeight;          // +0x148
    int    m_nLineHeight;           // +0x14C  (신규 확인: 가장 큰 글자 높이)

    int GetMaxWidth() {             // vtable +0x48
        typedef int(__thiscall* tGetMaxWidth)(void*);
        return ((tGetMaxWidth)(*(void***)this)[0x12])(this);
    }
};

/* ------------------------------------------------------------------ */
/*  게임 내부 함수 (원본 CRT/할당자를 그대로 써야 한다)                      */
/* ------------------------------------------------------------------ */
typedef void* (__cdecl* tOperatorNew)(unsigned int size);
typedef void(__cdecl* tOperatorDelete)(void* p);
typedef int(__cdecl* tIsSpace)(int c);

#define game_new     ((tOperatorNew)    0x005EA8FD)   // operator_new
#define game_delete  ((tOperatorDelete) 0x005EA8F2)   // delete_005ea8f2
#define game_isspace ((tIsSpace)        0x00445480)   // isspace_00445480

/* cFont vtable: +0x1C GetCharHeight, +0x20 GetCharWidth  (thiscall, 인자 1개) */
typedef int(__fastcall* tFontCharFn)(cFont* font, void* edx, int ch);

static __forceinline int Font_GetCharHeight(cFont* f, char ch)
{
    return ((tFontCharFn)((void**)f->vtable)[0x1C / 4])(f, 0, (int)(unsigned char)ch);
}
static __forceinline int Font_GetCharWidth(cFont* f, char ch)
{
    return ((tFontCharFn)((void**)f->vtable)[0x20 / 4])(f, 0, (int)(unsigned char)ch);
}
static __forceinline int IsSpaceCh(char ch)
{
    return game_isspace((int)(signed char)ch);      // 원본: movsx
}

/* ------------------------------------------------------------------ */
/*  int 배열 뒤에 값 하나 추가 (원본의 new → 복사 → 값 기록 → delete 패턴)   */
/*  nLineCount = 추가 후 원소 개수                                        */
/* ------------------------------------------------------------------ */
static void PushLineValue(int** ppArr, int nLineCount, int value)
{
    unsigned int cb = (unsigned int)nLineCount * 4;
    int* pNew = (int*)game_new(cb);

    if (cb > 4)
        memcpy(pNew, *ppArr, cb - 4);
    pNew[nLineCount - 1] = value;

    game_delete(*ppArr);
    *ppArr = pNew;
}

/* ------------------------------------------------------------------ */
/*  WordWrap 본체                                                       */
/*  원본: BOOL __thiscall (인자 없음) → 훅은 __fastcall + dummy edx        */
/* ------------------------------------------------------------------ */
#define TEXT (self->m_pszText)

BOOL __fastcall cTextLayout_WordWrap_Hook(cTextLayout* self, void* /*edx*/)
{
    int nCharIdx = 0;
    int nMaxWidth = 0;   // local_18 : 가장 긴 줄의 폭
    int nMaxLineHeight = 0;   // local_8
    int nCurrentLineWidth = 0;
    int nLineCount = 1;   // local_14
    int nLastSpaceIdx = 0;   // 마지막 "단어 시작" 인덱스

    int nBoxMaxWidth = self->GetMaxWidth();

    self->m_nMaxLineWidth = 0;
    self->m_nTotalHeight = 0;

    if (TEXT[0] != '\0')
    {
        do
        {
            char ch = TEXT[nCharIdx];

            if (ch == '\n')
            {
                /* 명시적 개행: 현재 줄 확정 */
                PushLineValue(&self->m_pLineEndIndices, nLineCount, nCharIdx - 1);
                PushLineValue(&self->m_pLineWidths, nLineCount, nCurrentLineWidth);

                if (nCurrentLineWidth > nMaxWidth)
                    nMaxWidth = nCurrentLineWidth;

                nCurrentLineWidth = 0;
                nLineCount++;
                nCharIdx++;
            }
            else
            {
                /* 단어 시작 위치 기록: 공백이 아니고, (맨 앞 or 앞 글자가 공백) */
                if (!IsSpaceCh(ch))
                {
                    if (nCharIdx == 0 ||
                        (nCharIdx > 0 && IsSpaceCh(TEXT[nCharIdx - 1])))
                    {
                        nLastSpaceIdx = nCharIdx;
                    }
                }

                int charH = Font_GetCharHeight(self->m_pFont, TEXT[nCharIdx]);
                if (charH > nMaxLineHeight)
                    nMaxLineHeight = charH;

                nCurrentLineWidth += Font_GetCharWidth(self->m_pFont, TEXT[nCharIdx]);

                if (nCurrentLineWidth > nBoxMaxWidth)
                {
                    /* 박스 폭 초과: 마지막 단어 시작 직전까지 되돌린다 */
                    if (nCharIdx >= nLastSpaceIdx)
                    {
                        do {
                            nCurrentLineWidth -= Font_GetCharWidth(self->m_pFont, TEXT[nCharIdx]);
                            nCharIdx--;
                        } while (nCharIdx >= nLastSpaceIdx);
                    }

                    /* 줄 끝의 공백 제거 */
                    if (IsSpaceCh(TEXT[nCharIdx]))
                    {
                        do {
                            nCurrentLineWidth -= Font_GetCharWidth(self->m_pFont, TEXT[nCharIdx]);
                            nCharIdx--;
                        } while (IsSpaceCh(TEXT[nCharIdx]));
                    }

                    if (nCurrentLineWidth > nMaxWidth)
                        nMaxWidth = nCurrentLineWidth;

                    PushLineValue(&self->m_pLineEndIndices, nLineCount, nCharIdx);
                    PushLineValue(&self->m_pLineWidths, nLineCount, nCurrentLineWidth);

                    nCurrentLineWidth = 0;
                    nLineCount++;
                    nCharIdx = nLastSpaceIdx;       // 다음 줄은 단어 시작부터
                }
                else
                {
                    nCharIdx++;
                }
            }
        } while (TEXT[nCharIdx] != '\0');

        if (nCurrentLineWidth > nMaxWidth)
            nMaxWidth = nCurrentLineWidth;
    }

    /* 마지막 줄 확정 (끝 인덱스 = strlen) */
    PushLineValue(&self->m_pLineEndIndices, nLineCount, (int)strlen(TEXT));

    self->m_nMaxLineWidth = nMaxWidth;
    self->m_nLineHeight = nMaxLineHeight;
    self->m_nTotalHeight = nLineCount * nMaxLineHeight;
    self->m_nLineCount = nLineCount;

    PushLineValue(&self->m_pLineWidths, nLineCount, nCurrentLineWidth);

    /* 옵션: 폭 고정 모드면 박스 최대폭 사용 */
    if (self->m_nUnknown130 == 1)
        self->m_nMaxLineWidth = self->GetMaxWidth();

    /* 여백 2px */
    if (self->m_nMaxLineWidth > 0) self->m_nMaxLineWidth += 2;
    if (self->m_nTotalHeight > 0) self->m_nTotalHeight += 2;

    return TRUE;
}
#undef TEXT



/*
cTextLayout::WordWrap (0x00444890) 복원 코드

Ghidra 디컴파일 결과와 어셈블리를 대조해서 복원했습니다. 디컴파일러가 오인한 부분은 어셈블리 기준으로 고쳤고, 그 내용은 코드 뒤에 정리했습니다.
*/

/*
// 훅 예시 (MinHook)
typedef BOOL (__fastcall* tWordWrap)(cTextLayout*, void*);
tWordWrap g_origWordWrap = NULL;
MH_CreateHook((LPVOID)0x00444890, &cTextLayout_WordWrap_Hook, (LPVOID*)&g_origWordWrap);
MH_EnableHook((LPVOID)0x00444890);
*/

/*

디컴파일러가 잘못 읽은 부분
GetMaxWidth 반환값: 디컴파일러는 byte로 봤지만 어셈블리는 mov [esp+0x34], eax로 32비트 int를 저장합니다.
바이트 복사 루프: for (charW_3 = 0; charW_3 != 0; ...)는 rep movsb가 잘못 풀린 것입니다. 복사 크기가 항상 4의 배수라 나머지 바이트는 없으므로 memcpy로 대체했습니다.
배열 추가 패턴: operator_new → 복사 → 끝에 값 기록 → 기존 배열 delete가 여러 번 반복되므로 PushLineValue 하나로 묶었습니다. 배열 크기는 nLineCount * 4 바이트이고 복사량은 크기 - 4입니다.
this[1].dummy[n] 오프셋: [4]=+0x13C, [5]=+0x140, [6]=+0x144, [7]=+0x148, [8]=+0x14C, [3]=+0x138, [1]=+0x130으로 확인했습니다. 구조체에는 +0x14C m_nLineHeight 필드를 추가해야 합니다.
폰트 vtable 호출: cFont_vtable은 클래스 멤버 함수로 호출하면 this가 틀어지므로, 반드시 폰트 객체를 ecx로 넘기는 방식으로 호출해야 합니다. 그래서 __fastcall 함수 포인터로 vtable 슬롯을 직접 호출했습니다.
원본의 동작과 주의점
문자 단위(바이트 단위)로 처리하므로 CP949 같은 2바이트 문자는 반으로 쪼개질 수 있습니다. 원본 동작이라 그대로 두었습니다.
한 단어가 박스 폭보다 길면 nCharIdx = nLastSpaceIdx로 같은 위치로 되돌아와 무한 루프에 빠집니다. 메모리도 계속 할당됩니다. 이는 원본의 버그입니다. 고치려면 줄바꿈 블록 진입 직후에 nLastSpaceIdx == 줄 시작 인덱스인지 확인해서, 그 경우 글자 단위로 강제 줄바꿈하도록 분기를 추가하면 됩니다. 줄 시작 인덱스를 추적하는 변수가 하나 더 필요합니다.
nLastSpaceIdx == 0인 상태에서 줄바꿈이 일어나면 TEXT[-1]을 읽게 됩니다. 이것도 원본과 동일하게 복원했습니다.
줄 끝 인덱스의 의미가 줄마다 다릅니다. 개행·자동 줄바꿈 줄은 "마지막 글자 인덱스(포함)"이고, 마지막 줄만 strlen(미포함)입니다. 이 배열을 소비하는 코드가 있다면 그 규칙에 맞춰야 합니다.
폰트 함수에 넘기는 인자는 원본에서 상위 3바이트가 쓰레기 값(포인터 일부)이지만, 하위 1바이트만 쓴다고 보고 unsigned char로 확장해서 넘겼습니다. 만약 크래시나 폭 오차가 보이면 GetCharWidth/GetCharHeight 내부의 인자 처리를 먼저 확인해 보세요.

*/


/////=============================================================================

#include <windows.h>
#include <ddraw.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  구조체 (WordWrap 때와 동일한 것에 필드만 추가)                          */
/* ------------------------------------------------------------------ */
struct cFont { void** vtable; /* ... */ };

struct cTextLayout {
    void* vftable;                 // +0x000
    char   pad_00[0x120];           // +0x004
    cFont* m_pFont;                 // +0x124
    char* m_pszText;               // +0x128
    char   pad_12C[4];              // +0x12C
    int    m_nUnknown130;           // +0x130  ==1 이면 줄 가운데 정렬
    char   pad_134[4];              // +0x134
    int    m_nLineCount;            // +0x138
    int* m_pLineEndIndices;       // +0x13C
    int* m_pLineWidths;           // +0x140
    int    m_nMaxLineWidth;         // +0x144
    int    m_nTotalHeight;          // +0x148
    int    m_nLineHeight;           // +0x14C
    WORD   m_wTextColor;            // +0x150  원본 픽셀이 0xFFFF 일 때 대체할 색
    char   pad_152[2];              // +0x152
    WORD   m_wShadowColor;          // +0x154
    char   pad_156[0x0E];           // +0x156 ~ +0x163
    BYTE   m_bShadow;               // +0x164  그림자 사용 여부

    /* vtable +0x54 : 그려 넣을 대상 서피스 반환 (포인터 전체 32bit) */
    IDirectDrawSurface7* GetTargetSurface() {
        typedef IDirectDrawSurface7* (__thiscall* tFn)(void*);
        return ((tFn)(*(void***)this)[0x54 / 4])(this);
    }
};

/* ------------------------------------------------------------------ */
/*  게임 내부 함수 / 폰트 vtable 헬퍼 (WordWrap 코드와 공유)               */
/* ------------------------------------------------------------------ */
typedef int(__cdecl* tIsSpace)(int c);
#define game_isspace ((tIsSpace)0x00445480)

typedef int(__fastcall* tFontCharFn)(cFont*, void*, int);
typedef RECT* (__fastcall* tFontRectFn)(cFont*, void*, int);
typedef void* (__fastcall* tFontSurfFn)(cFont*, void*);

static __forceinline int Font_GetCharWidth(cFont* f, char ch)      // +0x20
{
    return ((tFontCharFn)f->vtable[0x20 / 4])(f, 0, (int)(unsigned char)ch);
}

static __forceinline RECT* Font_GetCharRect(cFont* f, char ch)     // +0x24
{
    return ((tFontRectFn)f->vtable[0x24 / 4])(f, 0, (int)ch);
}      // 원본: movsx

static __forceinline void* Font_GetSurface(cFont* f)               // +0x28
{
    return ((tFontSurfFn)f->vtable[0x28 / 4])(f, 0);
}

static __forceinline int IsSpaceCh(char ch)
{
    return game_isspace((int)ch);
}                                  // 원본: movsx

/* ------------------------------------------------------------------ */
/*  RenderShadow 본체                                                   */
/* ------------------------------------------------------------------ */
#define TEXT (self->m_pszText)

BOOL __fastcall cTextLayout_RenderShadow_Hook(cTextLayout* self, void* /*edx*/)
{
    int charIdx = 0;     // [esp+0x10]
    int xOffset = 0;     // [esp+0x14]  현재 줄 안에서의 펜 x (센터링 오프셋 포함)
    int lineIdx = 0;     // [esp+0x18]
    int yAccum = 0;     // [esp+0x20]  현재 줄의 y 시작 (줄 높이 누적)

    IDirectDrawSurface7* pDst = self->GetTargetSurface();
    IDirectDrawSurface7* pSrc = (IDirectDrawSurface7*)Font_GetSurface(self->m_pFont);

    /* 첫 줄 가운데 정렬 오프셋 */
    if (self->m_nUnknown130 == 1)
        xOffset = (self->m_nMaxLineWidth - self->m_pLineWidths[0]) / 2;

    /* ---- 대상 서피스 Lock ---- */
    DDSURFACEDESC2 ddsdDst;
    memset(&ddsdDst, 0, sizeof(ddsdDst));
    ddsdDst.dwSize = 0x7C;
    if (pDst->Lock(NULL, &ddsdDst, 0, NULL) != 0)
        return FALSE;

    WORD* dstBase = (WORD*)ddsdDst.lpSurface;
    int   dstPitchW = (int)((unsigned)ddsdDst.lPitch >> 1);

    /* ---- 폰트(아틀라스) 서피스 Lock ---- */
    DDSURFACEDESC2 ddsdSrc;
    memset(&ddsdSrc, 0, sizeof(ddsdSrc));
    ddsdSrc.dwSize = 0x7C;
    if (pSrc->Lock(NULL, &ddsdSrc, 0, NULL) != 0)
    {
        pDst->Unlock(NULL);
        return FALSE;
    }

    WORD* srcBase = (WORD*)ddsdSrc.lpSurface;
    int   srcPitchW = (int)((unsigned)ddsdSrc.lPitch >> 1);

    while (lineIdx < self->m_nLineCount)
    {
        /* 이 줄에 그릴 글자가 있을 때만 (빈 줄이면 건너뜀) */
        if (charIdx <= self->m_pLineEndIndices[lineIdx])
        {
            int dstRowOffset = dstPitchW * yAccum;

            do
            {
                RECT* rc = Font_GetCharRect(self->m_pFont, TEXT[charIdx]);
                int w = rc->right - rc->left;
                int h = rc->bottom - rc->top;

                const WORD* srcRow = srcBase + (rc->top * srcPitchW + rc->left);
                WORD* dstRow = dstBase + (dstRowOffset + xOffset);

                for (int y = 0; y < h; y++)
                {
                    for (int x = 0; x < w; x++)
                    {
                        WORD px = srcRow[x];
                        if (px == 0)
                            continue;                       // 투명

                        if (px == 0xFFFF)
                            px = self->m_wTextColor;        // 흰색 → 글자색

                        dstRow[x] = px;

                        if (self->m_bShadow)                // 그림자: (+2, +2)
                            dstRow[x + dstPitchW * 2 + 2] = self->m_wShadowColor;
                    }
                    srcRow += srcPitchW;
                    dstRow += dstPitchW;
                }

                xOffset += Font_GetCharWidth(self->m_pFont, TEXT[charIdx]);
                charIdx++;

            } while (TEXT[charIdx] != '\0' &&
                charIdx <= self->m_pLineEndIndices[lineIdx]);
        }

        /* 줄 끝의 공백/개행 건너뛰기 (원본: 현재 글자가 공백일 때만 진입) */
        if (IsSpaceCh(TEXT[charIdx]))
        {
            do {
                charIdx++;
                if (TEXT[charIdx] == '\0')
                    break;
            } while (IsSpaceCh(TEXT[charIdx]));
        }

        /* 다음 줄 */
        lineIdx++;
        xOffset = 0;
        if (self->m_nUnknown130 == 1 && lineIdx < self->m_nLineCount)
            xOffset = (self->m_nMaxLineWidth - self->m_pLineWidths[lineIdx]) / 2;

        yAccum += self->m_nLineHeight;
    }

    pSrc->Unlock(NULL);
    pDst->Unlock(NULL);
    return TRUE;
}
#undef TEXT

// 훅 예시 (MinHook)
/*
typedef BOOL (__fastcall* tRenderShadow)(cTextLayout*, void*);
tRenderShadow g_origRenderShadow = NULL;
MH_CreateHook((LPVOID)0x00444D40, &cTextLayout_RenderShadow_Hook, (LPVOID*)&g_origRenderShadow);
MH_EnableHook((LPVOID)0x00444D40);
*/

/*
디컴파일러가 잘못 읽은 부분
vtable+0x54 반환값: 디컴파일러는 byte로 봤지만, 어셈블리는 MOV EBX,EAX로 32비트를 그대로 씁니다. 대상 IDirectDrawSurface7* 포인터입니다. GetMaxWidth(+0x48)와 같은 오인입니다.
vtable+0x28(폰트): cFont::GetSurface로, 아틀라스 서피스를 반환합니다. 디컴파일 결과에서는 이 반환값이 버려지고 unaff_ESI로 대체되어 있습니다.
Lock 호출 2개:
첫 번째는 [EDX+0x64], 즉 vtable 25번 Lock(NULL, &ddsd, 0, NULL)을 대상 서피스에 호출합니다.
두 번째 Lock은 같은 ddsd 버퍼를 재사용하며 폰트 서피스에 호출합니다. 이 때문에 디컴파일러가 iRam00000000 호출로 깨뜨렸습니다.
dwSize = 0x7C는 DDSURFACEDESC2입니다. 따라서 IDirectDrawSurface7이고, Lock은 vtable 0x64, Unlock은 0x80입니다.
원본은 첫 Lock 결과(lpSurface, lPitch/2)를 스택에 저장한 뒤 ddsd를 재사용합니다. 복원본에서는 구조체 두 개로 나눴으며 동작은 같습니다.
마지막 Unlock 2개: 디컴파일러는 piVar4, unaff_ESI로 표시했지만 실제로는 src와 dst 서피스입니다. 순서는 src → dst입니다.
psVar9 + uVar2 같은 포인터 연산: 디컴파일러가 ECX, EDX 레지스터 재사용을 변수 하나로 합쳐서 꼬였습니다. 어셈블리 기준으로는 아래 규칙입니다.
소스 행 포인터는 srcBase + top*srcPitchW + left이고, 행마다 srcPitchW씩 증가합니다.
대상 행 포인터는 dstBase + dstPitchW*yAccum + xOffset이고, 행마다 dstPitchW씩 증가합니다.
그림자 위치는 col + 2*dstPitchW + 2입니다.
필드 오프셋: dummy[9]=+0x150(글자색), dummy[10]=+0x154(그림자색), dummy[0xe]=+0x164(그림자 on/off, 바이트)로 확인했습니다. dummy[8]=+0x14C는 줄 높이입니다.
반환값: 성공이면 AL=1, 실패이면 AL=0입니다. 실패 시 상위 바이트에 HRESULT 잔여값이 남을 수 있으므로, 호출부는 AL만 보는 것으로 보입니다.
원본의 동작과 주의점
16bpp 전용: 픽셀을 WORD로 다루므로 RGB565/555 표면을 가정합니다. 0xFFFF는 글자색 치환 키이고, 0은 투명입니다.
클리핑이 없습니다: 대상 서피스가 m_nMaxLineWidth × m_nTotalHeight 이상이어야 합니다. 그림자가 +2px 밀려 나가는 것을 WordWrap의 +2 여백이 흡수합니다. 대상이 더 작으면 메모리가 깨집니다.
그림자 겹침: 그림자는 해당 픽셀을 찍은 직후에 같이 기록됩니다. 그래서 그림자가 이미 그려진 앞쪽 글자 픽셀을 덮을 수 있고, 뒤에 그려지는 글자가 다시 그림자를 덮습니다. 이것도 원본 동작입니다.
빈 줄 처리: \n이 연속되면 WordWrap이 lineEnd = charIdx-1을 넣기 때문에 charIdx > lineEnd가 되어 그리기를 건너뜁니다. 이후 공백 스킵에서 \n이 소비됩니다.
센터링 계산 차이: 가운데 정렬 오프셋은 (maxW - lineW) / 2입니다. m_nMaxLineWidth에는 이미 +2가 더해져 있으므로, 줄 폭이 최대폭과 같아도 오프셋이 1이 됩니다.
글자 인자: GetCharRect는 movsx(부호 확장)로 넘기지만, GetCharWidth는 AL만 교체한 레지스터라 상위 24비트가 포인터 쓰레기입니다. 그래서 GetCharWidth는 unsigned char로 넘겼습니다. 한글(CP949)처럼 0x80 이상 바이트에서 폭이나 그리기가 이상하면 이 부분을 먼저 확인해 보세요.
WordWrap과 연동: 이 함수는 WordWrap이 채운 m_pLineEndIndices(+0x13C), m_pLineWidths(+0x140), m_nLineHeight(+0x14C)를 그대로 읽습니다. 마지막 줄만 strlen(미포함) 규칙이지만, 이 함수는 TEXT[charIdx] != 0 검사를 같이 하므로 문제가 없습니다.
*/