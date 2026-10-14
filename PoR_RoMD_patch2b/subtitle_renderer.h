#pragma once
#include <windows.h>
#include <string>
#include <cstdint>
#include <algorithm>

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
public:
    static void RenderSubtitleToBuffer(
        void* destBuffer, int32_t destPitch, uint32_t width, uint32_t height,
        uint32_t destX, uint32_t destY, uint32_t binkFormatFlags, const std::wstring& text)
    {
        if (text.empty() || !destBuffer || width == 0 || height == 0) return;

        // 1. 실제 서페이스 포맷 감지 (마스크 적용)
        uint32_t surfaceType = binkFormatFlags & BINK_SURFACE_MASK;

        // 2. 바이트 수(Bytes Per Pixel) 판별 (플래그 및 Pitch 기반 크로스 체크)
        int bytesPerPixel = 4; // 기본 32비트

        if (surfaceType == BINK_SURFACE_565 || surfaceType == BINK_SURFACE_555 ||
            surfaceType == BINK_SURFACE_655 || surfaceType == BINK_SURFACE_664) {
            bytesPerPixel = 2; // 16비트
        }
        else if (surfaceType == BINK_SURFACE_24 || surfaceType == BINK_SURFACE_24R) {
            bytesPerPixel = 3; // 24비트
        }
        else if (surfaceType >= BINK_SURFACE_32 && surfaceType <= BINK_SURFACE_32RA) {
            bytesPerPixel = 4; // 32비트
        }
        else {
            // 플래그로 애매한 경우: 실제 destPitch와 width 비율로 계산
            int absPitch = std::abs(destPitch);
            if (absPitch > 0 && width > 0) {
                int estimatedBpp = absPitch / width;
                if (estimatedBpp == 2 || estimatedBpp == 3 || estimatedBpp == 4) {
                    bytesPerPixel = estimatedBpp;
                }
            }
        }

        // 3. GDI 32bpp 메모리 DC 생성
        HDC hdcScreen = GetDC(NULL);
        HDC hdcMem = CreateCompatibleDC(hdcScreen);

        BITMAPINFO bmi = { 0 };
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = width;
        bmi.bmiHeader.biHeight = -(LONG)height; // Top-Down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        uint32_t* pGdiPixels = nullptr;
        HBITMAP hBmp = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, (void**)&pGdiPixels, NULL, 0);
        HGDIOBJ hOldBmp = SelectObject(hdcMem, hBmp);

        // 투명 배경 초기화
        memset(pGdiPixels, 0, width * height * 4);

        // 4. 폰트 설정
        int fontSize = height / 20;
        if (fontSize < 18) fontSize = 18;

        HFONT hFont = CreateFontW(
            fontSize, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"맑은 고딕");
        HGDIOBJ hOldFont = SelectObject(hdcMem, hFont);

        SetBkMode(hdcMem, TRANSPARENT);

        // 자막 영역 (하단 배치)
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

        // 5. 비디오 서페이스 버퍼에 정확한 stride로 복사
        // Bink는 destx, desty 오프셋을 dest 버퍼에 적용해야 함
        uint8_t* baseDest = (uint8_t*)destBuffer + (destY * destPitch) + (destX * bytesPerPixel);

        for (uint32_t y = 0; y < height; y++) {
            uint32_t* srcRow = pGdiPixels + (y * width);
            uint8_t* dstRow = baseDest + (y * destPitch);

            for (uint32_t x = 0; x < width; x++) {
                uint32_t srcColor = srcRow[x] & 0x00FFFFFF;
                if (srcColor == 0) continue; // 투명 영역은 통과

                uint8_t r = (srcColor >> 16) & 0xFF;
                uint8_t g = (srcColor >> 8) & 0xFF;
                uint8_t b = srcColor & 0xFF;

                if (bytesPerPixel == 2) {
                    // 16비트 (RGB565 또는 RGB555)
                    uint16_t* pDst16 = (uint16_t*)(dstRow + (x * 2));
                    if (surfaceType == BINK_SURFACE_555) {
                        *pDst16 = (uint16_t)(((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3));
                    }
                    else {
                        // 기본 RGB565
                        *pDst16 = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
                    }
                }
                else if (bytesPerPixel == 4) {
                    // 32비트 (XRGB / ARGB / BGR)
                    uint32_t* pDst32 = (uint32_t*)(dstRow + (x * 4));
                    if (surfaceType == BINK_SURFACE_32R || surfaceType == BINK_SURFACE_32RA) {
                        // RGBA / RGB
                        *pDst32 = (r) | (g << 8) | (b << 16);
                    }
                    else {
                        // BGRA / BGR (Windows 기본 포맷)
                        *pDst32 = srcColor;
                    }
                }
                else if (bytesPerPixel == 3) {
                    // 24비트 (RGB24 / BGR24)
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

        // GDI 자원 해제
        SelectObject(hdcMem, hOldFont);
        DeleteObject(hFont);
        SelectObject(hdcMem, hOldBmp);
        DeleteObject(hBmp);
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);
    }
};