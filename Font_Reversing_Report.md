# Direct3D 8 / Win32 게임 비트맵 폰트 엔진 리버스 엔지니어링 & 한글화 패치 종합 보고서

## 1. 개요 및 데이터 구조

### 1.1 폰트 파일 포맷 (`./data/fonts/font<0~4>.dff`, `.bmp`)
- **.bmp**: 24bpp 흑백 비트맵 시트 (아틀라스 크기는 임의 가로x세로 가능, 서피스 동적 생성).
- **.dff**: 32비트 정수 기반 글리프 메트릭 및 정렬 매핑 테이블.
  - 헤더: `[글리프 총 개수][가로 크기][세로 크기]`
  - 글리프 영역 테이블: `[code, x0, y0, x1, y1] * 글리프 개수`
  - 문자 매핑 테이블: `[코드-인덱스 쌍 개수]`, 그 뒤 `[code, index] * 개수`
  - `CFontCharMap::InsertMapping (0x00435410)`을 통해 오름차순 삽입 정렬 배열을 구성함. (32비트 int를 지원하므로 임의의 2바이트 한글 코드도 정상 수용)

### 1.2 `cFont` 클래스 레이아웃 (크기: 0x2C)
- `+0x00`: `vftable` (가상 함수 테이블 주소: `0x0062250c`)
- `+0x04`: 로드된 글리프 개수 (`m_nGlyphCount`)
- `+0x08`: 글리프 정보 포인터 배열 (`FontGlyph** m_ppGlyphs`)
- `+0x0C`: 문자 코드 매핑 맵 (`CFontCharMap* m_pCharMap`)
- `+0x10`: Unknown (`0xFFFFFFFF` 초기화)
- `+0x14`: DirectDraw 서피스 포인터 (`IDirectDrawSurface* m_pSurface`)
- `+0x18`: GDI 메모리 DC 핸들 (`m_hDC`)
- `+0x1C`: GDI DIBSection 핸들 (`m_hBitmap`)
- `+0x20`: DIB 비트 버퍼 포인터 (`m_pBits`)
- `+0x24`: 텍스처 가로폭 (`m_nTextureWidth`)
- `+0x28`: 텍스처 세로폭 (`m_nTextureHeight`)

### 1.3 `cFont` vftable (`0x0062250c`)
- `+0x00` (`0x004356f0`): `CreateFontDIB`
- `+0x04` (`0x00435950`): `RasterizeGlyphs`
- `+0x08` (`0x004359b0`): `UploadDIBToSurface`
- `+0x0C` (`0x00435c10`): `CreateSurface`
- `+0x10` (`0x00435c70`): `Reset`
- `+0x14` (`0x00435d00`): `AddGlyph`
- `+0x18` (`0x00435dd0`): `SetSurface`
- `+0x1C` (`0x00435e00`): `GetCharHeight(int code)`
- `+0x20` (`0x00435e30`): `GetCharWidth(int code)`
- `+0x24` (`0x00435e60`): `GetCharRect(int code)` -> `RECT*` 반환
- `+0x28` (`0x005484e0`): `GetSurface()` -> 서피스 포인터 반환

---

## 2. 텍스트 파이프라인 3대 핵심 함수

1. **`CTextBox::DrawTextLines` (`0x0043bc80`)**
   - DirectDraw 서피스를 Lock하고 폰트 아틀라스 서피스에서 글자 사각형(`GetCharRect`)을 읽어 픽셀 단위 Blt 복사를 수행하는 화면 렌더러.
   - 글자마다 `GetCharWidth`를 호출하여 커서 X좌표 전진, 루프 끝에서 `INC EBX` 수행.
2. **`CTextBox::CalculateTextExtents` (`0x0043bf40`)**
   - 문자열 배열의 글자들을 순회하며 `GetCharHeight`, `GetCharWidth`를 호출해 텍스트 박스의 최대 가로폭(`m_nTotalWidth`)과 라인 높이를 계산.
3. **`CTextLayout::WordWrap` (`0x00444890`)**
   - 문장이 지정된 폭(`GetMaxWidth()`)을 초과하면 이전 공백(띄어쓰기) 위치까지 백트래킹(되감기)하며 단어 단위로 줄을 분할하는 레이아웃 함수.

---

## 3. 한글(2바이트) 처리를 위한 기술적 장벽 및 해결책

