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


//============================================================================

/*

cTextManager::CalculateTextExtents (0x0043BF40) 복원 코드

이 함수는 앞의 두 함수와 구조가 다릅니다. WordWrap이 하나의 긴 문자열을 쪼개는 것과 달리, 이미 줄 단위로 나뉜 char* 배열(m_ppLines)을 받아 줄 높이, 박스 폭, 전체 높이를 계산합니다. 폭에는 좌우 여백이 붙고, 줄바꿈 계산은 하지 않습니다.

*/

#include <windows.h>

/* ------------------------------------------------------------------ */
/*  구조체 (제시하신 구조체는 오프셋이 일부 틀려서 수정했습니다)            */
/* ------------------------------------------------------------------ */
struct cFont { void** vtable; /* ... */ };

struct cTextManager {
    void* vftable;             // +0x000
    char   pad_004[0x138];      // +0x004 ~ +0x13B
    int    m_nLineCount;        // +0x13C
    int    m_nLineHeight;       // +0x140  가장 큰 글자 높이        (출력)
    int    m_nBoxWidth;         // +0x144  가장 긴 줄 폭 + 여백     (출력)
    int    m_nTotalHeight;      // +0x148  m_nLineCount * 줄 높이   (출력)
    char   pad_14C[0x4C];       // +0x14C ~ +0x197
    char** m_ppLines;           // +0x198  줄별 문자열 포인터 배열
    char   pad_19C[4];          // +0x19C
    cFont* m_pFont;             // +0x1A0
};

/* ------------------------------------------------------------------ */
/*  폰트 vtable 헬퍼 (이전 코드와 동일)                                    */
/*  +0x1C GetCharHeight, +0x20 GetCharWidth (thiscall, 인자 1개)         */
/* ------------------------------------------------------------------ */
typedef int(__fastcall* tFontCharFn)(cFont*, void*, int);

static __forceinline int Font_GetCharHeight(cFont* f, char ch)
{
    return ((tFontCharFn)f->vtable[0x1C / 4])(f, 0, (int)(unsigned char)ch);
}

static __forceinline int Font_GetCharWidth(cFont* f, char ch)
{
    return ((tFontCharFn)f->vtable[0x20 / 4])(f, 0, (int)(unsigned char)ch);
}

/* ------------------------------------------------------------------ */
/*  본체                                                                */
/* ------------------------------------------------------------------ */
#define BOX_PADDING_LEFT   10      // 줄 폭 계산의 시작값 (EBP = 0xA)
#define BOX_PADDING_RIGHT  0x14    // 최대폭 비교 시 더하는 값

BOOL __fastcall cTextManager_CalculateTextExtents_Hook(cTextManager* self, void* /*edx*/)
{
    int maxLineHeight = 0;     // [esp+0x10]
    int maxBoxWidth = 0;     // [esp+0x14]

    for (int lineIdx = 0; lineIdx < self->m_nLineCount; lineIdx++)
    {
        int currentLineWidth = BOX_PADDING_LEFT;       // 줄마다 10으로 초기화
        int charIdx = 0;

        /* 빈 줄이면 아무것도 하지 않고 건너뜀 */
        while (self->m_ppLines[lineIdx][charIdx] != '\0')
        {
            char ch = self->m_ppLines[lineIdx][charIdx];

            int charH = Font_GetCharHeight(self->m_pFont, ch);
            if (charH > maxLineHeight)
                maxLineHeight = charH;

            currentLineWidth += Font_GetCharWidth(self->m_pFont, ch);

            if (currentLineWidth + BOX_PADDING_RIGHT > maxBoxWidth)
                maxBoxWidth = currentLineWidth + BOX_PADDING_RIGHT;

            charIdx++;
        }
    }

    self->m_nLineHeight = maxLineHeight;
    self->m_nBoxWidth = maxBoxWidth;
    self->m_nTotalHeight = self->m_nLineCount * maxLineHeight;

    return TRUE;
}

