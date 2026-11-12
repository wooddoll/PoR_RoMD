# Steam판(SNEG 재출시) 텍스트 출력 분석

`PoR_RoMD_steam` 한글 패치 작업 중 Ghidra(por2main.exe / sneg.dll / ddraw.dll)로 분석한 내용 정리.
주소는 Ghidra 기준: **por2main.exe** = 0x00400000, **sneg.dll** = 0x59460000 (코드에서는 `MB(오프셋)`), **ddraw.dll** = 0x596e0000.

> **주소 읽는 법 (중요)**: sneg.dll 은 Ghidra 이미지 베이스를 중간에 바꿔서 절마다 기준이 다르다.
> - 1~9절: sneg = `0x59460000` (`FUN_594xxxxx` / `FUN_595xxxxx`)
> - 10절 이후: sneg = `0x561b0000` (`FUN_561xxxxx` / `FUN_562xxxxx` / `FUN_563xxxxx`)
> - 예: 같은 함수가 `FUN_5952a050`(1~9절) = `FUN_5627a050`(14절). **오프셋 = 주소 − 그 절의 베이스**는 같다.
> - ddraw 는 계속 `0x596e0000`, exe 는 `0x00400000`. 실행 중 주소는 `error.log`의 `[SNEG.DLL] baseAddress`로 환산.
> - 다른 PC에서는 베이스가 또 다를 수 있으니 오프셋(RVA)으로 찾는다. `ghidra_scan/ApplyPoRAnnotations.java`는 RVA 기준이라 영향 없음.

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
  - 949 이면: Gothic TTF 교체 안 함(0/1250/1252 아님) → 한글 아틀라스 유지에 유리.
  - 원본 isChina 는 950만 참. version.dll 이 950 비교/PUSH 를 949로 바꾸므로(아래 "950→949 패치") 한국어도 isChina 참.

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
    **코드페이지 == 950(0x3B6)일 때만** (원본) `languages/zh-hant/sneg_ui_cjk_characters.txt`의 문자.
  - 글꼴 = `%WINDIR%\Fonts\msjh.ttc`, 없으면 `msyh.ttf`. 949면 아틀라스 폭 4096.
  - `FUN_597b7a30(atlas, ttf, 48.0f, cfg, ranges)` = ImGui `AddFontFromFileTTF`.
- ~~중국어가 `?`로 나오는 이유: ddraw는 949일 때만 CJK 목록을 읽는다~~ — **잘못된 분석**.
  당시 분석한 sneg.dll / ddraw.dll 은 950→949로 직접 고친 파일이었다(11/8). Steam 원본은 950 기준이라 중국어는 정상일 것.
  한국어용으로는 version.dll 이 메모리에서 950→949 로 바꾼다 (11절 "950→949 패치").
- 언어 이름(繁體中文, 日本語)이 어느 언어에서나 보이는 건 이 7자가 항상 들어가기 때문.

### 한국어 언어팩 — 적용함 (11절)

처음 세운 계획과 진행 상태:

1. ✅ `languages\ko\language.ini` 추가: `[Language]` `CodePage=949`.
2. ✅ `languages\ko\sneg_ui_strings.dat` 추가. 지금은 번역 없이 루트 영어 dat에 언어 이름 키만 더함 (UI 문자열 번역은 남음).
3. ✅ ddraw의 949 분기를 한국어용으로 돌림 (version.dll 패치, 11절):
   문자 목록 경로 → `languages/ko/sneg_ui_cjk_characters.txt`, 글꼴 `msjh.ttc` → `languages\ko\MaruBuri-Regular.ttf` (없으면 맑은 고딕).
4. ⬜ 다른 언어가 선택된 상태의 언어 이름 `한국어`: 루트 dat에는 `ui.settings.language.ko` 키가 없으므로
   `FUN_59513bd0`(UiTextPoll)을 훅해서 대신 반환하고, 항상 넣는 7자 seed에 `한국어` 3자를 추가해야 한다
   (seed 문자열 참조 `5972ba96`/`5972baa4`를 우리 UTF-16 문자열로 바꾸기). 이때 seed 글꼴도 한글이 있어야 함.
5. ✅ 확인: `languages\zh-hant\language.ini` 원래 CodePage는 950이 아니라 테스트하면서 949로 바꿔 둔 상태였음(11절),
   언어 폴더에는 `sfDat.dat`, `data\…`(데이터 덮어쓰기), `sneg_ui_strings.dat`, `language.ini`가 들어 있음.

## 10. 시작 크래시 (인트로 영상 `movie010` 직후) — 해결

### 증상

- 게임을 켜면 인트로 Bink 영상(dnd_intro → wotc_intro → sfslogo00 → movie010)이 재생되고,
  마지막 `movie010`이 끝나면서 메인 메뉴를 초기화하다 죽는다. BugSplat 창이 뜨거나, 그냥 꺼지거나, 가끔은 정상.
