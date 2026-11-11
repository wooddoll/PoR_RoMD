# Steam판(SNEG 재출시) 텍스트 출력 분석

`PoR_RoMD_steam` 한글 패치 작업 중 Ghidra(por2main.exe / sneg.dll / ddraw.dll)로 분석한 내용 정리.
주소는 Ghidra 기준: **por2main.exe** = 0x00400000, **sneg.dll** = 0x59460000 (코드에서는 `MB(오프셋)`), **ddraw.dll** = 0x596e0000.

---

## 1. 로드 순서와 훅 시점

```
por2main.exe 정적 import
 ├─ version.dll (우리 프록시)  DllMain → CreateThread(InitHookThread)   ← loader lock 해제 후 실행
 └─ ddraw.dll   (SNEG)         DllMain(FUN_596e2ad0) → FUN_596fce90
        GetPrivateProfileStringA("Plugin", "GameDll", "sneg.dll", …)
        LoadLibraryA("sneg.dll")
        GetProcAddress("SNEG_Load") → 즉시 호출
            SNEG_Load → FUN_594a5840 → FUN_594a4680   (기능별 등록 함수 연쇄 호출)
                → FUN_594a4420(게임 오프셋, 대체 함수, &원본)   ← MinHook CreateHook + Enable
```

- `InitHookThread`는 loader lock이 풀린 뒤에 돌기 때문에 **sneg 훅이 이미 전부 깔린 상태**에서 시작한다.
  `patchLazyDLL(L"sneg.dll", …)`은 이미 로드된 모듈을 보고 바로 패치한다.
- 따라서 `patchBYTE(MB(0xCA050), 0xC3)` (FUN_5952a050 등록 차단)은 **실제로는 효과 없음**.
  등록된 sneg 대체 함수 쪽을 훅해야 한다.
- 게임 함수 입구가 `E9`(sneg JMP)인지 보고, 아니면 게임 입구를 직접 훅한다 → `por2_hook.cpp`의 `HookGameIfNotHookedBySneg`.

## 2. sneg가 등록하는 텍스트 관련 훅

| 등록 함수 | 게임 주소 → sneg 대체 함수 | 조건 (기본값) |
|---|---|---|
| FUN_595149e0 | 0x43ffa0 → LoadFonts_59514810 | 항상 |
| FUN_594a7c80 | 0x444890 → WordWrap_594a7f70 | 항상 |
| FUN_59515f80 | 0x444d40 → RenderShadow_59515aa0 | `596443ca` (1) |
| FUN_59527c70 | 0x442590 → DrawTextTextLabel_59527720, 0x4429b0 → FUN_595278a0, 0x4421f0 → FUN_59527480 | ScaledEngineText(`596443c9`) 또는 HeadingFaceScaled(`5969925a`) |
| FUN_5952a050 | 0x43b4f0 → DrawMenu_59529ea0, 0x43bc80 → DrawTextLines_59529d90, 0x43f1b0 → FUN_59529c50, 0x43d1f0 → FUN_595297f0, 0x43f090 → FUN_59529a40 | ScaledEngineText(`596443c9`) 또는 isChina |
| FUN_5952cc00 | 0x43a4d0 → KeyInputDraw_5952c600 | ScaledEngineText(`596443c9`) 또는 HeadingFaceScaled(`5969925a`) |
| FUN_59576f30 | 0x436a50 → FUN_59576cc0 (위젯 툴팁), 0x43dfc0 → FUN_59576ae0 (메뉴 호버 툴팁), 0x390e0 → FUN_59576a50 | FUN_5959dff0 / FUN_5959dfe0 |
| FUN_5955ce80 | 0x476820 → SprintfMeasure_5955cb10, 0x473b20 → FUN_5955c080 | FUN_595b1100 |
| FUN_59515490 / FUN_59515a70 | 0x4451f0 → FUN_59515300 / FUN_59515760 (블렌딩, 텍스트 아님) | |

