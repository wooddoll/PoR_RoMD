# Ghidra 스크립트 (Steam판 분석용)

분석 내용 자체는 [`../steam_text_hooks.md`](../steam_text_hooks.md) 참고. 이 폴더는 그 내용을 Ghidra 프로젝트에 다시 적용하거나 재조사할 때 쓰는 스크립트다.

| 파일 | 용도 |
|---|---|
| `ApplyPoRAnnotations.java` | 구조체, 함수 이름, 프로토타입, 주석, 데이터 라벨을 **por2main.exe / sneg.dll / ddraw.dll**에 적용 |
| `FindFontMetricCalls.java` | cFont vtable +0x1C/+0x20/+0x24 간접 호출을 전부 찾아 인자가 1바이트 읽기인지 분류 |
| `fontcalls_exe.txt`, `fontcalls_sneg.txt` | `FindFontMetricCalls` 결과 (2026-11-11) |

## 다른 PC에서 같은 작업 하기

1. Steam 설치 폴더의 `por2main.exe`, `sneg.dll`, `ddraw.dll`을 Ghidra 프로젝트에 import 하고 기본 Auto Analysis를 돌린다.
   - 표의 주소는 이 이미지 베이스 기준: por2main.exe `0x00400000`, sneg.dll `0x59460000`, ddraw.dll `0x596e0000`.
   - **Ghidra에서 이미지 베이스를 바꿨어도 된다.** 모든 주소를 `주소 - 표 기준 베이스 + 현재 프로그램 이미지 베이스`
     (즉 RVA)로 바꿔서 적용한다. 현재 이미지 베이스는 콘솔 첫 줄 `base=`에 찍힌다.
   - **바이너리 빌드가 다르면 RVA가 달라진다.** 그래서 적용 전에 앵커를 확인하고, 하나라도 다르면 아무것도 하지 않고 멈춘다
     (`ABORT: anchors do not match ...`). 그래도 적용하려면 스크립트 인자로 `force`를 준다.

     | 프로그램 | 앵커 |
     |---|---|
     | por2main.exe | cFont vtable 슬롯 +0x1C/+0x24 값이 GetCharHeight/GetCharRect 주소와 같음, DrawMenu 입구 바이트 `81 EC` |
     | sneg.dll | export RVA: `SNEG_Load` 0x442F0, `SNEG_LanguagesPoll` 0x70E00, `SNEG_UiTextPoll` 0x79250 |
     | ddraw.dll | export RVA: `DirectDrawCreate` 0x2CE0, `POR_BuildEdition` 0x4C960 |
2. 스크립트를 Ghidra 사용자 스크립트 폴더(기본 `%USERPROFILE%\ghidra_scripts`)에 복사한다.
   - 폴더를 Ghidra 실행 중에 새로 만들었으면 Ghidra를 재시작하거나 Script Manager → Bundle Manager에서 새로고침
     (안 하면 `GhidraPlaceholderBundle cannot be cast` 오류).
3. **sneg.dll은 먼저** Analysis → Auto Analyze 옵션에서 `Non-Returning Functions - Discovered`를 끈다.
   - sneg.dll은 `operator delete`(0x595d28fa), `operator new`(0x595d28ca), `MakeChinaFont`가 noreturn으로 잘못 표시된다.
     그러면 디컴파일이 delete/new 호출마다 `Subroutine does not return`으로 잘린다. 스크립트가 이 표시를 풀고 호출 지점을 다시 디스어셈블한다.
   - 분석기를 켜 두면 다시 디스어셈블할 때 자동 분석이 돌면서 noreturn을 또 붙인다. 이미 붙었으면 끄고 스크립트를 한 번 더 실행.
   - 보조 스크립트: `FixNoReturn.java list`로 noreturn 함수를 나열할 수 있다 (사용자 스크립트 폴더에만 있음, 필요하면 같은 방식으로 만든다).
4. Script Manager의 `PoR_RoMD` 카테고리에서 `ApplyPoRAnnotations.java`를 **세 프로그램에서 각각** 실행한다.
   프로그램 이름(`por2main*`, `sneg*`, `ddraw*`)으로 적용할 표를 고른다. 콘솔에 `done: renamed N, signatures N, comments N, labels N, errors 0`이 나오면 정상.
5. 각 프로그램을 저장한다 (Ctrl+S).

## 스크립트가 하는 일