- `error.log`에 `movie010.sbv` 로드 다음 `ripple.sbv` / `logo.sbv`(메인 메뉴 활성화 때 여는 영상)가 없으면 이 크래시.
- Bink / 자막과는 무관했다.

### 크래시 위치

- `sneg.dll+0xB5901` (sneg `FUN_562b5760` 기준 오프셋 0xB5760) = 게임 **0x4451f0 대체 함수** (알파 블렌딩, RGB565).
  등록: `FUN_56265a70` → `FUN_561f4420(0x451f0, FUN_56265760, …)`, 조건 `FUN_562d15e0`.
- 호출 경로: 메인 메뉴 Activate(`FUN_562bd940`) → cInterfaceManager Draw(`FUN_56282c80`) → 위젯 vtable +0x10.
- 대상 객체: cTextLayout 계열 위젯 (vtable 0x006231c4, +0x54 = `0x436440` = `[this+0xF0]` 서피스 반환).
  메인 메뉴 하단 라벨 (`y=0x249(585), w=400, h=400`, 텍스트 = 빈 문자열 `lpDefault`. 오른쪽 옆은 `"v1.29 11/7/2026"` 버전 라벨).
- 블렌딩 루프는 원본(0x4451f0)이나 sneg 대체 함수나 똑같이 **위젯 크기 `+0x30`(폭) × `+0x34`(높이)** 만큼
  소스 서피스를 **pitch 무시하고 촘촘히** 읽는다. 소스 서피스는 높이 15, pitch 1600(약 24KB)이었는데
  위젯 크기는 400×400 → 30번째 줄 288px(= 0x6000 바이트)에서 버퍼 끝을 넘어 AV.
  힙 배치에 따라 다음 페이지가 매핑돼 있으면 안 죽어서 "가끔 정상"이었다.
- 알파 값 `+0x134` < 0 이면 블렌딩 안 함. sneg는 `FUN_562b1910`의 핸들러 목록(고해상도 경로)이 먼저 처리하면 건너뜀.

### 원인: 우리 `cTextLayout_RenderShadow_Hook`이 빈 문자열에서 FALSE 반환

게임 SetText (`0x4445f0`, thiscall):

```
WordWrap(0x444890)        → +0x144 폭, +0x148 높이 계산
0x444cb0(ddraw)           → +0x144 × +0x148 로 서피스 생성, vtable +0x2c(=0x436340)로 [this+0xF0]에 저장
RenderShadow(0x444d40)    → 글자 그리기
  FALSE 면 여기서 리턴
vtable +0x20 / +0x4c …    → 위젯 폭/높이(+0x30/+0x34)를 서피스 크기로 갱신
```

- 우리 훅(sneg `RenderShadow_59515aa0` = `MB(0xB5AA0)` 자리)은 `text_len < 1`이면 FALSE를 반환했다.
  → 위젯 크기가 생성 시 값(400×400)으로 남고 서피스만 작게 만들어짐 → 위 블렌딩이 넘쳐 읽음.
- 원본/sneg RenderShadow는 빈 문자열에도 TRUE를 반환한다. 그래서 `patchLazyDLL`을 끄면 문제가 없었다.
- **수정**: [RenderShadow.cpp](PoR_RoMD_steam/RenderShadow.cpp) — 빈 문자열은 잠금만 풀고 TRUE. (`strlen` 전에 null 확인)
- **교훈**: 게임 함수를 대체할 때 **반환값의 의미(후속 처리 실행 여부)**까지 원본과 맞출 것.
  BOOL 반환 훅에서 "그릴 게 없음"은 실패가 아니라 성공이다. (WordWrap / CalcExtents / DrawText 계열도 같은 관점으로 점검할 것)

### 같이 확인/정리한 것

- **MSVC 런타임 불일치**: 게임 폴더의 `msvcp140.dll`은 14.29인데 `version.dll`은 v143(14.4x)로 빌드되어
  게임 폴더의 구버전을 먼저 로드했다 (`_Mtx_*`, `_Cnd_*`, `_Thrd_*` 임포트).
  11/9~10 이벤트 로그의 `MSVCP140.dll` 0xc0000005 / `ucrtbase.dll` 0xc0000409 크래시와 맞음.
  → Win32 구성을 **정적 CRT (`/MT`, `/MTd`)**로 변경 ([PoR_RoMD_steam.vcxproj](PoR_RoMD_steam/PoR_RoMD_steam.vcxproj)).
  지금 의존 DLL: `KERNEL32`, `USER32`, `GDI32`, `minhook.x32.dll`, `api-ms-win-core-path-l1-1-0.dll`.
  `minhook.x32.dll`(vcpkg 동적)은 계속 게임 폴더에 있어야 한다. `_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR` 정의는 그대로 둠.
