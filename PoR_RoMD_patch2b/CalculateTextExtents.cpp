#include "por2.h"

#define WIN32_LEAN_AND_MEAN // 불필요한 Windows 헤더 제외 (선택 사항)
#include <windows.h>        // COM 및 기본 Windows API 정의 (ddraw.h보다 먼저 포함되어야 함)

#define BOX_PADDING_LEFT   10   // 줄 폭 계산의 시작값 (EBP = 0xA)
#define BOX_PADDING_RIGHT  20   // 최대폭 비교 시 더하는 값

BOOL __fastcall cTextManager_CalculateTextExtents_Hook(cTextManager* self, void* /*edx*/)
{
    int maxLineHeight = 0;     // [esp+0x10]
    int maxBoxWidth = 0;     // [esp+0x14]

    for (int lineIdx = 0; lineIdx < self->m_nLineCountB; ++lineIdx)
    {
        int currentLineWidth = BOX_PADDING_LEFT;       // 줄마다 10으로 초기화
        int charIdx = 0;

        /* 빈 줄이면 아무것도 하지 않고 건너뜀 */
        while (self->m_ppLinesB[lineIdx][charIdx] != '\0')
        {
            int chCode = getCharCode(self->m_ppLinesB[lineIdx], charIdx);

            int charH = self->m_pFont->GetCharHeight(chCode);
            if (maxLineHeight < charH)
                maxLineHeight = charH;

            currentLineWidth += self->m_pFont->GetCharWidth(chCode);

            if (maxBoxWidth < (currentLineWidth + BOX_PADDING_RIGHT))
                maxBoxWidth = currentLineWidth + BOX_PADDING_RIGHT;

            charIdx += (0xFF < chCode) ? 2 : 1;   // 2바이트면 +2, 아니면 +1
        }
    }

    self->m_nLineHeight = maxLineHeight;
    self->m_nBoxWidth = maxBoxWidth;
    self->m_nTotalHeight = self->m_nLineCount * maxLineHeight;

    return TRUE;
}
