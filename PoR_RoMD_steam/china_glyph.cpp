// ---------------------------------------------------------------------------
// [실험, 16절 1단계] sneg 2바이트(china) 통로로 한글을 비트맵 그대로 출력
//
// sneg 의 모든 텍스트 경로는 한 글자씩 china_pass(+0x468D0)를 부르고,
// originalText[1] != 0 (TTF 표시) 글자는 GetChinaFontBitmap(+0x463D0)이 돌려주는 알파 마스크로 그린다.
// 두 함수만 바꿔서:
//   - china_pass        : 한글이면 폭/높이 = 우리 비트맵 아틀라스의 한글 RECT, charCode = 유니코드, TTF 표시
//                         → sneg 레이아웃(줄바꿈 포함)과 렌더 칸이 비트맵 폭 하나로 통일된다.
//   - GetChinaFontBitmap: 배율 1.0 이면 아틀라스 글리프 픽셀(0xFFFF)을 알파 0xFF 마스크로 돌려준다.
//                         배율 > 1 은 아직 원래 함수(TTF 렌더) — 2단계에서 구현.
// 아틀라스에 없는 글자(기호 등)는 원래 china_pass/TTF 경로로 그대로 보낸다.
// ---------------------------------------------------------------------------
#include "por2.h"

#include <MinHook.h>
#include <map>
#include <memory>
#include <vector>
#include <cmath>
#include "help_util.h"

