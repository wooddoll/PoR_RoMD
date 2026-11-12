// ---------------------------------------------------------------------------
// [실험, 16절 1단계] sneg 2바이트(china) 통로로 한글을 비트맵 그대로 출력
//
// sneg 의 모든 텍스트 경로는 한 글자씩 china_pass(+0x468D0)를 부르고,
// originalText[1] != 0 (TTF 표시) 글자는 GetChinaFontBitmap(+0x463D0)이 돌려주는 알파 마스크로 그린다.
// 두 함수만 바꿔서:
//   - china_pass        : 한글이면 폭/높이 = 우리 비트맵 아틀라스의 한글 RECT, charCode = 유니코드, TTF 표시
//                         → sneg 레이아웃(줄바꿈 포함)과 렌더 칸이 비트맵 폭 하나로 통일된다.
//   - GetChinaFontBitmap: 배율 1.0 이면 아틀라스 글리프 픽셀(0xFFFF)을 알파 0xFF 마스크로 돌려준다.
//                         배율 > 1 (2단계, 마커에 hires) 이면 마루 부리 TTF 를 그 크기로 직접 렌더한다.
//                         칸 = 원래 함수와 같은 ceil(w*배율), ceil(h*배율). 글꼴 크기는 '가'가 한글 칸에 맞게,
//                         글자는 칸 가운데.
// 2단계(hires)에서는 ASCII(0x20~0x7E)도 TTF 표시로 보내 고해상도 글꼴을 마루 부리 하나로 통일한다
// (폭 = 비트맵 아틀라스 RECT 폭 → 레이아웃은 저해상도와 같음).
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

std::map<uint64_t, std::unique_ptr<CachedBitmap>> g_bitmapCache;    // 배율 1.0 (아틀라스)
std::map<uint64_t, std::unique_ptr<CachedBitmap>> g_ttfCache;       // 배율 > 1 (TTF)

bool g_hiRes = false;       // 마커 파일에 hires (2단계)
int  g_ttfScalePercent = 100;   // 마커 ttfscale=NN : 맞춘 글꼴 크기(em)에 곱하는 % (50~150)
int  g_ttfWeight = FW_NORMAL;   // 마커 ttfweight=NNN : 100~900 (400 보통, 700 굵게)

constexpr UINT kCodePage = 949;
constexpr int  kRefHangul = 0xB0A1;     // '가'
const wchar_t  kTtfFace[] = L"MaruBuri Regular";

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

// 아틀라스 글리프를 TTF 표시 글자로 채운다
bool FillTtfGlyph(cGlyphResult* out, cFont* font, int key, unsigned short charCode, int byteLength)
{
    const int idx = GlyphIndex(font, key);
    if (idx < 0)
        return false;
    const RECT& r = font->pGlyphs[idx]->rect;
    const int w = r.right - r.left;
    const int h = r.bottom - r.top;
    if (w <= 0 || h <= 0 || h >= 256)
        return false;
    out->charCode = charCode;
    out->byteLength = byteLength;
    out->width = w;
    out->height = h;
    out->originalText[0] = 0;
    out->originalText[1] = 1;       // TTF 표시: 소비처가 GetCharRect 대신 GetChinaFontBitmap 사용
    out->originalText[2] = 0;
    out->originalText[3] = 0;
    g_fontByHeight[h] = font;
    return true;
}

