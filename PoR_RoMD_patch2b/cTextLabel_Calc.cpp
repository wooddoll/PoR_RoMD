

/*
cTextLabel 측정 함수 (0x004428B0) 훅 코드

앞서 복원한 DrawText(0x442590)의 짝이 되는 측정 함수입니다. 줄 높이, 박스 폭, 전체 높이를 계산하며, cTextManager::CalculateTextExtents(0x43BF40)와 구조가 같습니다. 차이는 아래 세 가지입니다.

필드 오프셋이 다릅니다. 줄 수 +0x158, 줄 배열 +0x15C, 폰트 +0x164입니다.
줄 폭의 시작값이 하드코딩 10이 아니라 m_nLeft(+0x1A8) 입니다.
출력은 +0x144(줄 높이), +0x148(전체 높이), +0x14C(박스 폭)입니다.

DrawText가 줄 시작 x를 +0x1A8에서 읽으므로, 측정과 그리기가 같은 시작점을 씁니다. 두 함수의 vtable 슬롯(0x623038, 0x623040)도 8바이트 간격으로 같은 vtable 안에 있어 같은 클래스가 맞습니다.

호출 규약: this 외 스택 인자가 없고 끝이 일반 RET입니다. DrawText와 달리 RET 4가 아닙니다.
반환값: 항상 TRUE입니다.

*/

#include <windows.h>
#include <stddef.h>

/* ------------------------------------------------------------------ */
/*  구조체: DrawText 때의 cTextLabel 정의에서 pad_148 부분만 확정         */
/* ------------------------------------------------------------------ */
struct cFont { void** vtable; };

struct cTextLabel {                            // vtable 0x00623038 / 0x00623040 ...
    void* vftable;                    // +0x000
    char           pad_004[0x140];             // +0x004
    int            m_nLineHeight;              // +0x144  (출력) 가장 큰 글자 높이
    int            m_nTotalHeight;             // +0x148  (출력) 줄 수 × 줄 높이
    int            m_nBoxWidth;                // +0x14C  (출력) 가장 긴 줄의 끝 x + 20
    char           pad_150[8];                 // +0x150 ~ +0x157
    int            m_nLineCount;               // +0x158
    char** m_ppLines;                  // +0x15C
    char           pad_160[4];                 // +0x160
    cFont* m_pFont;                    // +0x164
    WORD           m_wColor;                   // +0x168
    char           pad_16A[0x0A];              // +0x16A
    void* m_pTarget;                  // +0x174
    char           pad_178[0x30];              // +0x178
    int            m_nLeft;                    // +0x1A8
};
static_assert(offsetof(cTextLabel, m_nLineHeight) == 0x144, "");
static_assert(offsetof(cTextLabel, m_nTotalHeight) == 0x148, "");
static_assert(offsetof(cTextLabel, m_nBoxWidth) == 0x14C, "");
static_assert(offsetof(cTextLabel, m_nLineCount) == 0x158, "");
static_assert(offsetof(cTextLabel, m_ppLines) == 0x15C, "");
static_assert(offsetof(cTextLabel, m_pFont) == 0x164, "");
static_assert(offsetof(cTextLabel, m_pTarget) == 0x174, "");
static_assert(offsetof(cTextLabel, m_nLeft) == 0x1A8, "");

/* ------------------------------------------------------------------ */
/*  폰트 vtable 헬퍼 (+0x1C GetCharHeight, +0x20 GetCharWidth)            */
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

#define LABEL_RIGHT_MARGIN 0x14        // 박스 폭 = (왼쪽 시작 + 글자폭 합) + 20

/* ------------------------------------------------------------------ */
/*  본체 (원본: thiscall, RET)                                           */
/* ------------------------------------------------------------------ */
BOOL __fastcall cTextLabel_CalcExtents_Hook(cTextLabel* self, void* /*edx*/)
{
    int maxLineHeight = 0;     // [esp+0x10]
    int maxBoxWidth = 0;     // [esp+0x14]

    for (int lineIdx = 0; lineIdx < self->m_nLineCount; lineIdx++)
    {
        int curX = self->m_nLeft;                  // 줄마다 m_nLeft 로 초기화

        for (int ci = 0; self->m_ppLines[lineIdx][ci] != '\0'; ci++)
        {
            char ch = self->m_ppLines[lineIdx][ci];

            int h = Font_GetCharHeight(self->m_pFont, ch);
            if (h > maxLineHeight)
                maxLineHeight = h;

            curX += Font_GetCharWidth(self->m_pFont, ch);
            if (curX + LABEL_RIGHT_MARGIN > maxBoxWidth)
                maxBoxWidth = curX + LABEL_RIGHT_MARGIN;
        }
    }

    self->m_nLineHeight = maxLineHeight;
    self->m_nBoxWidth = maxBoxWidth;
    self->m_nTotalHeight = self->m_nLineCount * maxLineHeight;
    return TRUE;
}



/*
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

원본의 동작과 주의점
빈 줄과 줄 수 0: 빈 줄은 폭과 높이에 기여하지 않습니다. 모든 줄이 비어 있거나 줄이 0개이면 높이, 폭, 전체 높이가 모두 0입니다.
높이는 전역 최댓값: 줄마다 따로 저장하지 않습니다. DrawText가 +0x144 하나로 모든 줄을 내려가는 것과 맞습니다.
폭은 마지막 글자 기준: 줄 끝 x에 20을 더한 값의 최댓값입니다. 왼쪽 여백(m_nLeft)은 이미 포함되어 있습니다.
글자 인자: 원본은 AL만 교체한 EAX를 push하므로 상위 24비트에 포인터 조각이 남습니다. 폰트 함수가 하위 바이트만 쓴다고 보고 unsigned char로 넘겼습니다. 이 점은 이전 함수들과 같습니다.
2바이트 문자 관련

이 함수는 DrawText와 한 쌍이므로, 폭 계산 방식이 DrawText의 글자 전진 방식과 정확히 같아야 합니다. 한쪽만 lead+trail 합친 글리프로 고치면 박스 폭이 실제 출력과 어긋납니다. DrawText에서 GetCharRect와 GetCharWidth를 부르는 곳과, 이 함수의 GetCharHeight와 GetCharWidth를 부르는 곳을 같은 문자 단위(1바이트 또는 2바이트)로 순회하도록 맞춰야 합니다. 고치신 방식을 알려 주시면 두 함수에 같이 반영하겠습니다.

지금까지 정리된 텍스트 함수 대응표
클래스	측정	그리기	줄 목록
cTextLayout	WordWrap (0x444890)	RenderShadow (0x444D40)	단일 문자열을 줄바꿈
cTextManager	CalculateTextExtents (0x43BF40)	DrawTextLines (0x43BC80), DrawMenu (0x43B4F0)	A: +0x18C, B: +0x198
cTextLabel	이 함수 (0x4428B0)	DrawText (0x442590)	+0x15C

cTextLabel에는 측정과 그리기만 확인된 상태입니다. 줄 문자열(+0x15C)을 채우는 SetText 계열도 있을 텐데, +0x15C에 쓰는 함수를 찾아 올려 주시면 2바이트 처리에 필요한 나머지 경로까지 확인할 수 있습니다.
*/