- **구조체** (`/PoR` 카테고리, 매번 교체): `RECT`, `MsvcString`, `cGlyph`, `cCharCodeMap`, `cFont_vtable`, `cFont`,
  `cGlyphResult`, `cRenderTarget`, `CWidgetBase`, `CKeyInput`, `cTextLayout`, `cTextLabel`, `cTextManager`,
  `SnegLanguageEntry`, `SnegLanguagesPollItem`, `SnegLanguagesPoll`, `SnegTextSettings`, `SuisHeader`,
  `SnegCharMetric`, `SnegCharItem`, `RetailInkHeader`, `RetailArialInkHeader`.
- **noreturn 수정** (sneg.dll): 위 3번 참고.
  - 오프셋은 `PoR_RoMD_steam/por2.h`와 같다. 확인 안 된 영역은 비워 둠(undefined).
- **함수 이름**: 이름이 `FUN_`/`DAT_`/`LAB_`인 경우에만 바꾼다. 직접 붙인 이름(예: `DrawMenu_59529ea0`)은 그대로 둔다.
- **프로토타입**: `__thiscall`은 사용자 지정 저장소(this = ECX, 스택 인자는 +4부터)로 적용해서 `this` 타입이 구조체 포인터로 보인다.
  그 밖의 호출 규약은 일반 함수 정의로 적용.
- **주석**: 함수 plate 주석은 `[PoR]`로 시작. 패치 위치에는 EOL 주석(`PoR_RoMD patch: ...`).
- **데이터 라벨**: sneg 설정 플래그(`g_NativeFont`, `g_HeadingFace`, `g_ScaledEngineText` …), 언어 테이블(`g_Languages`),
  ddraw의 `g_CjkSeedChars` 등. cFont vtable(0x0062250C)은 `cFont_vtable` 타입으로 지정.
- 여러 번 실행해도 된다(이미 적용된 이름은 건너뜀).

## 적용 확인 목록

주소는 표 기준 베이스 기준이다. 베이스를 바꿨다면 RVA(= 주소 - 표 기준 베이스)를 현재 베이스에 더해서 찾는다.

| 프로그램 | 주소 | 기대 결과 |
|---|---|---|
| por2main.exe | 0x0043f1b0 | `bool __thiscall cTextManager_DrawHMenu(cTextManager* this, int unused)`, 본문에 `this->m_nColumnMaxBytes` |
| por2main.exe | 0x00435e60 | `RECT* __thiscall …GetCharRect(cFont* this, int code)` |
| sneg.dll | 0x594d0e00 | `int SNEG_LanguagesPoll(SnegLanguagesPoll* out)`, 본문에 `out->items[0].flags` |
| sneg.dll | 0x59529c50 | `Sneg_DrawHMenu` |
| sneg.dll | 0x595258b0 | `GetMenuTextScale`, EOL `PoR_RoMD patch: D9 EE C3` |
| sneg.dll | 0x59519240 | `RetailFont_LoadTtfInk`, 디컴파일이 `operator_delete_sized` 뒤에서 잘리지 않음 |
| ddraw.dll | 0x5972ba20 | `BuildCjkGlyphSet`, 주석에 `codepage 949 (0x3B5)` |

## 표를 고칠 때

`ApplyPoRAnnotations.java`의 `applyExe()` / `applySneg()` / `applyDdraw()`에 한 줄씩 추가한다.
주소는 **표 기준 베이스로 환산해서** 적는다 (지금 Ghidra 베이스가 다르면 `주소 - 현재 베이스 + 표 기준 베이스`).

```java
fn(주소, "이름", "__thiscall", "반환형", "cTextManager* this, int arg", "[PoR] 설명");   // 호출 규약 null 이면 이름/주석만
lbl(주소, "g_이름", "EOL 주석");
eol(주소, "EOL 주석");
typed(주소, "구조체이름", "라벨", "EOL 주석");
```

타입 문자열은 `int uint short ushort char byte bool float dword word wchar void`, `/PoR` 구조체 이름,
뒤에 `*`(포인터), 구조체 필드에서는 `char[16]` 같은 배열을 쓸 수 있다.

## Python(PyGhidra) 관련

Ghidra 12.x는 기본 Python 엔진이 PyGhidra다 (NationalSecurityAgency/ghidra#8555).
Python 스크립트를 쓰려면 `support\pyghidraRun.bat`으로 Ghidra를 실행해야 한다. 여기 스크립트는 모두 Java라서 상관없다.