/*
훅 예시 (MinHook)
typedef BOOL (__fastcall* tCalcExtents)(cTextManager*, void*);
tCalcExtents g_origCalcExtents = NULL;
MH_CreateHook((LPVOID)0x0043BF40, &cTextManager_CalculateTextExtents_Hook, (LPVOID*)&g_origCalcExtents);
MH_EnableHook((LPVOID)0x0043BF40);

디컴파일러가 잘못 읽은 부분
charHeight_Width 변수: 디컴파일러가 EAX를 하나의 변수로 합쳤습니다. 이 변수에 높이와 폭이 번갈아 들어가서 charHeight_Width = local_8 같은 어색한 대입이 생겼습니다. 실제로는 maxLineHeight(스택 [esp+0x10])와 maxBoxWidth([esp+0x14])가 독립적으로 유지됩니다. 마지막에 this+0x140에 저장되는 값도 마지막 글자의 폭이 아니라 최대 높이입니다.
szLine, c: 디컴파일러가 만든 임시 변수입니다. 실제로는 매번 m_ppLines[lineIdx][charIdx]를 다시 읽습니다.
dummy[n] 오프셋: 이 클래스는 dummy가 +0x04에서 시작합니다. 대응은 [0x4e]=+0x13C(줄 수), [0x4f]=+0x140(줄 높이), [0x50]=+0x144(박스 폭), [0x51]=+0x148(전체 높이), [0x65]=+0x198(줄 배열)입니다.
제시하신 구조체의 오류:
+0x140은 m_rcBounds_left가 아니라 **줄 높이(출력)**입니다.
+0x148은 m_rcBounds_top이 아니라 **전체 높이(출력)**입니다.
줄 배열은 +0x18C가 아니라 **+0x198**입니다. +0x18C로 두면 엉뚱한 곳을 읽습니다.
vtable+0x1C/+0x20: 앞의 함수들과 같은 cFont 슬롯(GetCharHeight/GetCharWidth)입니다. 이 함수는 폰트 포인터를 +0x1A0에서 읽습니다.
원본의 동작과 주의점
폭 계산: maxBoxWidth = 10 + 줄의 글자폭 합 + 0x14 = 합 + 30입니다. 왼쪽 10px, 오른쪽 20px 여백으로 보입니다. 두 상수의 용도는 호출부를 봐야 확실해집니다.
빈 줄만 있을 때: 폭 갱신이 글자 루프 안에서만 일어나므로, 모든 줄이 비어 있으면 폭은 30이 아니라 0이 됩니다. 줄 높이도 0이라 전체 높이는 0입니다.
줄 높이는 전역 최댓값입니다. 줄마다 따로 저장하지 않고 모든 줄이 같은 높이를 쓰며, 전체 높이는 줄 수 × 최대 높이입니다. 줄 간격은 없습니다.
글자 인자: 이전 함수와 같은 문제입니다. 원본은 AL만 교체한 EAX를 push하므로 상위 24비트에 줄 포인터 조각이 남습니다. 폰트 함수가 하위 바이트만 쓴다고 보고 unsigned char로 넘겼습니다. CP949 환경에서 폭이 이상하면 GetCharWidth 내부의 인자 처리를 확인해 보세요.
줄 수 필드: +0x13C는 이 클래스의 줄 수입니다. cTextLayout의 +0x138과는 다른 클래스이므로 혼동하지 않도록 하세요.
반환값: 항상 TRUE이고, 실패 경로는 없습니다.
*/


///============================================================================


/*
cTextManager::DrawTextLines (0x0043BC80) 복원 코드

이 함수는 디컴파일이 가장 심하게 깨져 있어서 어셈블리만 보고 복원했습니다. 디컴파일러가 그리기 루프 전체를 “unreachable”로 지운 것이 핵심 원인입니다. 복원 결과를 요약하면 다음과 같습니다.

대상 서피스와 폰트 서피스를 Lock합니다.
대상 서피스의 비트 수를 읽어 모드를 정합니다. 16bpp(모드 2)일 때만 실제로 그립니다.
줄마다 글자를 하나씩 그리고, 글자 픽셀 중 0이 아닌 것은 모두 고정 색 하나로 찍습니다. 원본 색 정보는 쓰지 않습니다.
글리프 아틀라스에서 대상으로 복사하므로 센터링과 그림자는 없습니다. 시작 x는 5px입니다.
*/