cGlyphResult* __cdecl ChinaPass_Hook(cGlyphResult* out, cFont* font, const uint8_t* text, int len)
{
    // 2단계: ASCII 도 TTF 표시 (고해상도에서 마루 부리로). 저해상도(배율 1.0)는 아틀라스 마스크라 화면은 같다.
    if (g_hiRes && out && font && text && len >= 1 && text[0] >= 0x20 && text[0] <= 0x7E) {
        if (FillTtfGlyph(out, font, text[0], text[0], 1))
            return out;
    }
    if (out && font && text && len >= 2) {
        const uint8_t c1 = text[0], c2 = text[1];
        if (IsHangulRow(c1, c2)) {
            // 전각 영숫자: 번역문은 빌드 때 반각으로 바꾸지만, 남은 것은 ASCII 글자로 그린다 (2바이트 소비)
            if (const uint8_t a = FullwidthToAscii(c1, c2)) {
                if (g_hiRes && FillTtfGlyph(out, font, a, a, 2))
                    return out;
                const uint8_t tmp[2] = { a, 0 };
                g_origChinaPass(out, font, tmp, 1);
                out->byteLength = 2;
                return out;
            }
            wchar_t wc = 0;
            if (MultiByteToWideChar(kCodePage, MB_ERR_INVALID_CHARS, reinterpret_cast<const char*>(text), 2, &wc, 1) == 1 &&
                FillTtfGlyph(out, font, (c1 << 8) | c2, static_cast<unsigned short>(wc), 2))
                return out;
        }
    }
    return g_origChinaPass(out, font, text, len);
}