- 설정 플래그 getter: `0x595815d0 ~ 0x59581760` (`MOV AL,[addr]; RET`).
  `.data`의 `0x596443c8~cb` = `01 01 01 01`이지만, 시작할 때 `FUN_5957f090`(설정 읽기)이 덮어쓴다 (7절 참고).
  - `596443c9` = **ScaledEngineText** ([Text] `DefaultsRevision`≠0 이고 `ScaledEngineText`≠0, 또는 env `POR_SCALED_ENGINE_TEXT`). 설정이 없으면 **0**.
  - `5969925a` = **HeadingFaceScaled** (기본 1, HeadingFace 가 켜져 있을 때)
  - 따라서 ScaledEngineText 가 꺼져 있으면 FUN_5952a050 (DrawMenu 그룹)은 isChina 가 아닌 한 **등록되지 않고**,
    FUN_59527c70 / FUN_5952cc00 (TextLabel, KeyInput)은 HeadingFaceScaled 로 등록된다.
    → `HookGameIfNotHookedBySneg`가 두 경우를 모두 처리한다.
- 0x4429b0, 0x4421f0, 0x43d1f0, 0x43f090, 0x4451f0 대체 함수는 블릿/색 처리만 하고 글자를 그리지 않는다.

## 3. sneg Draw 함수의 두 경로

| 배율 함수 | 사용처 | 패치 |
|---|---|---|
| FUN_595258b0 (0xC58B0) | DrawMenu / DrawTextLines / FUN_59529c50 / FUN_5952a130 | `fldz; ret` (0 반환) |
| FUN_59525830 (0xC5830) | DrawTextTextLabel_59527720 / FUN_59527480 | `fldz; ret` (0 반환) |

- 배율 > 0 이면 `*_ani` 경로(D3D 고해상도)로 가고, 거기서 china 실시간 렌더링을 쓴다:
  `china_pass_594a68d0`, `MakeChinaFont_594a6c50`, `_V_china_FUN_594a7ca0 / 59522b70 / 59523f90 / 595249a0`,
  `_1_china_FUN_595236b0`, `_2_china_FUN_59524580`, `DrawText_ani_59528020`, `subDrawMenu_ani_59528e60`,
  `FUN_595287f0`, `FUN_59528ab0`.
- 두 배율 함수를 0으로 고정하면 기존 서피스 블릿 경로만 탄다.
  FUN_5952a130(0x43d1f0/0x43f090 대체 함수가 호출)의 ani 렌더도 이 조건 + DrawMenu 캐시 플래그(+0x20)가 없어서 실행되지 않음.

## 4. 폰트

- cFont 객체는 sneg LoadFonts도 exe의 `cFont_00435620`으로 만든다. vtable = **0x0062250C**.
  - +0x1C GetCharHeight_00435e00 (슬롯 0x00622528)
  - +0x20 GetCharWidth_00435e30 (슬롯 0x0062252C)
  - +0x24 GetCharRect_00435e60 (슬롯 0x00622530) — **글리프가 없으면 NULL 반환**
  - 세 함수 모두 `FindGlyphIndex_004355C0`만 호출하고, FindGlyphIndex 직접 호출은 이 세 곳뿐.
- 아틀라스에는 1바이트 글리프 256개(키 = signed char 값, -128~-1 포함)가 다 있다(`font.md`).
  → 한글 바이트를 1바이트씩 넘기면 크래시가 아니라 **라틴-1 글리프 폭/모양**이 나온다.
- 한글 2바이트 키 = `(c1 << 8) | c2` (0xA1A1~0xC8FE), 고정폭.
- **sneg 고해상도 폰트 옵션 주의**: `LoadFonts_59514810` → `about_font_59514c00`이 조건(FUN_59581650 플래그 &&
  코드페이지 0/1250/1252 등)을 만족하면 폰트의 charmap/서피스를 Gothic TTF 아틀라스로 교체한다. 켜지면 병합한 한글 아틀라스가 사라짐.

## 5. 1바이트씩 읽어서 폰트 메트릭을 부르는 함수 전체

`ghidra_scan/FindFontMetricCalls.java`로 vtable +0x1C/+0x20/+0x24 간접 호출을 전부 찾고, 인자가 1바이트 읽기
(`sext8` = MOVSX, `PIECE4` = `CONCAT31(쓰레기, byte)`, `load1`)인 것만 추렸다. (int 인자 호출은 대부분 위젯 setter라 제외)

