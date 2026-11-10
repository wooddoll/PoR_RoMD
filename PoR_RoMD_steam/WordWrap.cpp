#include "por2.h"
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

/* ------------------------------------------------------------------ */
/*  int 배열 뒤에 값 하나 추가 (기존 new 로 만든 배열은 free 후 delete 함)   */
/*  nLineCount = 추가 될 배열 크기                                        */
/* ------------------------------------------------------------------ */
static void PushLineValue(int** ppArr, int nLineCount, int value)
{
    if (nLineCount <= 0)
        return;

    unsigned int cb = (unsigned int)nLineCount * 4;
    int* pNew = (int*)_malloc(cb);

    if (cb > 4 && *ppArr)
        memcpy(pNew, *ppArr, cb - 4);
    pNew[nLineCount - 1] = value;

    if (*ppArr)
        _free(*ppArr);
    *ppArr = pNew;
}

// 행말 금칙: 줄 끝에 올 수 없는 문자 (여는 괄호, 따옴표 등)
static inline bool IsOpenBracket(int c) {
    return c == '(' || c == '[' || c == '{' || c == '<' || c == '"' || c == '\'';
}

// 행두 금칙: 줄 시작에 올 수 없는 문자 (닫는 괄호, 따옴표, 마침표, 쉼표 등)
static inline bool IsCloseBracket(int c) {
    return c == ')' || c == ']' || c == '}' || c == '>' || c == '"' || c == '\'' ||
        c == ',' || c == '.' || c == '!' || c == '?' || c == ':' || c == ';';
}

