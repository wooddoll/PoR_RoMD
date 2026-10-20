#pragma once

#include <string>
#include <cstdint>
#include <vector>
#include <algorithm>

#define WIN32_LEAN_AND_MEAN // 불필요한 Windows 헤더 제외 (선택 사항)
#include <windows.h>

// Bink 표면 포맷 상수
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
    // 자막 비트맵 캐시 데이터
    static inline std::wstring s_cachedText = L"";
    static inline uint32_t s_cachedWidth = 0;
    static inline uint32_t s_cachedHeight = 0;
    static inline std::vector<uint32_t> s_pixelBuffer; // 캐시된 32bpp 자막 비트맵

    // 자막 텍스트가 달라졌을 때만 1회 호출되어 비트맵을 갱신
    static void UpdateSubtitleBitmap(const std::wstring& text, uint32_t width, uint32_t height) {
        s_cachedText = text;
        s_cachedWidth = width;
        s_cachedHeight = height;
        s_pixelBuffer.assign(width * height, 0); // 0으로 투명 초기화

        // [핵심] GetDC(NULL) 대신 CreateCompatibleDC(NULL)을 사용하여 DWM 깜빡임 원천 차단
        HDC hdcMem = CreateCompatibleDC(NULL);
        if (!hdcMem) return;

        BITMAPINFO bmi = { 0 };
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = width;
        bmi.bmiHeader.biHeight = -(LONG)height; // Top-Down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        uint32_t* pGdiPixels = nullptr;
        HBITMAP hBmp = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, (void**)&pGdiPixels, NULL, 0);
        if (!hBmp || !pGdiPixels) {
            DeleteDC(hdcMem);
            return;
        }

        HGDIOBJ hOldBmp = SelectObject(hdcMem, hBmp);
        memset(pGdiPixels, 0, width * height * 4);

        int fontSize = height / 20;
        if (fontSize < 18) fontSize = 18;

        HFONT hFont = CreateFontW(
            fontSize, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"맑은 고딕");
        HGDIOBJ hOldFont = SelectObject(hdcMem, hFont);

        SetBkMode(hdcMem, TRANSPARENT);

        RECT rc;
        rc.left = 20;
        rc.right = width - 20;
        rc.top = height - (fontSize * 4);
        rc.bottom = height - (height / 25);

        // 외곽선 (검은색 8방향)
        SetTextColor(hdcMem, RGB(0, 0, 0));
        int outline = (fontSize >= 28) ? 2 : 1;
        for (int dx = -outline; dx <= outline; dx++) {
            for (int dy = -outline; dy <= outline; dy++) {
                if (dx == 0 && dy == 0) continue;
                RECT rcOut = rc;
                OffsetRect(&rcOut, dx, dy);
                DrawTextW(hdcMem, text.c_str(), -1, &rcOut, DT_CENTER | DT_BOTTOM | DT_WORDBREAK);
            }
        }

        // 본문 (흰색)
        SetTextColor(hdcMem, RGB(255, 255, 255));
        DrawTextW(hdcMem, text.c_str(), -1, &rc, DT_CENTER | DT_BOTTOM | DT_WORDBREAK);

        GdiFlush();

        // 렌더링된 결과를 캐시 버퍼로 복사
        memcpy(s_pixelBuffer.data(), pGdiPixels, width * height * 4);

        // GDI 해제
        SelectObject(hdcMem, hOldFont);
        DeleteObject(hFont);
        SelectObject(hdcMem, hOldBmp);
        DeleteObject(hBmp);
        DeleteDC(hdcMem);
    }

public:
    static void ClearCache() {
        s_cachedText.clear();
        s_pixelBuffer.clear();
    }

    static void RenderSubtitleToBuffer(
        void* destBuffer, int32_t destPitch, uint32_t width, uint32_t height,
        uint32_t destX, uint32_t destY, uint32_t binkFormatFlags, const std::wstring& text)
    {
        if (text.empty() || !destBuffer || width == 0 || height == 0) return;

        // 1. 자막 내용이나 해상도가 변경되었을 때만 딱 한 번 GDI 렌더링 수행
        if (s_cachedText != text || s_cachedWidth != width || s_cachedHeight != height) {
            UpdateSubtitleBitmap(text, width, height);
        }

        if (s_pixelBuffer.empty()) return;

        // 2. 바이트 수 및 서페이스 형식 판별
        uint32_t surfaceType = binkFormatFlags & BINK_SURFACE_MASK;
        int bytesPerPixel = 4;

        if (surfaceType == BINK_SURFACE_565 || surfaceType == BINK_SURFACE_555 ||
            surfaceType == BINK_SURFACE_655 || surfaceType == BINK_SURFACE_664) {
            bytesPerPixel = 2;
        }
        else if (surfaceType == BINK_SURFACE_24 || surfaceType == BINK_SURFACE_24R) {
            bytesPerPixel = 3;
        }
        else if (surfaceType >= BINK_SURFACE_32 && surfaceType <= BINK_SURFACE_32RA) {
            bytesPerPixel = 4;
        }
        else {
            int absPitch = std::abs(destPitch);
            if (absPitch > 0 && width > 0) {
                int estimatedBpp = absPitch / width;
                if (estimatedBpp == 2 || estimatedBpp == 3 || estimatedBpp == 4) {
                    bytesPerPixel = estimatedBpp;
                }
            }
        }

        // 3. 캐시된 비트맵 픽셀을 게임 비디오 서페이스로 고속 복사
        uint8_t* baseDest = (uint8_t*)destBuffer + (destY * destPitch) + (destX * bytesPerPixel);
        const uint32_t* srcPixels = s_pixelBuffer.data();

        // 텍스트가 위치하는 하단 영역만 탐색하여 복사 속도 극대화
        uint32_t startY = height / 2; // 상단 절반은 자막이 없으므로 스킵

        for (uint32_t y = startY; y < height; y++) {
            const uint32_t* srcRow = srcPixels + (y * width);
            uint8_t* dstRow = baseDest + (y * destPitch);

            for (uint32_t x = 0; x < width; x++) {
                uint32_t srcColor = srcRow[x] & 0x00FFFFFF;
                if (srcColor == 0) continue; // 투명 영역 통과

                uint8_t r = (srcColor >> 16) & 0xFF;
                uint8_t g = (srcColor >> 8) & 0xFF;
                uint8_t b = srcColor & 0xFF;

                if (bytesPerPixel == 2) {
                    uint16_t* pDst16 = (uint16_t*)(dstRow + (x * 2));
                    if (surfaceType == BINK_SURFACE_555) {
                        *pDst16 = (uint16_t)(((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3));
                    }
                    else {
                        *pDst16 = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
                    }
                }
                else if (bytesPerPixel == 4) {
                    uint32_t* pDst32 = (uint32_t*)(dstRow + (x * 4));
                    if (surfaceType == BINK_SURFACE_32R || surfaceType == BINK_SURFACE_32RA) {
                        *pDst32 = (r) | (g << 8) | (b << 16);
                    }
                    else {
                        *pDst32 = srcColor;
                    }
                }
                else if (bytesPerPixel == 3) {
                    uint8_t* pDst24 = dstRow + (x * 3);
                    if (surfaceType == BINK_SURFACE_24R) {
                        pDst24[0] = r; pDst24[1] = g; pDst24[2] = b;
                    }
                    else {
                        pDst24[0] = b; pDst24[1] = g; pDst24[2] = r;
                    }
                }
            }
        }
    }
};