| 게임 함수 | 용도 | 실제로 도는 코드 | 처리 |
|---|---|---|---|
| 0x43a4d0 KeyInputDraw | 입력창 | sneg KeyInputDraw_5952c600 | `CKeyInput_Draw_Hook` (sneg 함수 훅) |
| 0x444890 WordWrap | 줄바꿈 | sneg WordWrap_594a7f70 | `cTextLayout_WordWrap_Hook` |
| 0x444d40 RenderShadow | 그림자 텍스트 | sneg RenderShadow_59515aa0 | `cTextLayout_RenderShadow_Hook` |
| 0x442590 DrawTextTextLabel | 라벨 | sneg DrawText_59527cd0 (cdecl self, pDst) | `cTextLabel_DrawText_Hook` + 0xC5830 패치 |
| 0x43b4f0 DrawMenu | 메뉴 | sneg DrawMenu_59529ea0 | `cTextManager_MenuDraw_Hook` |
| 0x43bc80 DrawTextLines | 텍스트 박스 | sneg DrawTextLines_59529d90 | `cTextManager_DrawTextLines_Hook` |
| 0x43f1b0 | 가로 메뉴 (칸마다 첫 단어 가운데 정렬) | sneg FUN_59529c50 | `cTextManager_DrawHMenu_Hook` (MenuExtra.cpp) |
| 0x43b8e0 | 메뉴 마스크 (글자 모양대로 값 1, 원본은 문자열에 `...`을 직접 씀) | 원본 (sneg 등록 없음) | `cTextManager_DrawMenuMask_Hook` (게임 입구) |
| 0x43bf40 CalculateTextExtents | cTextManager 박스 크기 | 원본 | `cTextManager_CalculateTextExtents_Hook` (게임 입구) |
| 0x4428b0 CalcExtentTextLabel | 라벨 박스 크기 | 원본 | `cTextLabel_CalcExtents_Hook` (게임 입구) |
| 0x436a50 | 위젯 툴팁 폭 | sneg FUN_59576cc0 | FontMetrics (vtable) |
| 0x43dfc0 | 메뉴 호버 툴팁 폭 | sneg FUN_59576ae0 | FontMetrics (vtable) |
| 0x476820 SprintfMeasure | 파티 툴팁 폭 | sneg SprintfMeasure_5955cb10 또는 원본 | FontMetrics (vtable) |

- sneg 내부에서 1바이트를 읽는 함수 중 `FUN_5952d410`은 KeyInputDraw_5952c600 안에서만 쓰여서 함께 대체됨.
- `FUN_59523360`(vtable 0x59613460에서만 참조)은 아직 분석하지 않음.
- 툴팁 글자 자체는 cTextLayout(WordWrap/RenderShadow) 경로로 그려지므로 이미 처리됨. 툴팁 함수는 **박스 폭**만 계산.

### 호출 규약 (훅 시그니처)

| 함수 | 규약 |
|---|---|
| 0x43b4f0 DrawMenu, 0x43b8e0, 0x43bf40, 0x4428b0 | thiscall, 인자 없음, `RET` → `__fastcall(self, edx)` |
| 0x43bc80 DrawTextLines, 0x43f1b0 | thiscall, 스택 인자 1개(미사용), `RET 4` → `__fastcall(self, edx, int)` |
| sneg DrawText_59527cd0 | cdecl(self, pDst) |
| sneg DrawText_ani_59528020 | cdecl(self, pDst, float scale) |
| cFont vtable 메트릭 | thiscall(this, int code), `RET 4` |

## 6. FontMetrics (vtable 폭 보정)

- 우리 훅 코드는 por2.h의 인라인 `cFont::GetCharXXX`(FindGlyphIndex 직접 호출)를 쓰므로 이 vtable 훅을 거치지 않는다.
- 대체하지 않은 툴팁 폭 계산 함수만을 위한 것. 없어도 치명적이지 않다(박스 폭이 조금 어긋나는 정도, 200~300px 등으로 제한됨).
- **상태 없음**: 한글 바이트(0xA1~0xFE)는 호출마다 독립적으로 `'가'` 폭의 절반(올림)을 반환. 두 바이트 합 = 한 글자 폭
  (폭이 홀수면 글자당 +1px). Rect는 `'가'` 왼쪽 절반 크기의 고정 Rect.
- patch2b의 `HangulChannelState`(상태 머신)는 사용하지 않는다:
  - 원래 버전은 4바이트 읽기 패치로 다음 바이트(c2)를 받아야 동작하는데, 지금 호출부는 MOVSX라 c2 = 0xFF.
  - 리드/테일 교대 방식은 깨진 문자열이 섞이면 상태가 다음 호출로 넘어간다.

## 7. sneg 폰트 옵션과 TTF 사용처

### 설정 (`FUN_5957f090`, ini [Text] 섹션 / 환경변수)