### 문제점
- 원본 코드가 `char` 단위로 1바이트씩만 읽고, 루프 인덱스를 `+1`씩만 증가시킴.
- `GetCharHeight`와 `GetCharWidth` 시작부에 `MOVSX EAX, byte ptr [ESP+4]`가 있어서 상위 바이트를 잘라먹음.

### 해결책 (하이브리드 패치: 메모리 패치 + MinHook)
- **실행 파일(`exe`)을 직접 수정하지 않음.**
- **`d3d8.dll` 프록시 DLL**을 빌드하여 게임 실행 시 `Direct3DCreate8` 진입점에서 메모리 패치 및 MinHook 후킹을 자동으로 주입.
  - 단순 함수(`GetCharHeight`, `GetCharWidth`, `DrawTextLines` 점프): 메모리 패치 (`ApplyMemoryPatches`)
  - 복잡한 루프(`CalculateTextExtents`, `WordWrap`): C++ MinHook 후킹 (`Hooked_CalculateTextExtents`, `Hooked_WordWrap`)

---

## 4. Visual Studio 프록시 DLL 소스 코드

### 4.1 `d3d8.def`
```def
LIBRARY "d3d8"
EXPORTS
    Direct3DCreate8 = Fake_Direct3DCreate8 @1
```

### 4.2 `dllmain.cpp`
```c++
#include <windows.h>
#include <stdio.h>
#include "MinHook.h"

typedef void* (WINAPI *tDirect3DCreate8)(UINT SDKVersion);
tDirect3DCreate8 oDirect3DCreate8 = NULL;

void InstallHooks();

extern "C" void* WINAPI Fake_Direct3DCreate8(UINT SDKVersion)
{
    if (!oDirect3DCreate8)
    {
        char szSystemPath[MAX_PATH];
        GetSystemDirectoryA(szSystemPath, MAX_PATH);
        strcat_s(szSystemPath, "\\d3d8.dll");

        HMODULE hOrigD3D8 = LoadLibraryA(szSystemPath);
        if (hOrigD3D8) {
            oDirect3DCreate8 = (tDirect3DCreate8)GetProcAddress(hOrigD3D8, "Direct3DCreate8");
        }

        // 안전한 진입 시점에 후킹 설치
        InstallHooks();
    }

    if (oDirect3DCreate8) {
        return oDirect3DCreate8(SDKVersion);
    }
    return NULL;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        break;
    case DLL_PROCESS_DETACH:
        MH_Uninitialize();
        break;
    }
    return TRUE;
}
```

