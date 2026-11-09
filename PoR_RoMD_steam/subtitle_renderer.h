#pragma once

#include <string>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <condition_variable>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define BINK_SURFACE_24R    0
#define BINK_SURFACE_24     1
#define BINK_SURFACE_32     2
#define BINK_SURFACE_32R    3
#define BINK_SURFACE_32A    4
#define BINK_SURFACE_32RA   5
#define BINK_SURFACE_555    6
#define BINK_SURFACE_565    7
#define BINK_SURFACE_655    8
#define BINK_SURFACE_664    9
#define BINK_SURFACE_ARGB   10
#define BINK_SURFACE_MASK   15

class SubtitleRenderer {
private:
    // 프레임 좌표계 기준으로 잘라낸 자막 비트맵
    struct SubBitmap {
        int x = 0, y = 0;          // 프레임 내 좌상단 위치
        int w = 0, h = 0;
        std::vector<uint32_t> px;  // 32bpp, 0 = 투명
    };
    using BmpPtr = std::shared_ptr<const SubBitmap>;

    // ---- 캐시 / 워커 상태 ----
    static inline std::mutex s_mtx;
    static inline std::condition_variable s_cv;
    static inline std::unordered_map<std::wstring, BmpPtr> s_cache;
    static inline std::unordered_set<std::wstring> s_pending;
    static inline std::deque<std::wstring> s_queue;
    static inline uint32_t s_width = 0, s_height = 0;
    static inline uint32_t s_generation = 0;   // 해상도 변경 시 증가 -> 오래된 결과 폐기
    static inline std::thread s_worker;
    static inline bool s_stop = false;
    static inline bool s_started = false;

    // ---- 실제 GDI 렌더링 (어느 스레드에서 호출해도 무방) ----
    static BmpPtr RenderBitmap(const std::wstring& text, uint32_t width, uint32_t height) {
        int fontSize = (int)height / 20;
        if (fontSize < 18) fontSize = 18;
        int outline = (fontSize >= 28) ? 2 : 1;

        // 하단 띠 영역만 렌더링
        RECT rc;
        rc.left = 20;
        rc.right = (LONG)width - 20;
        rc.top = (LONG)height - (fontSize * 4);
        rc.bottom = (LONG)height - ((LONG)height / 25);

        int bandTop = (std::max)(0, (int)rc.top - 8);
        int bandH = (int)height - bandTop;
        if (bandH <= 0 || width == 0) return nullptr;

        RECT rcBand = rc;
        OffsetRect(&rcBand, 0, -bandTop);   // 띠 기준 좌표로 변환

        HDC hdcMem = CreateCompatibleDC(NULL);
        if (!hdcMem) return nullptr;

        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = (LONG)width;
        bmi.bmiHeader.biHeight = -bandH; // top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        uint32_t* gdiPx = nullptr;
        HBITMAP hBmp = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, (void**)&gdiPx, NULL, 0);
        if (!hBmp || !gdiPx) { DeleteDC(hdcMem); return nullptr; }

        HGDIOBJ oldBmp = SelectObject(hdcMem, hBmp);
        memset(gdiPx, 0, (size_t)width * bandH * 4);

        HFONT hFont = CreateFontW(
            fontSize, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"맑은 고딕");
        HGDIOBJ oldFont = SelectObject(hdcMem, hFont);
        SetBkMode(hdcMem, TRANSPARENT);

        const UINT fmt = DT_CENTER | DT_BOTTOM | DT_WORDBREAK;

        // 외곽선: 완전한 검정(0)은 "투명"과 구분이 안 되므로 RGB(1,1,1) 사용
        SetTextColor(hdcMem, RGB(1, 1, 1));
        for (int dx = -outline; dx <= outline; dx++) {
            for (int dy = -outline; dy <= outline; dy++) {
                if (dx == 0 && dy == 0) continue;
                RECT r = rcBand;
                OffsetRect(&r, dx, dy);
                DrawTextW(hdcMem, text.c_str(), -1, &r, fmt);
            }
        }
        SetTextColor(hdcMem, RGB(255, 255, 255));
        DrawTextW(hdcMem, text.c_str(), -1, &rcBand, fmt);
        GdiFlush();