// charCode(유니코드) → 아틀라스 키. ASCII 는 그대로, 그 밖은 949 2바이트 (c1<<8)|c2. 없으면 -1
int AtlasKey(unsigned short charCode)
{
    if (charCode >= 0x20 && charCode <= 0x7E)
        return charCode;
    const wchar_t wc = static_cast<wchar_t>(charCode);
    char mb[4] = {};
    BOOL usedDefault = FALSE;
    if (WideCharToMultiByte(kCodePage, 0, &wc, 1, mb, sizeof(mb), nullptr, &usedDefault) != 2 || usedDefault)
        return -1;
    const uint8_t c1 = static_cast<uint8_t>(mb[0]), c2 = static_cast<uint8_t>(mb[1]);
    return IsHangulRow(c1, c2) ? ((c1 << 8) | c2) : -1;
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

    const int idx = GlyphIndex(font, AtlasKey(g->charCode));
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

// 원래 함수의 칸 크기 계산 (FUN_562677e0): ceil(v * scale - 0.001)
inline int ScaledCell(int v, float scale)
{
    return (v > 0 && scale > 0.0f) ? static_cast<int>(std::ceil(v * scale - 0.001f)) : 0;
}

// ---- 배율 > 1: 마루 부리 TTF 렌더 ----
struct TtfFont {
    HFONT font = nullptr;
    int   tmHeight = 0;
};
std::map<uint32_t, TtfFont> g_ttfFonts;     // 키 = (칸 높이 << 16) | 한글 칸 폭
HDC g_ttfDC = nullptr;

HFONT MakeTtfFont(int emHeight)
{
    LOGFONTW lf = {};
    lf.lfHeight = -emHeight;                // 음수 = 글자(em) 높이
    lf.lfWeight = g_ttfWeight;
    lf.lfCharSet = HANGUL_CHARSET;
    lf.lfOutPrecision = OUT_TT_ONLY_PRECIS;
    lf.lfQuality = ANTIALIASED_QUALITY;     // 회색 안티앨리어싱 (ClearType 은 색이 섞임)
    lstrcpyW(lf.lfFaceName, kTtfFace);
    return CreateFontIndirectW(&lf);
}

// 칸 높이 cellH, 한글 칸 폭 hangulW 에 맞는 글꼴: 줄 높이(tmHeight) ≤ cellH, '가' 폭 ≤ hangulW
const TtfFont* GetTtfFont(int cellH, int hangulW)
{
    const uint32_t key = (static_cast<uint32_t>(cellH) << 16) | static_cast<uint32_t>(hangulW & 0xFFFF);
    auto it = g_ttfFonts.find(key);
    if (it != g_ttfFonts.end())
        return it->second.font ? &it->second : nullptr;

    TtfFont& tf = g_ttfFonts[key];
    if (!g_ttfDC)
        g_ttfDC = CreateCompatibleDC(nullptr);
    if (!g_ttfDC)
        return nullptr;

    int em = cellH;
    for (int pass = 0; pass < 3 && em > 0; ++pass) {
        HFONT f = MakeTtfFont(em);
        if (!f)
            return nullptr;
        HGDIOBJ old = SelectObject(g_ttfDC, f);
        TEXTMETRICW tm = {};
        GetTextMetricsW(g_ttfDC, &tm);
        SIZE sz = {};
        const wchar_t ref = 0xAC00;         // '가'
        GetTextExtentPoint32W(g_ttfDC, &ref, 1, &sz);
        SelectObject(g_ttfDC, old);

        const int lineH = static_cast<int>(tm.tmHeight);
        const int refW = static_cast<int>(sz.cx);
        int next = em;
        if (lineH > cellH)
            next = (std::min)(next, em * cellH / lineH);
        if (hangulW > 0 && refW > hangulW)
            next = (std::min)(next, em * hangulW / refW);
        if (next == em) {
            tf.font = f;
            tf.tmHeight = lineH;
            break;
        }
        DeleteObject(f);
        em = next;
    }
    // 사용자 조절 (ttfscale): 칸에 맞춘 크기에 % 를 곱해 다시 만든다. 100 보다 크면 칸을 넘어 잘릴 수 있다.
    if (tf.font && g_ttfScalePercent != 100) {
        const int scaled = (std::max)(1, em * g_ttfScalePercent / 100);
        if (HFONT f = MakeTtfFont(scaled)) {
            HGDIOBJ old = SelectObject(g_ttfDC, f);
            TEXTMETRICW tm = {};
            GetTextMetricsW(g_ttfDC, &tm);
            SelectObject(g_ttfDC, old);
            DeleteObject(tf.font);
            tf.font = f;
            tf.tmHeight = static_cast<int>(tm.tmHeight);
            em = scaled;
        }
    }
    static int s_logged = 0;
    if (s_logged++ < 8)
        PLOG_DEBUG << "[exp] ttf font cell " << cellH << " hangulW " << hangulW << " -> em " << em
                   << " tmHeight " << tf.tmHeight << (tf.font ? "" : " (failed)");
    return tf.font ? &tf : nullptr;
}

CachedBitmap* BuildTtfGlyph(const cGlyphResult* g, float scale)
{
    const int w = g->width, h = g->height;
    if (w <= 0 || h <= 0 || h >= 256 || w >= 256)
        return nullptr;
    const int cw = ScaledCell(w, scale);
    const int ch = ScaledCell(h, scale);
    if (cw <= 0 || ch <= 0 || cw > 1024 || ch > 1024)
        return nullptr;
    const uint64_t key = (static_cast<uint64_t>(g->charCode) << 40) | (static_cast<uint64_t>(cw) << 24) |
                         (static_cast<uint64_t>(ch) << 8) | static_cast<uint64_t>(h);
    auto it = g_ttfCache.find(key);
    if (it != g_ttfCache.end())
        return it->second.get();

    // 한글 칸 폭: 이 폰트의 '가' RECT 폭 × 배율 (ASCII 도 같은 글꼴 크기를 쓰기 위해)
    int hangulW = cw;
    if (cFont* font = g_fontByHeight[h]) {
        const int idx = GlyphIndex(font, kRefHangul);
        if (idx >= 0)
            hangulW = ScaledCell(font->pGlyphs[idx]->rect.right - font->pGlyphs[idx]->rect.left, scale);
    }
    const TtfFont* tf = GetTtfFont(ch, hangulW);
    if (!tf)
        return nullptr;

    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = cw;
    bi.bmiHeader.biHeight = -ch;            // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(g_ttfDC, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bmp || !bits) {
        if (bmp) DeleteObject(bmp);
        return nullptr;
    }
    HGDIOBJ oldBmp = SelectObject(g_ttfDC, bmp);
    HGDIOBJ oldFont = SelectObject(g_ttfDC, tf->font);
    memset(bits, 0, static_cast<size_t>(cw) * ch * 4);
    SetBkMode(g_ttfDC, TRANSPARENT);
    SetTextColor(g_ttfDC, RGB(255, 255, 255));
    const wchar_t wc = static_cast<wchar_t>(g->charCode);
    SIZE sz = {};
    GetTextExtentPoint32W(g_ttfDC, &wc, 1, &sz);
    const int x = (cw - sz.cx) / 2;
    const int y = (ch - tf->tmHeight) / 2;
    TextOutW(g_ttfDC, x, y, &wc, 1);
    GdiFlush();

    auto entry = std::make_unique<CachedBitmap>();
    entry->data.resize(static_cast<size_t>(cw) * ch);
    const uint8_t* src = static_cast<const uint8_t*>(bits);
    for (size_t i = 0; i < entry->data.size(); ++i)
        entry->data[i] = src[i * 4 + 2];    // R 채널 = 알파 (원래 함수와 같음)

    SelectObject(g_ttfDC, oldFont);
    SelectObject(g_ttfDC, oldBmp);
    DeleteObject(bmp);

    entry->view.width = cw;
    entry->view.height = ch;
    entry->view.begin = entry->data.data();
    entry->view.end = entry->data.data() + entry->data.size();
    entry->view.cap = entry->view.end;
    CachedBitmap* p = entry.get();
    g_ttfCache.emplace(key, std::move(entry));

    static int s_logged = 0;
    if (s_logged++ < 4)
        PLOG_DEBUG << "[exp] ttf glyph U+" << std::hex << g->charCode << std::dec << " " << w << "x" << h
                   << " scale " << scale << " -> " << cw << "x" << ch;
    return p;
}

