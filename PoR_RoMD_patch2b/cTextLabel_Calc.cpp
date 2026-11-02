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
        } while(pLine[chIdx] != '\0');
    }

    self->m_nLineHeight = maxLineHeight;
    self->m_nBoxWidth = maxBoxWidth;
    self->m_nTotalHeight = self->m_nLineCount * maxLineHeight;
    return TRUE;
}



/*
cTextLabel 측정 함수 (0x004428B0) 훅 코드

앞서 복원한 DrawText(0x442590)의 짝이 되는 측정 함수입니다. 줄 높이, 박스 폭, 전체 높이를 계산하며, cTextManager::CalculateTextExtents(0x43BF40)와 구조가 같습니다. 차이는 아래 세 가지입니다.

필드 오프셋이 다릅니다. 줄 수 +0x158, 줄 배열 +0x15C, 폰트 +0x164입니다.
줄 폭의 시작값이 하드코딩 10이 아니라 m_nLeft(+0x1A8) 입니다.
출력은 +0x144(줄 높이), +0x148(전체 높이), +0x14C(박스 폭)입니다.

DrawText가 줄 시작 x를 +0x1A8에서 읽으므로, 측정과 그리기가 같은 시작점을 씁니다. 두 함수의 vtable 슬롯(0x623038, 0x623040)도 8바이트 간격으로 같은 vtable 안에 있어 같은 클래스가 맞습니다.

호출 규약: this 외 스택 인자가 없고 끝이 일반 RET입니다. DrawText와 달리 RET 4가 아닙니다.
반환값: 항상 TRUE입니다.

설치

직접 CALL하는 곳이 없고 00623040(*)의 vtable 데이터 참조 하나뿐이므로 슬롯 교체가 가장 안전합니다.

// (1) vtable 슬롯 교체 
DWORD old;
VirtualProtect((LPVOID)0x00623040, 4, PAGE_READWRITE, &old);
*(void**)0x00623040 = (void*)&cTextLabel_CalcExtents_Hook;
VirtualProtect((LPVOID)0x00623040, 4, old, &old);

// (2) 인라인 훅 (MinHook) 
typedef BOOL(__fastcall* tCalcExt)(cTextLabel*, void*);
tCalcExt g_origCalcExt = NULL;
MH_CreateHook((LPVOID)0x004428B0, &cTextLabel_CalcExtents_Hook, (LPVOID*)&g_origCalcExt);
MH_EnableHook((LPVOID)0x004428B0);
*/