#include <windows.h>
#include <ddraw.h>

/* ------------------------------------------------------------------ */
/*  구조체 (asm에서 직접 확인한 오프셋만 기재)                             */
/* ------------------------------------------------------------------ */
struct cFont { void** vtable; /* ... */ };

struct cRenderTarget { void** vtable; /* +0x54 : 대상 서피스 반환 */ };

struct cTextManager {
    void* vftable;         // +0x000
    char           pad_004[0x13C];  // +0x004 ~ +0x13F
    int            m_nLineHeight;   // +0x140  줄 높이 (CalculateTextExtents 가 채운 값)
    char           pad_144[0x48];   // +0x144 ~ +0x18B
    char** m_ppLines;       // +0x18C  줄별 문자열 포인터 배열
    int            m_nLineCount;    // +0x190
    char           pad_194[0x0C];   // +0x194 ~ +0x19F
    cFont* m_pFont;         // +0x1A0
    char           pad_1A4[0x1C];   // +0x1A4 ~ +0x1BF
    cRenderTarget* m_pTarget;       // +0x1C0
};

/* ------------------------------------------------------------------ */
/*  게임 내부 함수 / vtable 헬퍼                                         */
/* ------------------------------------------------------------------ */
/* FUN_00436180(0x29, 0x9f, 0xdf) : 호출 직후 AX 를 16bit 픽셀로 사용.
   cdecl, 인자 3개. 이름은 임시 (RGB → 현재 픽셀 포맷 변환으로 추정). */
typedef WORD(__cdecl* tMakeColor16)(int a, int b, int c);
#define game_MakeColor16 ((tMakeColor16)0x00436180)

typedef int(__fastcall* tFontCharFn)(cFont*, void*, int);
typedef RECT* (__fastcall* tFontRectFn)(cFont*, void*, int);
typedef void* (__fastcall* tFontSurfFn)(cFont*, void*);
typedef IDirectDrawSurface7* (__fastcall* tTargetSurfFn)(cRenderTarget*, void*);

static __forceinline int Font_GetCharWidth(cFont* f, char ch)      // +0x20
{
    return ((tFontCharFn)f->vtable[0x20 / 4])(f, 0, (int)(unsigned char)ch);
}

static __forceinline RECT* Font_GetCharRect(cFont* f, char ch)     // +0x24 (원본: movsx)
{
    return ((tFontRectFn)f->vtable[0x24 / 4])(f, 0, (int)ch);
}

static __forceinline void* Font_GetSurface(cFont* f)               // +0x28
{
    return ((tFontSurfFn)f->vtable[0x28 / 4])(f, 0);
}

static __forceinline IDirectDrawSurface7* Target_GetSurface(cRenderTarget* t)  // +0x54
{
    return ((tTargetSurfFn)t->vtable[0x54 / 4])(t, 0);
}

/* ------------------------------------------------------------------ */
/*  본체                                                                */
/*  원본: RET 0x4 → 스택 인자 1개를 callee 가 정리한다 (함수 안에서는 안 읽음) */
/*  따라서 훅은 __fastcall(this, edx, int) 로 선언해야 스택이 맞는다.         */
/* ------------------------------------------------------------------ */
#define TEXT_LEFT_MARGIN  5
#define TEXT_COLOR_A      0x29
#define TEXT_COLOR_B      0x9F
#define TEXT_COLOR_C      0xDF

