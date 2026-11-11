#include "por2.h"
#include "help_util.h"

/* ------------------------------------------------------------------ */
/*  cFont vtable +0x1C / +0x20 / +0x24 교체                             */
/*  (GetCharHeight_00435e00 / GetCharWidth_00435e30 / GetCharRect_00435e60) */
/*                                                                    */
/*  게임과 sneg 에는 문자열을 1바이트씩 읽어서 이 세 함수에 넘기는          */
/*  "폭 계산 전용" 함수가 남아 있다:                                     */
/*    게임 툴팁 0x436a50 / 0x43dfc0, SprintfMeasure 0x476820,            */
/*    sneg FUN_59576cc0 / FUN_59576ae0 / SprintfMeasure_5955cb10         */
/*  (그리는 함수는 모두 따로 훅하므로 여기에는 폭 계산만 들어온다)         */
/*                                                                    */
/*  상태 머신(직전 바이트 기억)은 쓰지 않는다. 한글 바이트(0xA1~0xFE)는    */
/*  호출마다 독립적으로 "기준 한글('가') 폭의 절반(올림)" 을 돌려준다.     */
/*    -> 2바이트 합 = 한글 한 글자 폭 (폭이 홀수면 글자당 +1px)           */
/*  박스 크기 계산용이므로 약간 크게 잡히는 쪽으로 맞췄다.                */
/*                                                                    */
/*  우리 훅 코드는 por2.h 의 인라인 cFont::GetCharXXX (FindGlyphIndex 직접 */
/*  호출) 를 쓰므로 이 vtable 훅을 거치지 않는다.                        */
/* ------------------------------------------------------------------ */

namespace {

constexpr uintptr_t VT_GetCharHeight = 0x00622528;   // cFont vtable(0x0062250C) +0x1C
constexpr uintptr_t VT_GetCharWidth  = 0x0062252C;   // +0x20
constexpr uintptr_t VT_GetCharRect   = 0x00622530;   // +0x24

constexpr uintptr_t ORG_GetCharHeight = 0x00435E00;
constexpr uintptr_t ORG_GetCharWidth  = 0x00435E30;
constexpr uintptr_t ORG_GetCharRect   = 0x00435E60;

constexpr int REF_HANGUL = 0xB0A1;                    // '가' : 한글 바이트 크기의 기준 글리프

/* 한글 바이트용 Rect (폰트별, 기준 글리프의 왼쪽 절반). 폰트는 5개 */
struct HalfRectCache {
    const cFont* font;
    RECT rect;
};
constexpr int MAX_FONTS = 16;
HalfRectCache g_halfRect[MAX_FONTS] = {};

/* code 의 하위 1바이트가 한글 바이트인지 판정하고, 1바이트 코드는 signed char 값
   (폰트 키 형식)으로 정규화한다. 호출부가 MOVSX(부호 확장) 또는 CONCAT31(상위
   3바이트 쓰레기)로 넘기므로 하위 1바이트만 의미가 있다.
   2바이트 키(0xA1A1~0xC8FE)가 직접 들어오면 그대로 둔다.                       */
inline bool IsHangulByte(int code, int& norm)
{
    if ((code & 0xFFFF0000) == 0 && code > 0xFF) {
        norm = code;
        return false;
    }
    uint8_t b = static_cast<uint8_t>(code & 0xFF);
    norm = static_cast<int>(static_cast<signed char>(b));
    return 0xA1 <= b && b <= 0xFE;
}

inline const RECT* RefGlyphRect(cFont* f)
{
    if (!f->pCharMap)
        return nullptr;
    int idx = f->pCharMap->FindGlyphIndex(REF_HANGUL);
    return (idx >= 0) ? &f->pGlyphs[idx]->rect : nullptr;
}

inline int HalfCeil(int w) { return (w + 1) / 2; }

RECT* HalfRect(cFont* f, const RECT* ref)
{
    for (HalfRectCache& c : g_halfRect) {
        if (c.font == f)
            return &c.rect;
        if (c.font == nullptr) {
            c.rect = *ref;
            c.rect.right = ref->left + HalfCeil(ref->right - ref->left);
            c.font = f;                     // rect 를 채운 뒤에 등록
            return &c.rect;
        }
    }
    return &g_dummyZeroRect;                // 캐시가 꽉 차면 (있을 수 없음) 폭 0
}

} // namespace

int __fastcall cFont_GetCharHeight_Hook(cFont* self, void* /*edx*/, int code)
{
    int norm;
    if (IsHangulByte(code, norm)) {
        if (const RECT* ref = RefGlyphRect(self))
            return ref->bottom - ref->top;
    }
    return self->GetCharHeight(norm);
}

int __fastcall cFont_GetCharWidth_Hook(cFont* self, void* /*edx*/, int code)
{
    int norm;
    if (IsHangulByte(code, norm)) {
        if (const RECT* ref = RefGlyphRect(self))
            return HalfCeil(ref->right - ref->left);
    }
    return self->GetCharWidth(norm);
}

// [진단] 한글 바이트를 1바이트씩 vtable GetCharRect 로 넘기는 호출부(=1바이트씩 그리는 함수) 기록. 배포 전 제거
static void DiagLogByteCaller(void* ret)
{
    static void* s_seen[32] = {};
    for (int i = 0; i < 32; ++i) {
        if (s_seen[i] == ret) return;
        if (!s_seen[i]) {
            s_seen[i] = ret;
            HMODULE m = nullptr;
            char path[MAX_PATH] = "?";
            if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                   (LPCSTR)ret, &m) && m)
                GetModuleFileNameA(m, path, MAX_PATH);
            const char* base = strrchr(path, '\\');
            PLOG_DEBUG << "[diag] GetCharRect(hangul byte) caller " << ret << " = " << (base ? base + 1 : path)
                       << "+0x" << std::hex << (unsigned)((uintptr_t)ret - (uintptr_t)m);
            return;
        }
    }
}

RECT* __fastcall cFont_GetCharRect_Hook(cFont* self, void* /*edx*/, int code)
{
    int norm;
    if (IsHangulByte(code, norm)) {
        DiagLogByteCaller(_ReturnAddress());
        if (const RECT* ref = RefGlyphRect(self))
            return HalfRect(self, ref);
    }
    return self->GetCharRect(norm);         // 없는 글리프면 NULL 대신 빈 Rect (크래시 방지)
}

void InstallFontMetricHooks()
{
    struct Slot { uintptr_t slot; uintptr_t org; uintptr_t hook; const char* name; };
    const Slot slots[] = {
        { VT_GetCharHeight, ORG_GetCharHeight, reinterpret_cast<uintptr_t>(cFont_GetCharHeight_Hook), "GetCharHeight" },
        { VT_GetCharWidth,  ORG_GetCharWidth,  reinterpret_cast<uintptr_t>(cFont_GetCharWidth_Hook),  "GetCharWidth"  },
        { VT_GetCharRect,   ORG_GetCharRect,   reinterpret_cast<uintptr_t>(cFont_GetCharRect_Hook),   "GetCharRect"   },
    };

    for (const Slot& s : slots) {
        uint32_t cur = *reinterpret_cast<const uint32_t*>(s.slot);
        if (cur != s.org) {
            PLOG_ERROR << "[FontMetrics] vtable slot " << s.name << " @" << std::hex << s.slot
                       << " = " << cur << " (expected " << s.org << "), skip";
            continue;
        }
        patchDWORD(s.slot, static_cast<uint32_t>(s.hook));
    }
}