// 우리 china_pass 훅이 만든 글자인지 (아틀라스에 있는 ASCII/한글 행)
inline bool IsOurGlyph(const cGlyphResult* g)
{
    if (!g || g->charCode < 0x20)
        return false;
    if (g->charCode <= 0x7E)
        return g_hiRes;                     // ASCII 는 2단계에서만 TTF 표시로 보낸다
    return g->charCode >= 0x80;
}

ChinaBitmap* __cdecl GetChinaFontBitmap_Hook(cGlyphResult* glyph, float scale)
{
    if (IsOurGlyph(glyph)) {
        if (std::fabs(scale - 1.0f) < 0.001f) {
            if (CachedBitmap* b = BuildBitmapGlyph(glyph))
                return &b->view;
        }
        else if (g_hiRes && scale > 1.0f && AtlasKey(glyph->charCode) >= 0) {
            if (CachedBitmap* b = BuildTtfGlyph(glyph, scale))
                return &b->view;
        }
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
// 마커 파일(kr_native_test.txt)에서 "key=숫자" 를 찾는다. 없으면 def
static int ReadMarkerInt(const char* key, int def, int lo, int hi)
{
    char path[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, path, MAX_PATH);
    char* slash = (n > 0 && n < MAX_PATH) ? strrchr(path, '\\') : nullptr;
    if (!slash)
        return def;
    lstrcpyA(slash + 1, "kr_native_test.txt");
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return def;
    char buf[1024] = {};
    DWORD rd = 0;
    ReadFile(f, buf, sizeof(buf) - 1, &rd, NULL);
    CloseHandle(f);
    _strlwr_s(buf);
    const char* p = strstr(buf, key);
    if (!p)
        return def;
    const int v = atoi(p + strlen(key));
    return (v >= lo && v <= hi) ? v : def;
}

void InstallChinaGlyphHooks(uintptr_t snegBase, bool hiRes)
{
    g_hiRes = hiRes;
    g_ttfScalePercent = ReadMarkerInt("ttfscale=", 100, 50, 150);
    g_ttfWeight = ReadMarkerInt("ttfweight=", FW_NORMAL, 100, 900);
    PLOG_DEBUG << "[exp] ttf scale " << g_ttfScalePercent << "% weight " << g_ttfWeight;
    HookWithTrampoline(snegBase + 0x468D0, reinterpret_cast<void*>(&ChinaPass_Hook),
                       reinterpret_cast<void**>(&g_origChinaPass), "china_pass");
    HookWithTrampoline(snegBase + 0x463D0, reinterpret_cast<void*>(&GetChinaFontBitmap_Hook),
                       reinterpret_cast<void**>(&g_origGetChinaFontBitmap), "GetChinaFontBitmap");
}