BOOL __fastcall cTextManager_DrawTextLines_Hook(cTextManager* self, void* /*edx*/,
    int /*unusedStackArg*/)
{
    int mode = 0;   // 원본은 초기화 안 된 스택 변수. 1=8bpp 2=16bpp 3=24bpp 4=32bpp

    DDSURFACEDESC2 ddsd;
    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize = 0x7C;

    IDirectDrawSurface7* pDst = Target_GetSurface(self->m_pTarget);
    IDirectDrawSurface7* pSrc = (IDirectDrawSurface7*)Font_GetSurface(self->m_pFont);

    /* ---- 대상 서피스 Lock (실패 시 아무것도 Unlock 하지 않고 반환) ---- */
    if (pDst->Lock(NULL, &ddsd, 0, NULL) != 0)
        return FALSE;

    BYTE* dstBase = (BYTE*)ddsd.lpSurface;
    int   dstPitchW = (int)((unsigned)ddsd.lPitch >> 1);

    switch (ddsd.ddpfPixelFormat.dwRGBBitCount) {
    case 8:  mode = 1; break;
    case 16: mode = 2; break;
    case 24: mode = 3; break;
    case 32: mode = 4; break;
    default: break;                 // mode 는 건드리지 않음
    }

    /* ---- 폰트 서피스 Lock (원본은 같은 ddsd 를 재사용하고 결과도 검사하지 않음) ---- */
    pSrc->Lock(NULL, &ddsd, 0, NULL);
    BYTE* srcBase = (BYTE*)ddsd.lpSurface;
    int   srcPitchW = (int)((unsigned)ddsd.lPitch >> 1);

    int penY = 0;

    for (int lineIdx = 0; lineIdx < self->m_nLineCount; lineIdx++)
    {
        int penX = TEXT_LEFT_MARGIN;
        int charIdx = 0;

        while (self->m_ppLines[lineIdx][charIdx] != '\0')
        {
            char  ch = self->m_ppLines[lineIdx][charIdx];
            RECT* rc = Font_GetCharRect(self->m_pFont, ch);

            int w = rc->right - rc->left;
            int h = rc->bottom - rc->top;

            /* 16bpp 에서만 그린다 (다른 모드는 폭 전진만 한다) */
            if (w > 0 && mode == 2 && h > 0)
            {
                const WORD* srcRow = (const WORD*)srcBase + (rc->top * srcPitchW + rc->left);
                WORD* dstRow = (WORD*)dstBase + (penY * dstPitchW + penX);

                for (int y = 0; y < h; y++)
                {
                    for (int x = 0; x < w; x++)
                    {
                        if (srcRow[x] != 0)         // 0 = 투명
                            dstRow[x] = game_MakeColor16(TEXT_COLOR_A, TEXT_COLOR_B, TEXT_COLOR_C);
                    }
                    srcRow += srcPitchW;
                    dstRow += dstPitchW;
                }
            }

            penX += Font_GetCharWidth(self->m_pFont, ch);
            charIdx++;
        }

        penY += self->m_nLineHeight;
    }

    /* Unlock 순서: 대상 → 폰트 */
    pDst->Unlock(NULL);
    pSrc->Unlock(NULL);
    return TRUE;
}

