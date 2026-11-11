#include "por2.h"

#define LABEL_RIGHT_MARGIN 20        // 박스 폭 = (왼쪽 시작 + 글자폭 합) + 20

/* ------------------------------------------------------------------ */
/*  본체 (원본: thiscall, RET)                                           */
/* ------------------------------------------------------------------ */
BOOL __fastcall cTextLabel_CalcExtents_Hook(cTextLabel* self, void* /*edx*/)
{
    int maxLineHeight = 0;     // [esp+0x10]
    int maxBoxWidth = 0;     // [esp+0x14]

    for (int lineIdx = 0; lineIdx < self->m_nLineCount; ++lineIdx)
    {
        char* pLine = self->m_ppLines[lineIdx];
        int curX = self->m_nLeft;                  // 줄마다 m_nLeft 로 초기화
        int chIdx = 0;
        do {
            int chCode = getCharCode(pLine, chIdx);

            int chH = self->m_pFont->GetCharHeight(chCode);
            if (maxLineHeight < chH)
                maxLineHeight = chH;

            curX += self->m_pFont->GetCharWidth(chCode);
            if (maxBoxWidth < curX + LABEL_RIGHT_MARGIN)
                maxBoxWidth = curX + LABEL_RIGHT_MARGIN;

            chIdx += (0xFF < chCode) ? 2 : 1;   // 2바이트면 +2, 아니면 +1
        } while(pLine[chIdx - 1] != '\0' && pLine[chIdx] != '\0');   // empty line: stop after the NUL glyph (no read past terminator)
    }

    self->m_nLineHeight = maxLineHeight;
    self->m_nBoxWidth = maxBoxWidth;
    self->m_nTotalHeight = self->m_nLineCount * maxLineHeight;
    return TRUE;
}