| 키 (env) | 플래그 | 의미 |
|---|---|---|
| `NativeFont` (`POR_NATIVE_FONT`) | `59699258` FUN_595816b0 | 네이티브(china 실시간) 폰트. Legacy 모드면 강제 0 |
| `HeadingFace` (`POR_HEADING_FACE`), 기본 1 | `59699259` FUN_59581650 | 게임 폰트를 TTF(PoR Gothic)로 다시 만든다 |
| `AlternativeFont` | `59699261` FUN_59581720 | 켜면 Source Sans 3 사용 + 코드페이지 조건 무시 + 폰트 0~4 전부 교체 (꺼지면 0~2) |
| `HeadingFaceScaled` (`POR_HEADING_FACE_SCALED`), 기본 1 | `5969925a` | |
| `DefaultsRevision` + `ScaledEngineText` (`POR_SCALED_ENGINE_TEXT`) | `596443c9` | 고해상도 텍스트(`*_ani`) |
| `GameFontOutlines` (`POR_GAME_FONT_OUTLINES`) | `59699260` | |
| `CodePage` | `59699264` | 없으면 선택 언어의 코드페이지(`59699268`, 언어 테이블 +0x18) |
| `NativeFontFace` (`POR_FONT_FACE`), `NativeFontScalePercent` (`POR_FONT_SCALE`), `NativeFontCharSet` (`POR_FONT_CHARSET`), `NativeFontGroups` | | |
| LegacyMode / PuristMode (`POR_LEGACY`, `POR_PURIST`, SteamDeck), [Ui] `LegacyEverything` / `LegacyCrop` | `5969926c` 등 | Legacy 모드면 대부분의 새 기능이 꺼짐 |

### PoRGothic-Regular.ttf / SourceSans3-Regular.ttf (sneg.dll)

- `LoadFonts_59514810`(게임 0x43ffa0 대체) 끝과 `FUN_59514fb0`(설정이 바뀔 때 적용)에서 `about_font_59514c00`을 호출.
- 조건: `HeadingFace && !NativeFont && !Legacy(5969926c) && (AlternativeFont || 코드페이지 ∈ {0, 1252, 1250})`
- `about_font_59514c00` → `GothicTTF_load_59514db0`:
  `AddFontResourceExA(ttf, FR_PRIVATE)` → `CreateFontA`로 face 확인 → **게임 cFont의 vtable +0x10**(게임 자체의 GDI 래스터라이저)으로
  폰트를 원래 높이 × 배율로 다시 만든다. 원래 charmap(+0x0C)/서피스(+0x14)/아틀라스 크기는 백업해 두고,
  `FUN_59514fb0`에서 설정에 따라 원본 ↔ TTF 아틀라스를 바꿔 끼운다.
- **이게 켜지면 병합한 한글 아틀라스 대신 TTF 아틀라스가 쓰여서 한글이 사라진다.**
- 주의: `about_font_59514c00`은 `[0x727290]+0x18`의 `+0x48`(그래픽 컨텍스트)이 아직 없으면 아무것도 안 하고 리턴하는데,
  LoadFonts는 그 전에 `DAT_59677f70 = 1`을 세워서 이후 재시도하지 않는다. 시점에 따라 조건이 맞아도 로드되지 않을 수 있음.
- SourceSans3 경로 문자열은 테이블(`0x596131ec`, 0x14 간격: 키, 이름, face, 파일)로만 참조된다.

### por_retail_font%u.ttf / .ink (sneg.dll)

- `FUN_59524690` → `Load_retail_fonts_5951b000` (폰트 번호별, `.ttf` + `.ink` 외곽선, 옵션으로 `%WINDIR%\Fonts\arial.ttf` + `_arial.ink`).
  모드 플래그에 따라 retail TTF 대신 PoR Gothic/Source Sans로 만들기도 함 (`FUN_5951adc0`).
- 호출자는 `_V_china_FUN_59522b70`, `_1_china_FUN_595236b0`, `_2_china_FUN_59524580`, `FUN_59524070`, `FUN_59525930`.
  즉 **고해상도 텍스트(`*_ani`) 경로 전용** 고해상도 글리프 캐시다. NativeFont 가 켜져 있으면 안 쓴다.
- 우리 패치는 배율 함수 두 개(0xC58B0, 0xC5830)를 0으로 고정해서 이 경로를 막으므로 **retail 폰트는 로드되지 않는다**.