### 4.3 `hooks.cpp`
```c++
#include <windows.h>
#include <vector>
#include "MinHook.h"

// 폰트 인터페이스
class cFont {
public:
    virtual void  Dummy00() = 0;
    virtual void  Dummy04() = 0;
    virtual void  Dummy08() = 0;
    virtual void  Dummy0C() = 0;
    virtual void  Dummy10() = 0;
    virtual void  Dummy14() = 0;
    virtual void  Dummy18() = 0;
    virtual int   GetCharHeight(int code) = 0; // +0x1C
    virtual int   GetCharWidth(int code)  = 0; // +0x20
    virtual RECT* GetCharRect(int code)   = 0; // +0x24
    virtual void* GetSurface()            = 0; // +0x28
};

typedef void(__cdecl *tEngineDelete)(void*);
tEngineDelete EngineDelete = (tEngineDelete)0x005ea8f2;

// [후킹 1] CTextBox::CalculateTextExtents (0x0043bf40)
struct CTextBox {
    void*  vftable;
    char   pad_00[0x138];
    int    m_nLineCount;
    int    m_rcBounds_left;
    int    m_nTotalWidth;
    int    m_rcBounds_top;
    char   pad_14C[0x40];
    char** m_ppLines;
    char   pad_19C[4];
    cFont* m_pFont;
};

typedef BOOL(__thiscall *tCalculateTextExtents)(CTextBox*);
tCalculateTextExtents fpOriginalCalculateTextExtents = NULL;

BOOL __fastcall Hooked_CalculateTextExtents(CTextBox* pThis, void* /*edx*/)
{
    int maxBoxWidth = 0;
    int maxLineHeight = 0;

    char** ppLines = *(char***)((char*)pThis + 0x198);
    cFont* pFont   = *(cFont**)((char*)pThis + 0x1A0);

    if (pThis->m_nLineCount > 0 && ppLines && pFont)
    {
        for (int lineIdx = 0; lineIdx < pThis->m_nLineCount; ++lineIdx)
        {
            const char* szLine = ppLines[lineIdx];
            if (!szLine) continue;

            int currentLineWidth = 10;

            for (int i = 0; szLine[i] != '\0'; )
            {
                unsigned char c = (unsigned char)szLine[i];
                int code = 0;
                int charBytes = 1;

                if (c >= 0x80) { // 한글 2바이트 (빅엔디안)
                    code = (c << 8) | (unsigned char)szLine[i + 1];
                    charBytes = 2;
                } else {         // ASCII
                    code = c;
                    charBytes = 1;
                }

                int h = pFont->GetCharHeight(code);
                if (maxLineHeight < h) maxLineHeight = h;

                int w = pFont->GetCharWidth(code);
                currentLineWidth += w;

                if (maxBoxWidth < currentLineWidth + 20) {
                    maxBoxWidth = currentLineWidth + 20;
                }

                i += charBytes;
            }
        }
    }

    pThis->m_rcBounds_left = 0;
    pThis->m_nTotalWidth   = maxBoxWidth;
    pThis->m_rcBounds_top  = 0;

    return TRUE;
}

// [후킹 2] CTextLayout::WordWrap (0x00444890)
struct CTextLayout {
    void*  vftable;
    char   pad_00[0x120];
    cFont* m_pFont;
    char*  m_pszText;
    char   pad_12C[4];
    int    m_nUnknown130;
    char   pad_134[4];
    int    m_nLineCount;
    int*   m_pLineEndIndices;
    int*   m_pLineWidths;
    int    m_nMaxLineWidth;
    int    m_nTotalHeight;

    int GetMaxWidth() {
        typedef int(__thiscall *tGetMaxWidth)(void*);
        return ((tGetMaxWidth)(*(void***)this)[0x48 / 4])(this);
    }
};

typedef BOOL(__fastcall *tWordWrap)(CTextLayout*);
tWordWrap fpOriginalWordWrap = NULL;

BOOL __fastcall Hooked_WordWrap(CTextLayout* pThis)
{
    int nBoxMaxWidth = pThis->GetMaxWidth();
    const char* szText = pThis->m_pszText;

    pThis->m_nTotalHeight  = 0;
    pThis->m_nMaxLineWidth = 0;

    if (!szText || szText[0] == '\0') {
        pThis->m_nLineCount = 0;
        return TRUE;
    }

    std::vector<int> lineEndIndices;
    std::vector<int> lineWidths;

    int currentLineWidth = 0;
    int maxLineWidth     = 0;
    int lineStartIdx     = 0;
    int lastSpaceIdx     = -1;
    int widthAtLastSpace = 0;

    int i = 0;
    while (szText[i] != '\0')
    {
        if (szText[i] == '\n') 
        {
            lineEndIndices.push_back(i - 1);
            lineWidths.push_back(currentLineWidth);
            if (maxLineWidth < currentLineWidth) maxLineWidth = currentLineWidth;

            currentLineWidth = 0;
            lastSpaceIdx = -1;
            i++;
            lineStartIdx = i;
            continue;
        }

        unsigned char c = (unsigned char)szText[i];
        int code = 0;
        int charBytes = 1;

        if (c >= 0x80) {
            code = (c << 8) | (unsigned char)szText[i + 1];
            charBytes = 2;
        } else {
            code = c;
            charBytes = 1;
        }

        if (code == ' ') {
            lastSpaceIdx = i;
            widthAtLastSpace = currentLineWidth;
        }

        int charW = pThis->m_pFont->GetCharWidth(code);

        if (currentLineWidth + charW > nBoxMaxWidth && i > lineStartIdx)
        {
            if (lastSpaceIdx != -1 && lastSpaceIdx > lineStartIdx) {
                lineEndIndices.push_back(lastSpaceIdx);
                lineWidths.push_back(widthAtLastSpace);
                if (maxLineWidth < widthAtLastSpace) maxLineWidth = widthAtLastSpace;
                i = lastSpaceIdx + 1;
            } else {
                lineEndIndices.push_back(i - 1);
                lineWidths.push_back(currentLineWidth);
                if (maxLineWidth < currentLineWidth) maxLineWidth = currentLineWidth;
            }

            currentLineWidth = 0;
            lastSpaceIdx = -1;
            lineStartIdx = i;
            continue;
        }

        currentLineWidth += charW;
        i += charBytes;
    }

    lineEndIndices.push_back(i - 1);
    lineWidths.push_back(currentLineWidth);
    if (maxLineWidth < currentLineWidth) maxLineWidth = currentLineWidth;

    int lineCount = (int)lineEndIndices.size();
    pThis->m_nLineCount    = lineCount;
    pThis->m_nMaxLineWidth = maxLineWidth;

    int* pNewEndIndices = new int[lineCount];
    int* pNewWidths     = new int[lineCount];

    memcpy(pNewEndIndices, lineEndIndices.data(), lineCount * sizeof(int));
    memcpy(pNewWidths, lineWidths.data(), lineCount * sizeof(int));

    if (pThis->m_pLineEndIndices) EngineDelete(pThis->m_pLineEndIndices);
    if (pThis->m_pLineWidths)     EngineDelete(pThis->m_pLineWidths);

    pThis->m_pLineEndIndices = pNewEndIndices;
    pThis->m_pLineWidths     = pNewWidths;

    return TRUE;
}

// [메모리 패치] GetCharHeight/Width 32비트화 및 DrawTextLines 인라인 패치
void ApplyMemoryPatches()
{
    DWORD oldProtect;

    // 1. cFont::GetCharHeight (0x00435e00) -> 8B 44 24 04 90
    VirtualProtect((void*)0x00435e00, 5, PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy((void*)0x00435e00, "\x8B\x44\x24\x04\x90", 5);
    VirtualProtect((void*)0x00435e00, 5, oldProtect, &oldProtect);

    // 2. cFont::GetCharWidth (0x00435e30) -> 8B 44 24 04 90
    VirtualProtect((void*)0x00435e30, 5, PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy((void*)0x00435e30, "\x8B\x44\x24\x04\x90", 5);
    VirtualProtect((void*)0x00435e30, 5, oldProtect, &oldProtect);

    // 3. CTextBox::DrawTextLines 내부 점프 패치
    VirtualProtect((void*)0x0043bd9b, 0x100, PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy((void*)0x0043bd9b, "\xE9\x60\x77\x1D\x00\x90", 6);
    memcpy((void*)0x0043be72, "\xE9\x89\x77\x1D\x00\x90\x90\x90\x90\x90\x90\x90\x90\x90", 14);
    VirtualProtect((void*)0x0043bd9b, 0x100, oldProtect, &oldProtect);

    // 4. Code Cave 본체 쓰기 (0x00613500)
    VirtualProtect((void*)0x00613500, 0x200, PAGE_EXECUTE_READWRITE, &oldProtect);

    const unsigned char cave1[] = {
        0x0F, 0xB6, 0x14, 0x18, 0x80, 0xFA, 0x80, 0x72, 0x0F, 0x0F, 0xB6, 0x44, 0x18, 0x01, 
        0xC1, 0xE2, 0x08, 0x0B, 0xD0, 0x8B, 0xC2, 0xEB, 0x03, 0x90, 0x0F, 0xBE, 0xC2, 0x8B, 
        0x11, 0xE9, 0x7F, 0x88, 0xE2, 0xFF
    };
    memcpy((void*)0x00613500, cave1, sizeof(cave1));

    const unsigned char cave2[] = {
        0x51, 0x0F, 0xB6, 0x14, 0x18, 0x80, 0xFA, 0x80, 0x72, 0x16, 0x0F, 0xB6, 0x44, 0x18, 
        0x01, 0xC1, 0xE2, 0x08, 0x0B, 0xD0, 0x52, 0x83, 0xC3, 0x02, 0x8B, 0x8E, 0xA0, 0x01, 
        0x00, 0x00, 0xEB, 0x0D, 0x0F, 0xBE, 0xC2, 0x50, 0x43, 0x8B, 0x8E, 0xA0, 0x01, 0x00, 
        0x00, 0xEB, 0x00, 0x8B, 0x11, 0xFF, 0x52, 0x20, 0x59, 0x8B, 0x4C, 0x24, 0x30, 0x03, 
        0xC8, 0xE9, 0x42, 0x88, 0xE2, 0xFF
    };
    memcpy((void*)0x00613600, cave2, sizeof(cave2));
    VirtualProtect((void*)0x00613500, 0x200, oldProtect, &oldProtect);
}

// [초기화 진입점]
void InstallHooks()
{
    ApplyMemoryPatches();

    if (MH_Initialize() == MH_OK)
    {
        MH_CreateHook((LPVOID)0x0043bf40, &Hooked_CalculateTextExtents, (LPVOID*)&fpOriginalCalculateTextExtents);
        MH_CreateHook((LPVOID)0x00444890, &Hooked_WordWrap, (LPVOID*)&fpOriginalWordWrap);
        MH_EnableHook(MH_ALL_HOOKS);
    }
}
```