- **자막 워커 스레드**(`subtitle_renderer.h`)는 원인이 아니었다. 자막이 바뀔 때 화면이 멈추는 문제 때문에 넣은 것이므로 유지.
- **sneg 주소**: 지금 Ghidra의 sneg.dll 이미지 베이스는 `0x561b0000` (1절의 0x59460000과 다름). 오프셋(`MB(…)`)은 같다.
  게임 실행 중 베이스는 매번 다름 → `error.log`의 `[SNEG.DLL] baseAddress`로 환산.
  sneg 쪽 `DAT_56396e70` = 게임 이미지 베이스(0x400000), 즉 `DAT_56396e70 + 0x327290` = 게임 `[0x727290]`.
- **sneg 고해상도 텍스트와의 관계**: `FUN_56275540`이 매 그리기마다 서피스가 32bpp인지와 배율(`FUN_56275830` = `MB(0xC5830)`)을 비교한다.
  우리가 배율을 0으로 고정했으므로 sneg가 만든 32bpp 서피스는 `FUN_56275310(obj, 0)`에서
  게임 `0x444cb0`(16bpp 재생성) + `0x444d40`(RenderShadow, 즉 우리 훅)으로 다시 만들어진다.
  → 우리 RenderShadow 훅은 SetText 말고 이 경로에서도 불린다.

### 디버깅 방법 메모

- VS로 `por2main.exe`를 디버그 실행하고 vs-mcp로 `debug_get_callstack` / `debug_evaluate @reg` / `memory_read`.
  sneg 기호가 없으므로 `주소 - 실행 중 베이스 + 0x561b0000`으로 Ghidra 주소 환산.
- 방어 훅이 삼키는 AV에도 VS는 첫 발생 시점에서 멈춘다 → 계속(F5), 또는 예외 설정에서 0xC0000005 해제, 또는 `Ctrl+F5`.
- 자막/화면 끊김 같은 성능 확인은 Release + 디버거 없이.

## 11. Prototype UI / 지도 툴팁의 한글 — 해결 (한국어 언어팩)

### 증상 (Language= 비어 있음 = 내장 영어, 1252)

- 첫 화면(메인 메뉴)과 아이템 툴팁 등 **반투명 창 위의 글자가 모두 `?`**. 영문은 정상.
- `sneg.ini` `[Ui] Prototype = 0`으로 끄면 2001년판처럼 한글이 잘 나옴 → Prototype UI 쪽 문제.
  (단 UI를 끄면 Steam판을 쓸 이유가 없으므로 켠 채로 고쳐야 함)
- 지도 마커 툴팁만은 Prototype을 꺼도 깨짐: `½Ì±Û`처럼 **한글 2바이트를 1바이트씩 cp1252로** 해석한 글자.

### 확인 순서 (진단 코드는 12절 표 참고)

1. WordWrap 진단: 들어오는 텍스트는 정상 EUC-KR, 글리프 2951개, 한글 조회 실패 0.
2. 폰트 아틀라스 서피스 덤프: 우리 한글 아틀라스(`font0.bmp` 1024×741 등)가 그대로 올라가 있음.
   → 게임 cFont 경로(우리 훅)는 정상. `?`는 다른 렌더러에서 나옴.
3. sneg는 게임 위젯 그리기(`0x4451f0`)에 핸들러 목록(`FUN_562b1860`로 등록, 최대 8개)을 끼워 둔다.
   핸들러 5개(`0x562b16e0`, `0x562b05f0`, `0x562b0bd0`, `0x5628e380`, `0x56304f80`)가 메인 메뉴 등의 위젯을
   **숨기고**(TRUE 반환), 같은 텍스트를 **ddraw의 ImGui 오버레이**가 대신 그린다.
   켜고 끄는 건 ddraw가 보내는 Prototype UI 기능 플래그(`FUN_56229bd0`, `DAT_5638fa24~34`).

### 원인 1: ddraw ImGui CJK 글꼴에 한글이 없음 → `?`

- `FUN_5972b2d0` (9절): 코드페이지 949일 때 CJK 글자 목록을 `<폰트폴더>\msjh.ttc`(없으면 `msyh.ttf`)로 아틀라스에 넣는다.
  두 글꼴 모두 한글이 없어서 ImGui가 `?`로 그림. (목록 파일에는 한글 2350자가 이미 있었음)