        // 실제 픽셀이 있는 bbox 계산
        int minX = (int)width, minY = bandH, maxX = -1, maxY = -1;
        for (int y = 0; y < bandH; y++) {
            const uint32_t* row = gdiPx + (size_t)y * width;
            for (uint32_t x = 0; x < width; x++) {
                if (row[x] & 0x00FFFFFF) {
                    if ((int)x < minX) minX = (int)x;
                    if ((int)x > maxX) maxX = (int)x;
                    if (y < minY) minY = y;
                    if (y > maxY) maxY = y;
                }
            }
        }

        BmpPtr result;
        if (maxX >= minX && maxY >= minY) {
            auto bm = std::make_shared<SubBitmap>();
            bm->x = minX;
            bm->y = bandTop + minY;
            bm->w = maxX - minX + 1;
            bm->h = maxY - minY + 1;
            bm->px.resize((size_t)bm->w * bm->h);
            for (int y = 0; y < bm->h; y++) {
                const uint32_t* src = gdiPx + (size_t)(minY + y) * width + minX;
                memcpy(&bm->px[(size_t)y * bm->w], src, (size_t)bm->w * 4);
            }
            result = bm;
        }
        else {
            result = std::make_shared<SubBitmap>(); // 빈 자막 (w=h=0)
        }

        SelectObject(hdcMem, oldFont);
        DeleteObject(hFont);
        SelectObject(hdcMem, oldBmp);
        DeleteObject(hBmp);
        DeleteDC(hdcMem);
        return result;
    }

    // ---- 워커 스레드 ----
    static void WorkerLoop() {
        for (;;) {
            std::wstring text;
            uint32_t w, h, gen;
            {
                std::unique_lock<std::mutex> lk(s_mtx);
                s_cv.wait(lk, [] { return s_stop || !s_queue.empty(); });
                if (s_stop) return;
                text = std::move(s_queue.front());
                s_queue.pop_front();
                w = s_width; h = s_height; gen = s_generation;
            }

            BmpPtr bmp = RenderBitmap(text, w, h);   // 락 밖에서 무거운 작업

            {
                std::lock_guard<std::mutex> lk(s_mtx);
                s_pending.erase(text);
                if (gen == s_generation && bmp)      // 해상도가 바뀌었으면 버림
                    s_cache[text] = bmp;
            }
        }
    }

    // s_mtx를 잡은 상태에서 호출
    static void EnqueueLocked(const std::wstring& text) {
        if (text.empty() || s_cache.count(text) || s_pending.count(text)) return;
        if (!s_started) {                           // 첫 호출 시 지연 시작 (DllMain에서 만들지 않도록)
            s_started = true;
            s_stop = false;
            s_worker = std::thread(WorkerLoop);
        }
        s_pending.insert(text);
        s_queue.push_back(text);
        s_cv.notify_one();
    }

    // s_mtx를 잡은 상태에서 호출
    static void SetResolutionLocked(uint32_t w, uint32_t h) {
        if (s_width == w && s_height == h) return;
        s_width = w; s_height = h;
        s_generation++;
        s_cache.clear();
        s_pending.clear();
        s_queue.clear();
    }