## 5. 차후 작업 시 참고 사항
### 폰트 데이터 파일(.dff) 제작 시:
- 한글 완성형 코드(CP949)의 빅엔디안 값(예: '가' = 0xB0A1 = 45217)을 키로 등록할 것.
### 빌드 환경:
- Visual Studio에서 Win32(x86) Release 모드로 빌드.
- 프로젝트에 MinHook 라이브러리 링크 및 모듈 정의 파일(d3d8.def) 지정 필수.


## 6. 폰트 로드 경로 변경 (`./data/fonts/font%d` -> 커스텀 경로)

### 6.1 원본 호출 위치 어셈블리 분석
- **호출 함수**: `CFontManager_InitFonts_0043ffa0`
- **주소**: `0x0043ffcf ~ 0x0043ffd6`
```assembly
0043ffcf 50              PUSH  EAX                              ; 인자 3: 폰트 번호 (%d)
0043ffd0 68 38 48 6b 00  PUSH  0x006B4838                       ; 인자 2: "./data/fonts/font%d" 포맷 문자열 주소
0043ffd5 51              PUSH  ECX                              ; 인자 1: 출력 버퍼 (local_10c)
0043ffd6 e8 12 ae 1a 00  CALL  _sprintf_maybe                   ; sprintf 호출
```