/* ------------------------------------------------------------------ */
/*  WordWrap 객체                                                       */
/*  원본: BOOL __thiscall (원본 함수) 을 위해 __fastcall + dummy edx        */
/* ------------------------------------------------------------------ */
BOOL __fastcall cTextLayout_WordWrap_Hook(cTextLayout* self, void* /*edx*/)
{
    if (!self || !self->m_pszText || !self->m_pFont)
        return FALSE;

    int charIdx = 0;
    int nMaxWidth = 0;        // local_18 : 최대 줄 너비 값
    int nMaxLineHeight = 0;   // local_8
    int nCurrentLineWidth = 0;
    int nLineCount = 1;       // local_14
    int nLastWordIdx = 0;     // 마지막 "단어 시작" 인덱스
    int nLineStartIdx = 0;    // 현재 줄 시작 인덱스 (무한 루프 방지)

    int nBoxMaxWidth = self->GetMaxWidth();

    self->m_nMaxLineWidth = 0;
    self->m_nTotalHeight = 0;

    const char* _Text = self->m_pszText;

    int chCode = getCharCode(_Text, charIdx);
    if (chCode != '\0')
    {
        do {
            if (chCode == '\n') {
                /* 줄바꿈 처리: 현재 줄 확정 */
                // [버그 수정 3] charIdx == 0일 때 -1이 저장되어야 RenderShadow가 빈 줄을 정상 처리함
                PushLineValue(&self->m_pLineEndIndices, nLineCount, charIdx - 1);
                PushLineValue(&self->m_pLineWidths, nLineCount, nCurrentLineWidth);

                if (nMaxWidth < nCurrentLineWidth)
                    nMaxWidth = nCurrentLineWidth;

                nCurrentLineWidth = 0;
                nLineCount++;
                charIdx++;
                nLineStartIdx = charIdx;   // 새 줄 시작 인덱스 갱신
                nLastWordIdx = charIdx;
            }
            else {
                // 문장의 처음이거나 이전 문자가 공백인 경우
                if (!IsSpaceCh(chCode)) {
                    if (charIdx == 0 ||
                        (0 < charIdx && IsSpaceCh((uint8_t)_Text[charIdx - 1])))
                    {
                        nLastWordIdx = charIdx;
                    }
                }

                int charH = self->m_pFont->GetCharHeight(chCode);
                if (nMaxLineHeight < charH)
                    nMaxLineHeight = charH;

                nCurrentLineWidth += self->m_pFont->GetCharWidth(chCode);

                if (nBoxMaxWidth < nCurrentLineWidth) {
                    /* 단어 시작점에 도달했는지 확인: nLastWordIdx == nLineStartIdx 이면
                       한 단어에 글자가 많아서 폭을 넘는 것 => 글자 단위로 강제 줄바꿈 */
                    if (nLastWordIdx == nLineStartIdx) {
                        /* 글자 단위 강제 줄바꿈: 현재 글자를 제외하고 줄 저장 */
                        nCurrentLineWidth -= self->m_pFont->GetCharWidth(chCode);

                        /* 다음 줄 시작 위치. 괄호/기호 금칙 때문에 앞으로 당길 수 있음 */
                        int breakIdx = charIdx;

                        bool moved;
                        do {
                            moved = false;
                            /* 여는 괄호 바로 뒤에서는 끊지 않음 */
                            if (breakIdx - 1 > nLineStartIdx && IsOpenBracket((uint8_t)_Text[breakIdx - 1])) {
                                breakIdx -= 1;
                                moved = true;
                            }
                            /* 닫는 괄호/마침표 바로 앞에서는 끊지 않음 (직전 글자를 같이 넘김) */
                            else if (breakIdx > nLineStartIdx && IsCloseBracket((uint8_t)_Text[breakIdx])) {
                                int p = breakIdx - 1;
                                if (0 < p && IsHangul((uint8_t)_Text[p - 1], (uint8_t)_Text[p]))
                                    --p;                       // 직전 글자가 2바이트면 2바이트 이동
                                if (p > nLineStartIdx) {
                                    breakIdx = p;
                                    moved = true;
                                }
                            }
                        } while (moved);

                        /* 줄에서 빠지는 글자들의 폭을 뺀다 ([breakIdx, charIdx)) */
                        for (int k = breakIdx; k < charIdx; ) {
                            int code = getCharCode(_Text, k);
                            nCurrentLineWidth -= self->m_pFont->GetCharWidth(code);
                            k += (0xFF < code) ? 2 : 1;
                        }

                        if (nMaxWidth < nCurrentLineWidth)
                            nMaxWidth = nCurrentLineWidth;

                        // [버그 수정 1] charIdx - 1 대신 당겨진 breakIdx - 1을 저장해야 글자 중복 출력을 방지함
                        PushLineValue(&self->m_pLineEndIndices, nLineCount, breakIdx - 1);
                        PushLineValue(&self->m_pLineWidths, nLineCount, nCurrentLineWidth);

                        nCurrentLineWidth = 0;
                        nLineCount++;
                        charIdx = breakIdx;          // 줄 끝에서 빠진 글자부터 다시 처리
                        nLastWordIdx = breakIdx;
                        nLineStartIdx = breakIdx;
                    }
                    else {
                        /* 폭 초과: 마지막 단어 시작 위치까지 되돌아감 */
                        if (nLastWordIdx <= charIdx) {
                            do {
                                nCurrentLineWidth -= self->m_pFont->GetCharWidth(chCode);
                                --charIdx;
                                if (0 < charIdx) { // 이전 글자가 2바이트 문자인지 검사
                                    uint8_t c1 = static_cast<uint8_t>(_Text[charIdx - 1]);
                                    uint8_t c2 = static_cast<uint8_t>(_Text[charIdx]);
                                    if (IsHangul(c1, c2))   // 2바이트 중의 테일바이트이면 한바이트 더 이동
                                        --charIdx;
                                }
                                chCode = getCharCode(_Text, charIdx);
                            } while (nLastWordIdx <= charIdx);
                        }

                        /* 뒤따르는 공백 문자 제거 */
                        // [버그 수정 2] charIdx > nLineStartIdx 검사로 음수 언더플로우/크래시 방지
                        if (charIdx >= nLineStartIdx && IsSpaceCh(chCode)) {
                            do {
                                nCurrentLineWidth -= self->m_pFont->GetCharWidth(chCode);
                                if (charIdx <= nLineStartIdx)
                                    break;
                                --charIdx;
                                chCode = getCharCode(_Text, charIdx);
                            } while (IsSpaceCh(chCode));
                        }

                        if (nMaxWidth < nCurrentLineWidth)
                            nMaxWidth = nCurrentLineWidth;

                        PushLineValue(&self->m_pLineEndIndices, nLineCount, charIdx);
                        PushLineValue(&self->m_pLineWidths, nLineCount, nCurrentLineWidth);

                        nCurrentLineWidth = 0;
                        nLineCount++;
                        charIdx = nLastWordIdx;       // 다음 줄 단어 시작점부터
                        nLineStartIdx = charIdx;
                    }
                }
                else {
                    charIdx += (0xFF < chCode) ? 2 : 1;   // 2바이트면 +2, 아니면 +1
                }
            }

            chCode = getCharCode(_Text, charIdx);
        } while (chCode != '\0');

        if (nMaxWidth < nCurrentLineWidth)
            nMaxWidth = nCurrentLineWidth;
    }

    /* 마지막 줄 확정 (끝 인덱스 = strlen) */
    PushLineValue(&self->m_pLineEndIndices, nLineCount, (int)strlen(_Text));

    if (nMaxLineHeight == 0)
        nMaxLineHeight = self->m_pFont->GetCharHeight('A'); // 안전 기본값

    self->m_nMaxLineWidth = nMaxWidth;
    self->m_nLineHeight = nMaxLineHeight;
    self->m_nTotalHeight = nLineCount * nMaxLineHeight;
    self->m_nLineCount = nLineCount;

    PushLineValue(&self->m_pLineWidths, nLineCount, nCurrentLineWidth);

    /* 옵션: 중앙 정렬이면 박스 최대 너비로 설정 */
    if (self->m_nAlign == 1)
        self->m_nMaxLineWidth = self->GetMaxWidth();

    /* 여백 2px */
    if (0 < self->m_nMaxLineWidth)
        self->m_nMaxLineWidth += 2;

    if (0 < self->m_nTotalHeight)
        self->m_nTotalHeight += 2;

    return TRUE;
}