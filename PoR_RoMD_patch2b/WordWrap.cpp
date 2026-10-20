#include "por2.h"
#include <string.h>

#define WIN32_LEAN_AND_MEAN // 불필요한 Windows 헤더 제외 (선택 사항)
#include <windows.h>        // COM 및 기본 Windows API 정의 (ddraw.h보다 먼저 포함되어야 함)

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

    if (cb > 4)
        memcpy(pNew, *ppArr, cb - 4);
    pNew[nLineCount - 1] = value;

    _free(*ppArr);
    *ppArr = pNew;
}

/* ------------------------------------------------------------------ */
/*  WordWrap 객체                                                       */
/*  원본: BOOL __thiscall (원본 함수) 을 위해 __fastcall + dummy edx        */
/* ------------------------------------------------------------------ */
#define _Text (self->m_pszText)

BOOL __fastcall cTextLayout_WordWrap_Hook(cTextLayout* self, void* /*edx*/)
{
    int charIdx = 0;
    int nMaxWidth = 0;   // local_18 : 최대 줄 너비 값
    int nMaxLineHeight = 0;   // local_8
    int nCurrentLineWidth = 0;
    int nLineCount = 1;   // local_14
    int nLastWordIdx = 0;   // 마지막 "단어 시작" 인덱스
    int nLineStartIdx = 0;   // 현재 줄 시작 인덱스 (무한 루프 방지)

    int nBoxMaxWidth = self->GetMaxWidth();

    self->m_nMaxLineWidth = 0;
    self->m_nTotalHeight = 0;

    int chCode = getCharCode(_Text, charIdx);
    if (chCode != '\0')
    {
        do {
            if (chCode == '\n') {
                /* 줄바꿈 처리: 현재 줄 확정 */
                PushLineValue(&self->m_pLineEndIndices, nLineCount, charIdx - 1);   // WARN!!: 문장의 시작위치에 '\n'이 있으면 -1 위치를 읽게 됨
                PushLineValue(&self->m_pLineWidths, nLineCount, nCurrentLineWidth);

                if (nMaxWidth < nCurrentLineWidth)
                    nMaxWidth = nCurrentLineWidth;

                nCurrentLineWidth = 0;
                nLineCount++;
                charIdx++;
                nLineStartIdx = charIdx;   // 새 줄 시작 인덱스 갱신
            }
            else {
                // 문장의 처음이거나 이전 문자가 공백인 경우
                if (!IsSpaceCh(chCode)) {
                    if (charIdx == 0 ||
                        (0 < charIdx && IsSpaceCh(_Text[charIdx - 1])))
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

                        if (nMaxWidth < nCurrentLineWidth)
                            nMaxWidth = nCurrentLineWidth;

                        PushLineValue(&self->m_pLineEndIndices, nLineCount, charIdx - 1);
                        PushLineValue(&self->m_pLineWidths, nLineCount, nCurrentLineWidth);

                        nCurrentLineWidth = 0;
                        nLineCount++;
                        nLastWordIdx = charIdx;    // 마지막 단어와 
                        nLineStartIdx = charIdx;   // 다음 줄은 현재 nCharIdx에서 시작
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
                        // nCharIdx는 nLastWordIdx보다 1바이트 앞을 가르킴. 
                        // nLastWordIdx는 공백의 다음 위치이므로 현재 nCharIdx는 항상 공백문자 위치

                        /* 뒤따르는 공백 문자 제거 */
                        if (IsSpaceCh(chCode)) {
                            do {
                                nCurrentLineWidth -= self->m_pFont->GetCharWidth(chCode);
                                --charIdx; // 공백문자는 1바이트라서 2바이트 이동이 아님
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
#undef _Text
