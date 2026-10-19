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