- **수정** (`ApplyDDrawPatches`, [sneg_hook.cpp](PoR_RoMD_steam/sneg_hook.cpp), `patchLazyDLL(L"ddraw.dll", …)`):
  - 폰트 폴더는 `%WINDIR%\Fonts`(`FUN_597252d0`)이고 경로 = 폴더 + `"/"` + 파일 이름 (`FUN_5971bff0`).
  - ddraw `+0x4B427`: `FF B5 24 FF FF FF` (`PUSH [EBP-0xDC]`, 폴더 `std::string*`) → `PUSH <우리 가짜 std::string>; NOP`.
    폴더 = `<게임>\languages\ko` (비 ASCII 경로면 8.3 짧은 이름. ImGui는 UTF-8, 존재 확인은 ANSI라서).
    MSVC x86 `std::string` 배치(+0 포인터, +0x10 길이, +0x14 용량 ≥16)를 흉내 낸 정적 구조체.
  - ddraw `+0x4B439`: `6A 08 68 <"msjh.ttc">` → `"MaruBuri-Regular.ttf"` (1순위, 언어팩 폴더)
  - ddraw `+0x4B500`: `6A 08 68 <"msyh.ttf">` → `"malgun.ttf"` (2순위, 폴더는 `%WINDIR%\Fonts` 그대로)
  - 글꼴: 마루 부리(MaruBuri, SIL OFL 1.1) — 설치기가 `languages\ko\`에 복사. 원본은 [font/MaruBuri-Regular.ttf](font/MaruBuri-Regular.ttf).
  - ddraw `+0x4BD79`: `6A 2C 68 <"languages/zh-hant/sneg_ui_cjk_characters.txt">` → `languages/ko/sneg_ui_cjk_characters.txt`
  - 바이트와 원래 문자열이 맞을 때만 패치. `error.log`: `[ddraw] CJK list -> …`, `[ddraw] CJK font dir -> …`,
    `[ddraw] CJK font -> MaruBuri-Regular.ttf`, `[ddraw] CJK fallback font -> malgun.ttf`.
- ddraw 이미지 베이스(Ghidra) = `0x596e0000`. 오프셋 = 주소 − `0x596e0000`.
- ddraw `[Text]` 설정은 `config.ini`에서 읽는다(`FUN_596f8530`). ddraw 코드페이지 `FUN_596f89b0` = `[Text] CodePage`(>0) 또는 언어 코드페이지.

### 950→949 패치 (원본 sneg.dll / ddraw.dll 의 CJK 분기를 한국어에도)

- SNEG 의 CJK 분기는 원래 **코드페이지 950(번체 중국어)** 전용. 테스트 중에는 sneg.dll / ddraw.dll 파일을 직접 고쳐 둔 상태였고,
  Steam 파일 검사로 원본이 돌아오자 첫 화면부터 다시 `?` 가 나왔다.
- `PatchCodePage950To949` ([sneg_hook.cpp](PoR_RoMD_steam/sneg_hook.cpp)): 즉시값 `B6 03 00 00` 의 첫 바이트를 `B5` 로.
  원래 값이 950일 때만 패치(이미 949면 통과), 아니면 `cp950 patch site mismatch`. 로그 `[sneg] … 9/9`, `[ddraw] … 3/3`.

| 모듈 | RVA (imm32 위치) | 내용 |
|---|---|---|
| sneg | 0x46696, 0x46926, 0xAE516, 0xAE60C | `cmp eax, 950` (isChina `FUN_562b6690` 등) |
| sneg | 0x46935, 0x4696D, 0xAE58D, 0xAE5BD, 0xAE662 | `push 950` (china 경로 `MultiByteToWideChar` 코드페이지) |
| ddraw | 0x4B5D4 | `FUN_5972b2d0` CJK 아틀라스 폭 4096 조건 |
| ddraw | 0x4BBC7 | `FUN_5972ba20` CJK 글자 목록 로드 조건 |
| ddraw | 0x8091A | `FUN_59760450` 요청 구조체 코드페이지 |

- 주의: sneg 의 isChina 로 정해지는 기능 등록(예: DrawMenu 그룹 `FUN_5952a050`)은 ddraw DllMain → `SNEG_Load` 시점,
  즉 version.dll 보다 먼저 끝난다. 파일 패치 때와 달리 그 등록은 950 기준 그대로지만, `HookGameIfNotHookedBySneg` 가 두 경우 모두 처리한다.

### 참고: 튜토리얼 대화창의 글꼴

- 튜토리얼 "DM:" 대사 일부는 Steam 원본에서도 **원작 비트맵 폰트(옛 UI)** 로 나온다 (version.dll 없이 확인).
- 11/11 23:17~23:20 스크린샷에서 매끈한 글꼴로 보인 건 `sneg_ui\fonts` 가 `fonts_backup` 으로 숨겨져 retail 글꼴을 못 찾던 상태였기 때문.
- ddraw 글꼴 슬롯: 슬롯 0~2 = `por_retail_font0~2` 외곽선, 3~4 = arial 경로(`por_retail_font3/4_arial.ink`), 모드 3. 시작 직후 정해지고 바뀌지 않음.

### 원인 2: 지도 툴팁은 sneg가 넘기는 "언어 코드페이지"로 변환 → Latin-1 글자

- 지도 마커 툴팁 = ddraw `FUN_5976c040` ← sneg `SNEG_MapFlagLabelPoll`(`FUN_562ad940`).
  구조체(0x13C 바이트) +0x18 = 코드페이지, +0x3C~ = 텍스트. ddraw는 `FUN_5973aee0(텍스트, 코드페이지)`로 UTF-8 변환.
- sneg가 채우는 곳: `FUN_562ada80` → `FUN_562ad9c0`. 코드페이지 = `FUN_56258f80()` =
  **선택된 언어(`languages\<폴더>`) 항목의 코드페이지** (`DAT_563c7eb8 + 0x18 + idx*0x1C`).
  `config.ini [Text] CodePage`가 아니다. 내장 영어면 1252 → 한글이 cp1252로 변환됨.
- 참고: ddraw 쪽 다른 텍스트(`FUN_59789190`)는 ddraw 코드페이지로 `IsDBCSLeadByteEx`를 검사해서,
  DBCS 선행 바이트가 있으면 retail 글꼴(1바이트 전용, `FUN_5978a200`/`FUN_59726b10`)을 포기하고 ImGui로 그린다 → 정상.
- `gamefont` 모드에서는 sneg `BridgeChineae_English`(`FUN_56276f00`)가 게임 폰트의 코드 0~255 글리프 256개만
  1비트 비트맵 표로 만들어 ddraw에 넘긴다 → 한글 불가. (FontMetrics 훅 때문에 한글 바이트는 '가'의 왼쪽 절반 조각으로 보임)
  따라서 `config.ini`의 `*Font=`는 원래 값(`retailfont`) 그대로 둔다.
- **수정**: 한국어 언어팩을 선택해서 언어 코드페이지 자체를 949로 만든다 (아래).

### 한국어 언어팩 `languages\ko\`

| 파일 | 내용 |
|---|---|
| `language.ini` | `[Language] CodePage=949`, `Release=Korean patch by wooddoll` |
| `sfDat.dat` | 한국어 sfDat |
| `data\Dialogue\`, `data\Interface\`, `data\STR\` | 한국어 데이터 (지금은 `zh-hant`에서 복사. 루트의 한국어 파일과 같은 내용) |
| `data\fonts\font0~4.bmp/.dff` | 한글 병합 폰트 (독일어 팩처럼 언어 폴더에 폰트를 둘 수 있음) |
| `sneg_ui_cjk_characters.txt` | ImGui CJK 글자 목록 (UTF-8, 한글 2350자 + 기호) |
| `sneg_ui_strings.dat` | 루트 영어 dat 기반, 언어 코드 `ko`, `ui.settings.language.ko = 한국어` 추가 |

- 선택: `%LOCALAPPDATA%\SNEG\POR2\game_settings.ini` `[Text] Language=ko` (설정창에서 바꾸면 다음 실행부터).
- `config.ini [Text] CodePage=949`도 유지 (ddraw 코드페이지, sneg TTF 교체 조건 회피).
- SUIS 읽기/쓰기 도구: [install/sneg_ui_strings.py](install/sneg_ui_strings.py) (`python sneg_ui_strings.py <dat> [키 일부]`).
  헤더의 언어 코드 문자열은 **u16** 길이 접두.
- `languages\zh-hant\`는 원래 중국어 팩인데 테스트하면서 `language.ini`를 949로, `data`를 한국어로 바꿔 둔 상태.
  `sneg_ui_strings.dat`는 중국어 그대로. 배포 전에 원본으로 복구해야 함 (12절).

### 남은 것

- 다른 언어가 선택된 상태의 언어 목록: 루트 dat에 `ui.settings.language.ko`가 없어서 `[[키]]`, 그리고 949가 아니면
  CJK 목록을 안 읽으므로 글자가 `?`일 수 있음. → 9절 계획 4번 (UiTextPoll 훅 + seed 7자에 `한국어` 추가).
- 언어팩 번역: `sneg_ui_strings.dat`의 UI 문자열(설정창 등)은 아직 영어.
- 루트 파일(`sfDat.dat`, `data\Dialogue`, `data\Fonts` 등)이 한국어로 덮어써져 있음 → 영어 원본으로 복구하고 언어팩만 쓰도록.
- 설치기([install.py](install/install.py)): 게임 폴더에 `sneg_ui`가 있으면 SNEG판(`installSneg`), 없으면 2001년판(`install2001`).
  - `install/Data/version_2001.dll` / `version_sneg.dll` 중 맞는 것을 `version.dll`로 복사 (SNEG판 dll은 `PoR_RoMD_steam` Release 빌드).
  - SNEG판: `languages\ko\`에 `language.ini`, `sfDat.dat`, `data\Dialogue`, `data\STR`, `data\fonts`(원본 `Data\Fonts`로 빌드),
    `MaruBuri-Regular.ttf`(+ 있으면 `MaruBuri-OFL.txt`), `sneg_ui_cjk_characters.txt`, `sneg_ui_strings.dat`(루트 dat + `한국어` 키).
    자막 `.sbv`는 영상과 같은 `Data\Bink`에 추가. `%LOCALAPPDATA%\SNEG\POR2`의 `config.ini [Text] CodePage=949`,
    `game_settings.ini [Text] Language=ko`를 `WritePrivateProfileStringW`로 기록.
  - SNEG판은 exe 검사(`exe_test.json`)를 건너뛰고 `sneg.dll` 존재만 확인.
  - 남은 것: `install/Data/version_sneg.dll` 넣기, OFL 라이선스 파일, `Data/Interface`(zh-hant에 있던 `Main.bmp`) 처리 여부,
    Steam판 영어 sfDat(1.4, 200034 바이트)와 한국어 sfDat(2001 기준)의 문자열 수 차이(`[Strings] Map=`) 확인.
    → sfDat 헤더에 문장마다 오프셋이 있어서 id 어긋남 문제는 없을 것으로 봄 (2026-11-12).

## 12. 배포판에서 빼거나 바꿀 것

| 항목 | 위치 | 처리 |
|---|---|---|
| `CrashLogVEH` + `AddVectoredExceptionHandler` (`crash.log` 기록) | [dllmain.cpp](PoR_RoMD_steam/dllmain.cpp) | **제거**. 개발용 진단. 첫 발생 예외(정상 처리되는 AV 포함)까지 기록하고, VEH가 항상 먼저 돈다 |
| `plog::init(plog::debug, "error.log")` | dllmain.cpp `InitHookThread` | 레벨을 `plog::warning` 이상으로 낮추거나 끄기. 지금은 Bink `.sbv` 없음까지 ERROR로 매번 기록 |
| `[Subtitle] Not found` ERROR 로그 | [bink_hook.cpp](PoR_RoMD_steam/bink_hook.cpp) `DetourBinkOpen` | 자막 없는 영상이 정상이므로 DEBUG로 내리기 |
| `SnegAlphaBlend_Hook` (sneg 0xB5760 AV 방어 훅) | [sneg_hook.cpp](PoR_RoMD_steam/sneg_hook.cpp) | 근본 원인은 고쳤으므로 **선택**. 다른 빈 텍스트 경로 대비로 남겨도 해는 없음(로그 최대 5회). 빼려면 `ApplySNEGPatches`의 해당 블록과 함수 3개 삭제 |
| `Lock_Spy` / `#if 0` 디버그 블록 | [RenderShadow.cpp](PoR_RoMD_steam/RenderShadow.cpp) | 제거 (빌드엔 영향 없음) |
| 주석 처리된 sneg 바이트 패치들 | sneg_hook.cpp | 정리 (기록용이면 이 문서로 옮기기) |
| `DiagLogText` / `DiagDumpFontSurface` (텍스트 hex 로그, 폰트 서피스 BMP 덤프) | [WordWrap.cpp](PoR_RoMD_steam/WordWrap.cpp) | **제거**. 매 WordWrap마다 문자열 검사 + 처음 한 번 `diag_font_*.bmp` 저장 |
| `DiagLogByteCaller` (한글 바이트로 vtable GetCharRect 호출한 주소 기록) | [FontMetrics.cpp](PoR_RoMD_steam/FontMetrics.cpp) | **제거** |
| `DiagDDrawFontSlots` (ddraw 글꼴 슬롯 상태 감시 스레드) | [sneg_hook.cpp](PoR_RoMD_steam/sneg_hook.cpp) | **제거** (`ApplyDDrawPatches` 의 `CreateThread` 포함) |
| `DiagSnegScale` (고해상도 배율 플래그 기록), 실험 모드(`IsNativeCjkTest`, 14절) | [sneg_hook.cpp](PoR_RoMD_steam/sneg_hook.cpp), [por2_hook.cpp](PoR_RoMD_steam/por2_hook.cpp) | 진단은 **제거**. 실험 모드는 마커 파일이 없으면 꺼져 있으니 남겨도 되지만, 배포판에서는 빼는 쪽 권장 |
| `diag_font_*.bmp`, `crash.log` | 게임 폴더 | 삭제 |
| `languages\zh-hant\` 변경 (`language.ini` CodePage=949, `data` 한국어) | 게임 폴더 | 원본 중국어 팩으로 복구 |
| 루트 한국어 파일 (`sfDat.dat`, `data\Dialogue`, `data\Fonts` 등) | 게임 폴더 | 영어 원본으로 복구 (한국어는 `languages\ko`에만) |
| 설정 백업 (`config.ini.before_gamefont`, `config.ini.gamefont`, `game_settings.ini.before_ko`) | `%LOCALAPPDATA%\SNEG\POR2` | 삭제 |
| `sneg.ini.before_proto` | 게임 폴더 | 삭제 |
| 빌드 산출물 (`version.pdb`, `version.exp`, `version.lib`) | 게임 폴더 (OutDir) | 배포 패키지에서 제외. `version.dll` + `minhook.x32.dll`만 |
| 디버그 구성 빌드 | | 배포는 **Release|Win32**만 (Debug는 `/MTd` + 디버그 힙) |
| `ddraw.dll.bak`, `sneg.dll.bak`, `sneg.dll.backup`, `version.dll.bakup` 등 | 게임 폴더 | 설치기(`install/`)가 건드리지 않도록 확인 |

배포 전 확인:

- [ ] `dumpbin /dependents version.dll`에 `MSVCP140.dll` / `VCRUNTIME140.dll`이 없는지 (정적 CRT 유지)
- [ ] `patchLazyDLL(L"sneg.dll", ApplySNEGPatches)`와 `patchLazyDLL(L"ddraw.dll", ApplyDDrawPatches)`가 켜져 있는지 (테스트하면서 주석 처리한 적 있음)
- [ ] `error.log`에 `[ddraw] CJK font -> MaruBuri-Regular.ttf` 등 4줄이 나오고 `mismatch`가 없는지
- [ ] 새 PC(VS 없음)에서 인트로 영상 → 메인 메뉴 진입 몇 번 반복

## 13. 테스트 체크리스트

- [ ] 메뉴 / 가로 메뉴 / 메뉴 `...` 생략과 마스크 위치
- [ ] 텍스트 박스, 라벨 박스 크기 (CalculateTextExtents / CalcExtentTextLabel)
- [ ] 라벨이 고해상도 창 배율에서도 한글로 나오는지 (0xC5830 패치)
- [ ] 툴팁 박스 폭 (위젯 / 메뉴 호버 / 파티)
- [ ] `error.log`의 `already hooked by sneg` / `hooked directly` 로그
- [x] 인트로 영상 → 메인 메뉴 진입 (빈 문자열 라벨, 10절) — `ripple.sbv` / `logo.sbv` 로그까지 나오면 통과
- [ ] `error.log`에 `access violation swallowed`가 나오지 않는지 (나오면 다른 빈 텍스트 경로가 남아 있음)
- [ ] 인트로 `movie010` 자막이 끊김 없이 나오는지
- [x] Prototype UI(`[Ui] Prototype = 1`)에서 첫 화면 / 반투명 창 툴팁이 한글로 나오는지 (11절)
- [x] 지도 마커 툴팁이 한글로 나오는지 (`Language=ko`)
- [x] 설정창 언어 목록에 `한국어`가 나오는지 (`Language=ko` 상태)
- [ ] 다른 언어를 선택한 상태에서 언어 목록의 `한국어` 표시
- [ ] 마루 부리로 새 UI 한글이 나오는지 (`languages\ko\MaruBuri-Regular.ttf`), 지우면 맑은 고딕으로 떨어지는지
- [ ] 게임 경로에 한글이 들어간 경우 (8.3 짧은 이름 처리)
- [ ] 설치기: 깨끗한 SNEG판 / 2001년판 각각 설치

## 14. [실험] sneg 자체 CJK(china) 경로로 한글 출력 — 보류 (2026-11-12)

### 동기

- 950→949 패치로 sneg 가 한국어를 번체 중국어처럼 다루게 되었으니, sneg 의 china 렌더 경로(문자열 → 유니코드 → GDI TTF)를 쓰면
  우리 비트맵 한글 폰트 병합과 텍스트 훅(WordWrap/RenderShadow/DrawMenu/…)이 대부분 필요 없고, 고해상도로 매끈하게 나올 것이라는 가정.

### 실험 모드 (코드는 남겨 둠, 기본은 꺼짐)

- 켜는 법: 게임 폴더에 `kr_native_test.txt` 를 둔다. **첫 줄 = china 글꼴 높이 배율(%)**, 50~150, 기본 71. 파일을 지우면 원래(훅) 방식.
- `IsNativeCjkTest()` ([sneg_hook.cpp](PoR_RoMD_steam/sneg_hook.cpp)) 가 참이면:
  - 950→949 패치는 그대로, ddraw CJK/마루부리 패치도 그대로.
  - 끄는 것: 배율 0 고정(`0xC58B0`, `0xC5830`), sneg 쪽 텍스트 훅 7개, 게임 입구 직접 훅, 알파 블렌딩 가드.
  - 켜 두는 것: FontMetrics vtable 훅 (게임 쪽 줄 나누기가 1바이트씩 폭을 재므로) — [por2_hook.cpp](PoR_RoMD_steam/por2_hook.cpp).
  - DrawMenu 그룹(`FUN_5627a050`) 재등록: `SNEG_Load` 때 isChina(950 기준)로 건너뛰므로 949 패치 뒤에 직접 호출.
  - china 글꼴 교체: `FUN_561f6740` 의 `CreateFontW(L"Microsoft JhengHei", CHINESEBIG5)` / 확인 문자 U+7D93 / 대체 `L"Microsoft YaHei"`
    → `L"MaruBuri Regular"`(languages\ko 의 TTF 를 `AddFontResourceExW(FR_PRIVATE)`), HANGUL_CHARSET, U+AC00, 대체 맑은 고딕.
    (sneg RVA `0x467B4`, `0x467B9`, `0x467CA`, `0x46805`)
  - sneg IAT `CreateFontW`(sneg+`0x1A7068`) 훅: 마루부리/맑은 고딕 HANGUL 글꼴만 높이 × 배율.
  - 진단: `DiagSnegScale` — 고해상도 배율 플래그와 `FUN_562758b0` 결과를 10초마다 기록.
- 설정: `%LOCALAPPDATA%\SNEG\POR2\config.ini` `[Text] ScaledEngineText=1` 필요 (백업 `config.ini.before_scaled`).

### 알게 된 것

- china 렌더: `china_pass`(`FUN_561f68d0`)가 949 리드 바이트면 `MultiByteToWideChar` 로 한 글자 변환 →
  `FUN_561f66a0`(높이별 GDI 글꼴 캐시) → `FUN_561f63d0` 에서 `TextOutW` 로 DIB 에 그려 글리프 캐시.
  폭은 `GetTextExtentPoint32W`. sneg WordWrap(`WordWrap_594a7f70`)도 isChina 면 이 폭으로 줄을 나눈다.
- 저해상도 경로(`FUN_561f6c50`)는 800×600 서피스에 그리고 확대 → 크고 거침. 알파를 `> 0x7F` 로 이진화.
- 고해상도 배율 함수가 둘이다:
  - `FUN_562758b0`(메뉴 계열): `ScaledEngineText || isChina` (+ `c8`,`ca`,`cb`, `[sneg+0x1E44EF]`, 레거시 아님, 창 배율 ≥ 1.01).
  - `FUN_56275830`(cTextLayout/라벨): isChina 를 안 봄 → `ScaledEngineText=1` 이어야 대화/라벨이 고해상도.
  - 플래그 주소: `0x563943c8`(c8) / `c9`(=ScaledEngineText) / `ca` / `cb`, 레거시 `0x563e926c`. 이 PC 는 c8=ca=cb=ef=1, 창 배율 1.8.
- `version.dll` DllMain 은 ddraw DllMain 보다 **늦게** 돈다(exe 임포트 순서 DDRAW → VERSION). `[early] sneg.dll at DllMain: already loaded`.
  → DLL 로드 알림으로 `SNEG_Load` 전에 패치하는 방법은 안 된다 (코드는 남아 있고, 먼저 돌면 자동으로 동작).
  `SNEG_Load` 전에 끼려면 ddraw 의 `[Plugin] GameDll` 을 우리 DLL 로 바꿔 sneg 를 대신 로드하는 등 다른 방법이 필요.

### 결과

| 항목 | 결과 |
|---|---|
| 첫 화면 / 툴팁 / 지도 툴팁 (ddraw ImGui) | 정상 (원래부터 ImGui 경로) |
| 대화 / 메뉴 / 라벨 / 한글 입력 | 한글은 나옴. `ScaledEngineText=1` + 배율 71% 로 매끈하고 크기도 비슷 |
| 줄바꿈 | **깨짐**: 게임/sneg 가 비트맵 폭 기준으로 1차 줄바꿈, china 렌더가 TTF 폭으로 2차 줄바꿈 → 짧은 줄이 끼고 잘림. FontMetrics 를 켜도 해결 안 됨 |
| 영문 | 대화 속 영문(예: `DM`)은 비트맵 글리프 확대(거침) — TTF 와 섞여 일관성 없음 |
| 툴팁 한글 | ImGui 아틀라스를 48px 로 만들고 축소해 찍어서 약간 흐림 (별개 문제) |

### 결론 / 다음에 할 것

- 게임 레이아웃(줄 나누기, 박스 크기, 메뉴 위치)은 비트맵 글리프 폭 기준이고 sneg 는 글자 이미지만 바꿔 끼우는 구조라,
  TTF 폭과 비트맵 폭을 완전히 맞추기 어렵다. **이 방식은 보류**, 본선은 기존 훅 방식(비트맵 한글 폰트 + 텍스트 훅).
- 다시 시도한다면: 줄을 나누는 모든 곳(게임 cTextManager 의 줄 분할, CalculateTextExtents, sneg WordWrap)이 렌더와 **같은 폭 함수**를 쓰도록
  맞추거나(예: FontMetrics 가 `china_pass` 폭을 돌려주게), china 렌더의 2차 줄바꿈을 끄는 쪽을 찾아야 한다.
- 별개로 남은 개선: 툴팁 한글 흐림(ImGui CJK 아틀라스 48px, `FUN_5972b2d0` 의 `0x42400000` → 표시 크기에 가깝게, 또는 오버샘플링),
  대화창이 원작 비트맵이라 거친 것(Steam 원본도 같음).

### 이 PC 상태 메모 (2026-11-12 새벽)

- 게임 폴더에 `kr_native_test.txt`(첫 줄 71) 가 **남아 있음** → 지우면 기존 훅 방식으로 돌아감.
- `config.ini` 는 `ScaledEngineText=1` 로 바뀐 상태 (원래 값은 `config.ini.before_scaled`).
- Steam 파일 검사로 sneg.dll / ddraw.dll / 루트 데이터는 원본. 한국어는 `languages\ko` 와 version.dll 메모리 패치로만 동작.
- 배포 전 제거 목록(12절)에 `DiagSnegScale`, 실험 모드 코드 정리 여부를 추가로 검토.
