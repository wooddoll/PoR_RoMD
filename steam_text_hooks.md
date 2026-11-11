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
| FUN_59527c70 | 0x442590 → DrawTextTextLabel_59527720, 0x4429b0 → FUN_595278a0, 0x4421f0 → FUN_59527480 | `596443c9` (1) 또는 `5969925a` |
| FUN_5952a050 | 0x43b4f0 → DrawMenu_59529ea0, 0x43bc80 → DrawTextLines_59529d90, 0x43f1b0 → FUN_59529c50, 0x43d1f0 → FUN_595297f0, 0x43f090 → FUN_59529a40 | `596443c9` 또는 isChina |
| FUN_5952cc00 | 0x43a4d0 → KeyInputDraw_5952c600 | `596443c9` 또는 `5969925a` |
| FUN_59576f30 | 0x436a50 → FUN_59576cc0 (위젯 툴팁), 0x43dfc0 → FUN_59576ae0 (메뉴 호버 툴팁), 0x390e0 → FUN_59576a50 | FUN_5959dff0 / FUN_5959dfe0 |
| FUN_5955ce80 | 0x476820 → SprintfMeasure_5955cb10, 0x473b20 → FUN_5955c080 | FUN_595b1100 |
| FUN_59515490 / FUN_59515a70 | 0x4451f0 → FUN_59515300 / FUN_59515760 (블렌딩, 텍스트 아님) | |

- 설정 플래그 getter: `0x595815d0 ~ 0x59581760` (`MOV AL,[addr]; RET`).
  기본값 `0x596443c8~cb` = `01 01 01 01`, `0x5969925x`는 bss(설정 파일에서 채움).
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

## 7. Ghidra 스캔 도구

- `ghidra_scan/FindFontMetricCalls.java` : 디컴파일러 p-code로 `CALLIND [vtable+0x1C/0x20/0x24]`를 찾고 인자를 분류.
  실행: Script Manager에서 인자로 출력 파일 경로(선택: 시작/끝 주소 hex)를 준다.
  결과: `ghidra_scan/fontcalls_exe.txt`, `ghidra_scan/fontcalls_sneg.txt` (함수, 호출 주소, H/W/R, 인자 분류).
- sneg.dll 프로젝트에는 exe 이미지가 0x0003xxxx 영역에 겹쳐 있으므로 그 영역 결과는 무시.
- Ghidra 12.x: 기본 Python 엔진이 PyGhidra로 바뀜 (NationalSecurityAgency/ghidra#8555).
  Python 스크립트는 `support\pyghidraRun.bat`으로 실행한 Ghidra에서 사용. 12.1.4에서는 `# @runtime Jython`만으로는 안 되고 Jython 확장이 필요.
  사용자 스크립트 폴더를 Ghidra 실행 중에 만들면 placeholder 번들이 되어 Java 스크립트도 실패 → 재시작으로 해결.

## 8. 테스트 체크리스트

- [ ] 메뉴 / 가로 메뉴 / 메뉴 `...` 생략과 마스크 위치
- [ ] 텍스트 박스, 라벨 박스 크기 (CalculateTextExtents / CalcExtentTextLabel)
- [ ] 라벨이 고해상도 창 배율에서도 한글로 나오는지 (0xC5830 패치)
- [ ] 툴팁 박스 폭 (위젯 / 메뉴 호버 / 파티)
- [ ] `error.log`의 `already hooked by sneg` / `hooked directly` 로그