### 6.2 구현 방식 비교
* [방식 1] .data 섹션 원본 문자열 제자리 덮어쓰기 (In-place Overwrite)
* 주소: 0x006B4838
* 공간 분석:
- 원본 문자열: "./data/fonts/font%d" (20바이트, 널 종료 포함)
- 뒤쪽 패딩: 0x00이 4바이트 존재 -> 총 24바이트 가용
- 변경할 문자열: "./data/fonts/font_kr%d" (23바이트, 널 종료 포함)
* 결과: 가용 공간(24바이트) 안에 23바이트가 들어가므로 제자리 수정 가능.

```c++
DWORD oldProtect;
VirtualProtect((void*)0x006b4838, 24, PAGE_READWRITE, &oldProtect);
memcpy((void*)0x006b4838, "./data/fonts/font_kr%d", 23);
VirtualProtect((void*)0x006b4838, 24, oldProtect, &oldProtect);
```

* [방식 2] 포맷 스트링 PUSH 포인터 교체 (★ 가장 추천!)
* 주소: 0x0043FFD1 (PUSH 0x68 바로 뒤의 4바이트 즉시값 주소)
* 장점:
- 원본 .data 섹션을 전혀 건드리지 않음.
- 문자열 길이 제한이 완전히 사라짐 (예: "./data/fonts/korean_hd/font%d" 등 긴 경로도 자유롭게 사용 가능).
* 구현 원리:
 - DLL 내부에 전역 문자열을 정의하고, 0x0043FFD1의 4바이트 값을 해당 전역 문자열의 메모리 주소(&g_szNewFontPath)로 교체.

### 6.3 최종 반영 코드 (hooks.cpp의 ApplyMemoryPatches에 추가)
```c++
// 1. 사용할 커스텀 폰트 경로 (길이 제한 없음)
const char g_szNewFontPath[] = "./data/fonts/font_kr%d";

void ApplyMemoryPatches()
{
    DWORD oldProtect;

    // ... (기존 GetCharHeight, GetCharWidth, DrawTextLines 패치 유지) ...

    // [추가] 폰트 경로 PUSH 인자 교체 (0x0043ffd1)
    const char* pNewPath = g_szNewFontPath;
    VirtualProtect((void*)0x0043ffd1, 4, PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy((void*)0x0043ffd1, &pNewPath, 4);
    VirtualProtect((void*)0x0043ffd1, 4, oldProtect, &oldProtect);
}
```
### 6.4 결과 및 장점
* 게임이 실행되면 ./data/fonts/font0.bmp, font0.dff 대신 ./data/fonts/font_kr0.bmp, font_kr0.dff (0~4번)를 자동으로 로드합니다.
* 영문 원본 폰트 파일을 덮어쓰거나 훼손하지 않고, 한글화 폰트를 독립적인 파일로 깔끔하게 관리할 수 있습니다.