namespace {

// GetChinaFontBitmap 반환값 (sneg 캐시 노드 +0x20 의 값 부분과 같은 배치)
//   +0x00 폭, +0x04 높이, +0x08 std::vector<uint8_t> {begin, end, cap} — 행 우선, 폭 × 높이 바이트 알파
struct ChinaBitmap {
    int      width;
    int      height;
    uint8_t* begin;
    uint8_t* end;
    uint8_t* cap;
    int      reserved[2];
};
static_assert(offsetof(ChinaBitmap, begin) == 0x8, "");

struct CachedBitmap {
    ChinaBitmap          view{};
    std::vector<uint8_t> data;
};

typedef cGlyphResult* (__cdecl* tChinaPass)(cGlyphResult* out, cFont* font, const uint8_t* text, int len);
typedef ChinaBitmap* (__cdecl* tGetChinaFontBitmap)(cGlyphResult* glyph, float scale);

tChinaPass          g_origChinaPass = nullptr;
tGetChinaFontBitmap g_origGetChinaFontBitmap = nullptr;

// GetChinaFontBitmap 은 폰트를 받지 않는다 → china_pass 훅에서 본 폰트를 글자 높이로 기억한다.
// 병합 폰트 font0~4 의 높이는 18/16/28/13/15 로 모두 다르다.
cFont* g_fontByHeight[256] = {};

std::map<uint64_t, std::unique_ptr<CachedBitmap>> g_bitmapCache;

constexpr UINT kCodePage = 949;

inline bool IsHangulRow(uint8_t c1, uint8_t c2)
{
    return IsHangul(c1, c2);        // c1 0xA1~0xC8 (기호/전각 행 포함), c2 0xA1~0xFE
}

// EUC-KR 전각 영숫자 A3A1~A3FD (A3DC ￦ 제외) → ASCII. 바꿀 수 없으면 0
inline uint8_t FullwidthToAscii(uint8_t c1, uint8_t c2)
{
    if (c1 != 0xA3 || c2 < 0xA1 || c2 > 0xFD || c2 == 0xDC)
        return 0;
    return static_cast<uint8_t>(c2 - 0x80);
}

int GlyphIndex(cFont* font, int key)
{
    if (!font || !font->pCharMap || !font->pGlyphs)
        return -1;
    const int idx = font->pCharMap->FindGlyphIndex(key);
    return (idx >= 0 && idx < font->glyphCount) ? idx : -1;
}

cGlyphResult* __cdecl ChinaPass_Hook(cGlyphResult* out, cFont* font, const uint8_t* text, int len)
{
    if (out && font && text && len >= 2) {
        const uint8_t c1 = text[0], c2 = text[1];
        if (IsHangulRow(c1, c2)) {
            // 전각 영숫자: 번역문은 빌드 때 반각으로 바꾸지만, 남은 것은 ASCII 글자로 그린다 (2바이트 소비)
            if (const uint8_t a = FullwidthToAscii(c1, c2)) {
                const uint8_t tmp[2] = { a, 0 };
                g_origChinaPass(out, font, tmp, 1);
                out->byteLength = 2;
                return out;
            }
            const int idx = GlyphIndex(font, (c1 << 8) | c2);
            wchar_t wc = 0;
            if (idx >= 0 &&
                MultiByteToWideChar(kCodePage, MB_ERR_INVALID_CHARS, reinterpret_cast<const char*>(text), 2, &wc, 1) == 1)
            {
                const RECT& r = font->pGlyphs[idx]->rect;
                const int w = r.right - r.left;
                const int h = r.bottom - r.top;
                if (w > 0 && h > 0 && h < 256) {
                    out->charCode = static_cast<unsigned short>(wc);
                    out->byteLength = 2;
                    out->width = w;
                    out->height = h;
                    out->originalText[0] = 0;
                    out->originalText[1] = 1;       // TTF 표시: 소비처가 GetCharRect 대신 GetChinaFontBitmap 사용
                    out->originalText[2] = 0;
                    out->originalText[3] = 0;
                    g_fontByHeight[h] = font;
                    return out;
                }
            }
        }
    }
    return g_origChinaPass(out, font, text, len);
}

// 아틀라스(16bpp, 글자 픽셀 = 0xFFFF)에서 한 글자를 알파 마스크로 읽는다
CachedBitmap* BuildBitmapGlyph(const cGlyphResult* g)
{
    const int w = g->width, h = g->height;
    if (w <= 0 || h <= 0 || h >= 256 || w >= 256)
        return nullptr;
    const uint64_t key = (static_cast<uint64_t>(g->charCode) << 32) | (static_cast<uint64_t>(w) << 16) | static_cast<uint64_t>(h);
    auto it = g_bitmapCache.find(key);
    if (it != g_bitmapCache.end())
        return it->second.get();

    cFont* font = g_fontByHeight[h];
    if (!font || !font->pSurface)
        return nullptr;

    const wchar_t wc = static_cast<wchar_t>(g->charCode);
    char mb[4] = {};
    BOOL usedDefault = FALSE;
    if (WideCharToMultiByte(kCodePage, 0, &wc, 1, mb, sizeof(mb), nullptr, &usedDefault) != 2 || usedDefault)
        return nullptr;
    const uint8_t c1 = static_cast<uint8_t>(mb[0]), c2 = static_cast<uint8_t>(mb[1]);
    if (!IsHangulRow(c1, c2))
        return nullptr;
    const int idx = GlyphIndex(font, (c1 << 8) | c2);
    if (idx < 0)
        return nullptr;
    const RECT r = font->pGlyphs[idx]->rect;

    IDirectDrawSurface7* surf = font->pSurface;
    DDSURFACEDESC2 ddsd = {};
    ddsd.dwSize = sizeof(ddsd);
    if (surf->Lock(nullptr, &ddsd, DDLOCK_WAIT | DDLOCK_READONLY, nullptr) != DD_OK)
        return nullptr;

    auto entry = std::make_unique<CachedBitmap>();
    entry->data.assign(static_cast<size_t>(w) * h, 0);
    const int rw = (std::min)(w, static_cast<int>(r.right - r.left));
    const int rh = (std::min)(h, static_cast<int>(r.bottom - r.top));
    const int pitchW = static_cast<int>(static_cast<unsigned>(ddsd.lPitch) >> 1);
    const WORD* base = static_cast<const WORD*>(ddsd.lpSurface);
    if (base && ddsd.ddpfPixelFormat.dwRGBBitCount == 16 &&
        r.left >= 0 && r.top >= 0 && r.right <= static_cast<LONG>(ddsd.dwWidth) && r.bottom <= static_cast<LONG>(ddsd.dwHeight))
    {
        for (int y = 0; y < rh; ++y) {
            const WORD* src = base + (r.top + y) * pitchW + r.left;
            uint8_t* dst = entry->data.data() + static_cast<size_t>(y) * w;
            for (int x = 0; x < rw; ++x)
                if (src[x] == 0xFFFF)
                    dst[x] = 0xFF;
        }
    }
    else {
        static int s_logged = 0;
        if (s_logged++ < 3)
            PLOG_ERROR << "[exp] font atlas surface unexpected (bpp " << ddsd.ddpfPixelFormat.dwRGBBitCount << ")";
    }
    surf->Unlock(nullptr);

    entry->view.width = w;
    entry->view.height = h;
    entry->view.begin = entry->data.data();
    entry->view.end = entry->data.data() + entry->data.size();
    entry->view.cap = entry->view.end;
    CachedBitmap* p = entry.get();
    g_bitmapCache.emplace(key, std::move(entry));
    return p;
}

ChinaBitmap* __cdecl GetChinaFontBitmap_Hook(cGlyphResult* glyph, float scale)
{
    if (glyph && std::fabs(scale - 1.0f) < 0.001f && glyph->charCode >= 0x80 && glyph->charCode <= 0xFFFF) {
        if (CachedBitmap* b = BuildBitmapGlyph(glyph))
            return &b->view;
    }
    return g_origGetChinaFontBitmap(glyph, scale);
}

bool HookWithTrampoline(uintptr_t target, void* detour, void** orig, const char* name)
{
    // 두 함수 모두 SEH 프롤로그: 55 8B EC 6A FF 68 <imm32>
    static const uint8_t kPrologue[] = { 0x55, 0x8B, 0xEC, 0x6A, 0xFF, 0x68 };
    if (memcmp(reinterpret_cast<const void*>(target), kPrologue, sizeof(kPrologue)) != 0) {
        PLOG_ERROR << "[exp] " << name << " prologue mismatch";
        return false;
    }
    MH_STATUS st = MH_Initialize();
    if (st == MH_OK || st == MH_ERROR_ALREADY_INITIALIZED) {
        st = MH_CreateHook(reinterpret_cast<LPVOID>(target), detour, orig);
        if (st == MH_OK) st = MH_EnableHook(reinterpret_cast<LPVOID>(target));
    }
    PLOG_DEBUG << "[exp] " << name << " hook status=" << st;
    return st == MH_OK;
}

} // namespace

// sneg_hook.cpp 의 실험 모드에서 호출
void InstallChinaGlyphHooks(uintptr_t snegBase)
{
    HookWithTrampoline(snegBase + 0x468D0, reinterpret_cast<void*>(&ChinaPass_Hook),
                       reinterpret_cast<void**>(&g_origChinaPass), "china_pass");
    HookWithTrampoline(snegBase + 0x463D0, reinterpret_cast<void*>(&GetChinaFontBitmap_Hook),
                       reinterpret_cast<void**>(&g_origGetChinaFontBitmap), "GetChinaFontBitmap");
}