/*
훅 예시 (MinHook)
typedef BOOL (__fastcall* tDrawTextLines)(cTextManager*, void*, int);
tDrawTextLines g_origDrawTextLines = NULL;
MH_CreateHook((LPVOID)0x0043BC80, &cTextManager_DrawTextLines_Hook, (LPVOID*)&g_origDrawTextLines);
MH_EnableHook((LPVOID)0x0043BC80);

디컴파일러가 잘못 읽은 부분
RET 0x4: 어셈블리는 스택 인자 1개를 정리하며 반환하는데, 디컴파일러는 인자 없는 __thiscall로 봤습니다. 함수 안에서 이 인자를 읽는 코드는 없습니다. 그래도 훅의 시그니처에 int 하나를 넣어야 스택이 맞습니다. 0x0043C73F의 호출부에서 실제로 무엇을 push하는지는 확인해 두세요.
“Removing unreachable block” 경고: 디컴파일러는 [ESP+0x10](모드 변수)을 초기화되지 않은 값으로 보고 mode == 2 분기를 죽은 코드로 지웠습니다. 그 결과 그리기 루프 전체가 사라지고 GetCharRect, GetCharWidth 호출만 남았습니다. 실제로는 스위치에서 이 변수를 세팅하고 있고, 모드 2일 때 픽셀 복사 루프를 탑니다.
빈 switch(uStack_3c): uStack_3c는 ddsd.ddpfPixelFormat.dwRGBBitCount(스택 [ESP+0xA8])입니다. 점프 테이블은 bits - 8로 인덱싱하며, 바이트 테이블이 8→1, 16→2, 24→3, 32→4로 매핑합니다. 그 외 값은 default라서 모드를 건드리지 않습니다.
unaff_EBP, unaff_EBX 호출: 두 번째 Lock이 대상 서피스가 아니라 폰트 서피스([ESP+0x1C])에 호출된 것을 놓쳐서 생긴 오류입니다. 첫 Lock 결과는 [ESP+0x40](dst lpSurface)과 [ESP+0x50](dst pitch/2)에, 두 번째 결과는 [ESP+0x4C](src lpSurface)와 [ESP+0x28](src pitch/2)에 저장됩니다.
FUN_00436180 호출 누락: 디컴파일 결과에서는 이 호출 전체가 사라졌습니다. 원본은 비투명 픽셀마다 FUN_00436180(0x29, 0x9F, 0xDF)를 호출하고 반환 AX를 대상 픽셀에 씁니다. 이 함수는 RGB를 현재 픽셀 포맷으로 바꾸는 것으로 보이며, 글자색이 고정이라는 의미입니다.
EBP 재사용: 루프 안에서 EBP가 줄 인덱스에서 글자 폭(w)으로 바뀌었다가, [ESP+0x18]에서 다시 복원됩니다. 디컴파일러가 이를 한 변수로 합쳐서 줄 인덱스 흐름이 꼬였습니다.
필드 오프셋 (this->dummy[n]이 아니라 직접 오프셋으로 확인):
+0x140: 줄 높이
+0x18C: 줄 포인터 배열
+0x190: 줄 수
+0x1A0: 폰트
+0x1C0: 대상 서피스를 주는 객체(vtable +0x54)
원본의 동작과 주의점
구조체가 CalculateTextExtents와 다릅니다. 이전 함수는 줄 수가 +0x13C이고 줄 배열이 +0x198이었는데, 이 함수는 줄 수가 +0x190이고 줄 배열이 +0x18C입니다. 줄 높이(+0x140)와 폰트(+0x1A0)만 같습니다. 서로 다른 클래스이거나, 같은 클래스의 서로 다른 배열일 수 있습니다. 두 함수를 같은 구조체로 묶으면 안 됩니다.
16bpp만 지원합니다. 8, 24, 32bpp 서피스에서는 아무것도 그리지 않고, 폭 계산과 Unlock만 하고 TRUE를 반환합니다. 24와 32bpp 모드도 분기는 있지만 그리는 코드가 없습니다.
고정 색상입니다. 아틀라스 픽셀값은 “0이냐 아니냐”만 보고 버려지며, 모든 글자가 같은 색입니다. 원본 아틀라스 색을 쓰는 RenderShadow와는 다릅니다.
고정 여백입니다. x는 항상 5에서 시작합니다. CalculateTextExtents는 폭에 10 + 20을 더했으므로 좌우 여백이 비대칭입니다.
클리핑이 없습니다. 대상 서피스가 (5 + 최대 줄 폭) × (줄 수 × 줄 높이)보다 작으면 메모리가 깨집니다.
두 번째 Lock의 결과를 검사하지 않습니다. 실패하면 ddsd에 첫 Lock의 대상 서피스 값이 그대로 남아서 대상이 자기 자신을 읽게 됩니다. 이것도 그대로 복원했습니다.
Lock 실패 시 반환 경로: 첫 Lock이 실패하면 Unlock 없이 FALSE로 반환합니다. 폰트 서피스는 아직 Lock하지 않았으니 문제는 없습니다.
mode 초기화: 원본에서는 초기화되지 않은 스택 값입니다. 비트 수가 8, 16, 24, 32가 아니면 쓰레기 값이 그대로 남고 우연히 2가 될 가능성도 있습니다. 복원본에서는 0으로 초기화했습니다.
글자 인자: GetCharRect는 movsx(부호 확장)로, GetCharWidth는 AL만 교체된 레지스터를 push하므로 상위 24비트가 쓰레기입니다. 이전 함수들과 같은 방식으로 처리했습니다.
*/