public:
    // 영상 열 때(자막 로딩 직후) 호출: 모든 자막을 백그라운드에서 미리 렌더링
    static void Prefetch(const std::vector<std::wstring>& texts, uint32_t width, uint32_t height) {
        if (width == 0 || height == 0) return;
        std::lock_guard<std::mutex> lk(s_mtx);
        SetResolutionLocked(width, height);
        for (const auto& t : texts) EnqueueLocked(t);
    }

    // 영상 닫을 때: 캐시/큐 정리 (워커 스레드는 유지)
    static void ClearCache() {
        std::lock_guard<std::mutex> lk(s_mtx);
        s_generation++;
        s_cache.clear();
        s_pending.clear();
        s_queue.clear();
    }

    // 게임 종료/언로드 시 호출. DllMain(DLL_PROCESS_DETACH)에서는 호출하지 말 것(로더 락 데드락 위험)
    static void Shutdown() {
        {
            std::lock_guard<std::mutex> lk(s_mtx);
            s_stop = true;
            s_cv.notify_all();
        }
        if (s_worker.joinable()) s_worker.join();
        s_started = false;
        ClearCache();
    }

    static void RenderSubtitleToBuffer(
        void* destBuffer, int32_t destPitch, uint32_t width, uint32_t height,
        uint32_t destX, uint32_t destY, uint32_t binkFormatFlags, const std::wstring& text)
    {
        if (text.empty() || !destBuffer || width == 0 || height == 0) return;

        // 1. 캐시 조회 (락은 매우 짧게)
        BmpPtr bmp;
        {
            std::lock_guard<std::mutex> lk(s_mtx);
            SetResolutionLocked(width, height);
            auto it = s_cache.find(text);
            if (it != s_cache.end()) bmp = it->second;
        }

        // 캐시 미스: Prefetch를 못 한 자막. 1회만 동기 렌더링 후 캐시에 저장
        if (!bmp) {
            bmp = RenderBitmap(text, width, height);
            if (!bmp) return;
            std::lock_guard<std::mutex> lk(s_mtx);
            if (s_width == width && s_height == height)
                s_cache[text] = bmp;
        }
        if (bmp->w == 0 || bmp->h == 0) return;

        // 2. 서페이스 형식 판별
        uint32_t surfaceType = binkFormatFlags & BINK_SURFACE_MASK;
        int bpp = 4;
        if (surfaceType == BINK_SURFACE_565 || surfaceType == BINK_SURFACE_555 ||
            surfaceType == BINK_SURFACE_655 || surfaceType == BINK_SURFACE_664) {
            bpp = 2;
        }
        else if (surfaceType == BINK_SURFACE_24 || surfaceType == BINK_SURFACE_24R) {
            bpp = 3;
        }
        else if (surfaceType >= BINK_SURFACE_32 && surfaceType <= BINK_SURFACE_32RA) {
            bpp = 4;
        }
        else {
            int absPitch = std::abs(destPitch);
            int est = absPitch / (int)width;
            if (est == 2 || est == 3 || est == 4) bpp = est;
        }

        const bool swapRB32 = (surfaceType == BINK_SURFACE_32R || surfaceType == BINK_SURFACE_32RA);
        const bool is555 = (surfaceType == BINK_SURFACE_555);
        const bool rgbOrder24 = (surfaceType == BINK_SURFACE_24R);

        // 3. bbox 영역만 복사 (프레임 범위 밖은 클리핑)
        for (int y = 0; y < bmp->h; y++) {
            uint32_t fy = bmp->y + y;
            if (fy >= height) break;

            const uint32_t* src = &bmp->px[(size_t)y * bmp->w];
            uint8_t* dstRow = (uint8_t*)destBuffer
                + (ptrdiff_t)(destY + fy) * destPitch
                + (size_t)(destX + bmp->x) * bpp;

            int cw = bmp->w;
            if ((uint32_t)(bmp->x + cw) > width) cw = (int)width - bmp->x;

            for (int x = 0; x < cw; x++) {
                uint32_t c = src[x] & 0x00FFFFFF;
                if (c == 0) continue;

                uint8_t r = (c >> 16) & 0xFF;
                uint8_t g = (c >> 8) & 0xFF;
                uint8_t b = c & 0xFF;

                switch (bpp) {
                case 2: {
                    uint16_t* d = (uint16_t*)(dstRow + x * 2);
                    *d = is555
                        ? (uint16_t)(((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3))
                        : (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
                    break;
                }
                case 3: {
                    uint8_t* d = dstRow + x * 3;
                    if (rgbOrder24) { d[0] = r; d[1] = g; d[2] = b; }
                    else { d[0] = b; d[1] = g; d[2] = r; }
                    break;
                }
                default: {
                    uint32_t* d = (uint32_t*)(dstRow + x * 4);
                    *d = swapRB32 ? (uint32_t)(r | (g << 8) | (b << 16)) : c;
                    break;
                }
                }
            }
        }
    }
};