### PoRGothic-Regular.ttf (ddraw.dll)

- `FUN_5971d860`: ddraw의 새 UI(sneg_ui 오버레이: 메인/설정/업적 메뉴 등) 글꼴 선택.
  `"builtin"`(0) / `"gamefont"`(1) / PoRGothic(2, 파일이 있고 코드페이지 0/1252/1250일 때만) / 그 밖의 경로(3).
- 게임 텍스트(cFont)와는 별개.

## 8. Ghidra 스캔 도구

- **분석 결과를 Ghidra에 적용**: `ghidra_scan/ApplyPoRAnnotations.java` (구조체, 이름, 프로토타입, 주석, 라벨).
  다른 PC에서 다시 적용하는 방법은 [`ghidra_scan/README.md`](ghidra_scan/README.md).

- `ghidra_scan/FindFontMetricCalls.java` : 디컴파일러 p-code로 `CALLIND [vtable+0x1C/0x20/0x24]`를 찾고 인자를 분류.
  실행: Script Manager에서 인자로 출력 파일 경로(선택: 시작/끝 주소 hex)를 준다.
  결과: `ghidra_scan/fontcalls_exe.txt`, `ghidra_scan/fontcalls_sneg.txt` (함수, 호출 주소, H/W/R, 인자 분류).
- sneg.dll 프로젝트에는 exe 이미지가 0x0003xxxx 영역에 겹쳐 있으므로 그 영역 결과는 무시.
- Ghidra 12.x: 기본 Python 엔진이 PyGhidra로 바뀜 (NationalSecurityAgency/ghidra#8555).
  Python 스크립트는 `support\pyghidraRun.bat`으로 실행한 Ghidra에서 사용. 12.1.4에서는 `# @runtime Jython`만으로는 안 되고 Jython 확장이 필요.
  사용자 스크립트 폴더를 Ghidra 실행 중에 만들면 placeholder 번들이 되어 Java 스크립트도 실패 → 재시작으로 해결.

## 9. 언어 팩과 sneg_ui (설정창)

### 언어 목록 = `languages\<폴더>\language.ini` (sneg `FUN_59509040`)

- 시작할 때 `<게임 폴더>\languages\*`를 `FindFirstFileA`로 훑는다.
  - 폴더 이름: `[A-Za-z0-9_-]`만, 1~15자. `.`/`..` 제외.
  - `language.ini`의 `[Language] CodePage`가 0이 아니고 `IsValidCodePage`를 통과해야 등록.
  - 0번은 내장(원본 영어, 코드페이지 1252). 정렬 후 최대 16개.
- 항목 = `{ std::string 폴더이름, int 코드페이지 }` (0x1C 바이트), `DAT_59677eb8` 벡터.
- `language.ini`의 다른 키: `[Dialogue] TextField` (2~4), `[Spells] LevelLast`.
- 선택 언어: 명령행 `Language` → env `POR_LANGUAGE` → 설정 ini `[Text] Language`.
  **시작할 때 한 번만 적용**(`FUN_5950a400`) → 설정창에서 바꾸면 다음 실행부터 반영.
- 선택 언어 폴더에 있는 파일은 `FUN_5950d4f0(name)`이 `languages\<폴더>\<name>`으로 바꿔 준다(폴더 안 파일 목록 해시 `DAT_59677ef4`).
  → 번역 파일을 원본 대신 `languages\ko\`에 두는 방식이 가능해 보임 (게임 파일 열기 쪽 확인 필요).
- `GetCodepage_59581600` = ini `[Text] CodePage` 또는 선택 언어의 코드페이지.
  - `isChina_FUN_594a6690` = 코드페이지 == **950**(0x3B6)
  - 949 이면: isChina 아님, Gothic TTF 교체 안 함(0/1250/1252 아님) → 한글 아틀라스 유지에 유리.

### SNEG_LanguagesPoll (ddraw 설정창이 호출)

- 항목 0x18 바이트: `char name[16]`, `int codepage`, `flags` (bit0 = 시작 시 적용된 언어, bit1 = 다음 실행용으로 선택된 언어).
- ddraw는 bit0 언어의 코드페이지를 `DAT_599f102c`에 저장(`596fd5e4` → `FUN_596f89d0`).
  ddraw 코드페이지 = ini `[Text] CodePage` (>0이면) 또는 이 값 (`FUN_596f89b0`).
- 언어 이름 표시 키: `"ui.settings.language." + 폴더이름` (`FUN_5970a580`에서 `"%s%s"`).

### UI 문자열 = `sneg_ui_strings.dat` (sneg `FUN_59580880` → `FUN_59513400`)

- 선택 언어 폴더의 `sneg_ui_strings.dat`가 있으면 그것, 없으면 루트 파일. 시작할 때 한 번 로드.
- 포맷: `"SUIS"`(u32 0x53495553), u32 버전=1, 길이 접두 문자열 1개(`FUN_595143c0`, `DAT_5964405c`), u32 개수,
  항목마다 `u16 키길이, 키, u32 코드페이지, u16 값길이, 값`.
- `SNEG_UiTextPoll` → `FUN_59513bd0`: 종류 1 = sfDat 문자열 id, 종류 2 = 키 조회.
  키가 없으면 `"[[키]]"`를 코드페이지 65001로 반환.

### ddraw UI 글꼴 (Dear ImGui 아틀라스)

- `FUN_5971d860`: 기본 글꼴 선택 (`builtin` / `gamefont` / PoRGothic(코드페이지 0/1250/1252일 때만) / 경로).
- `FUN_5972b2d0(atlas, 폰트폴더, 코드페이지, ranges)`: CJK 글꼴 추가.
  - 글리프 집합 = `FUN_5972ba20`: 항상 `"繁體中文日本語"`(UTF-16, `0x5982da50`) 7자 +
    **코드페이지 == 949(0x3B5)일 때만** `languages/zh-hant/sneg_ui_cjk_characters.txt`의 문자.
  - 글꼴 = `%WINDIR%\Fonts\msjh.ttc`, 없으면 `msyh.ttf`. 949면 아틀라스 폭 4096.
  - `FUN_597b7a30(atlas, ttf, 48.0f, cfg, ranges)` = ImGui `AddFontFromFileTTF`.
- **중국어가 `?`로 나오는 이유**: sneg는 번체 중국어를 950으로 다루는데 ddraw는 949일 때만 CJK 목록을 읽는다.
  그래서 중국어 UI에는 언어 이름용 7자 말고는 글리프가 없다 (SNEG 쪽 버그로 보임).
- 언어 이름(繁體中文, 日本語)이 어느 언어에서나 보이는 건 이 7자가 항상 들어가기 때문.

### 한국어 추가 계획 (기존 파일은 건드리지 않음)

1. `languages\ko\language.ini` 새로 추가: `[Language]` `CodePage=949`.
2. `languages\ko\sneg_ui_strings.dat` 새로 추가: 루트 dat를 파싱해 한국어로 번역해서 같은 SUIS 포맷으로 생성.
3. ddraw의 949 분기를 한국어용으로 돌린다 (version.dll에서 패치):
   - 문자 목록 경로 → `languages/ko/sneg_ui_cjk_characters.txt` (한국어 UI에 쓰이는 글자, 또는 KS X 1001 2350자)
   - 글꼴 `msjh.ttc` / `msyh.ttf` → `malgun.ttf` (맑은 고딕은 한자 7자도 있음)
4. 언어 이름 `한국어`: 영어 등 다른 언어가 켜진 상태의 루트 dat에는 `ui.settings.language.ko` 키가 없으므로
   `FUN_59513bd0`(UiTextPoll)을 훅해서 대신 반환하고, 항상 넣는 7자 seed에 `한국어` 3자를 추가해야 한다
   (seed 문자열 참조 `5972ba96`/`5972baa4`를 우리 UTF-16 문자열로 바꾸기). 이때 seed 글꼴도 한글이 있어야 함.
5. 확인 필요: `languages\zh-hant\language.ini`의 CodePage 값, 언어 폴더에 실제로 들어 있는 파일, ImGui 설정(MergeMode) 위치.

## 10. 테스트 체크리스트

- [ ] 메뉴 / 가로 메뉴 / 메뉴 `...` 생략과 마스크 위치
- [ ] 텍스트 박스, 라벨 박스 크기 (CalculateTextExtents / CalcExtentTextLabel)
- [ ] 라벨이 고해상도 창 배율에서도 한글로 나오는지 (0xC5830 패치)
- [ ] 툴팁 박스 폭 (위젯 / 메뉴 호버 / 파티)
- [ ] `error.log`의 `already hooked by sneg` / `hooked directly` 로그
