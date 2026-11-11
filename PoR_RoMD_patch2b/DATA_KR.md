# Data_kr 리다이렉트 (2001년 원본판 전용)

원본 `Data` 폴더와 `sfDat.dat`를 건드리지 않고, 한글 파일을 `<게임 폴더>\Data_kr`에 두고 쓰는 방식.

## 동작

`version.dll`(이 프로젝트)이 `DllMain`(DLL_PROCESS_ATTACH)에서 **por2main.exe의 IAT**를 바꾼다 (`data_redirect.cpp`).

| KERNEL32 import | 처리 |
|---|---|
| `CreateFileA` | 읽기 전용(`GENERIC_WRITE` 등 쓰기 권한 없음) + `OPEN_EXISTING`일 때만 대체. 저장 파일 쓰기 등은 그대로 |
| `GetFileAttributesA` | 대체 |
| `FindFirstFileA` | 와일드카드 없는 경로(존재/크기 확인, CRT `_stat`)는 대체. 와일드카드 목록은 원본 폴더를 그대로 열거하되 파일 크기/시간을 Data_kr 파일 기준으로 바꿈 |
| `FindNextFileA` / `FindClose` | 위 목록의 크기 보정, 핸들 정리 |

경로 대응 (대소문자 무시, 상대 경로는 `GetFullPathNameA`로 절대 경로로 바꿔서 비교):

| 게임이 여는 경로 | 대신 여는 경로 (있을 때만) |
|---|---|
| `<게임>\Data\<하위경로>` (`./Data/Fonts/font0.dff`, `data\str\TEE.str`, `.\Data\Dialogue\xxx.por` …) | `<게임>\Data_kr\<하위경로>` |
| `<게임>\<파일>` (루트 파일, 예: `sfDat.dat`) | `<게임>\Data_kr\<파일>` |
| 그 밖의 하위 폴더, `Data_kr` 자체 | 대상 아님 |

- Data_kr에 없는 파일은 원본을 그대로 연다. 그래서 `Dialogue\all.db`, `all.idx` 같은 원본 파일은 Data_kr에 복사할 필요가 없다.
- 결과는 경로별로 캐시한다 (실행 중에 Data_kr 내용이 바뀌는 경우는 고려하지 않음).
- 자막(`.sbv`)은 이 DLL이 직접 읽으므로 `bink_hook.cpp`의 `DetourBinkOpen`에서 `DataRedirect_Resolve`로 `Data_kr\Bink\<movie>.sbv`를 먼저 찾는다.
- 사운드(mss32)와 동영상(binkw32)은 각자 파일을 열기 때문에 대상이 아니다 (번역 대상 파일도 없음).
- `Data_kr` 폴더가 없으면 아무것도 패치하지 않는다.

### 왜 IAT인가

예전 `PatchResourcePath()`(`por2_hook.cpp`, 지금은 호출 안 함)처럼 경로 문자열 참조를 하나씩 바꾸면, 로더가 여러 곳에 흩어져 있어서
(폰트 `0x0043ffd1`, 대화 `0x005b3832…`, STR `0x0051bd4e`, `0x0052d80d`, `0x0052f493`, `0x0052f772`, Rand `0x0052a8c2` …) 빠지는 곳이 생긴다.
exe의 파일 열기는 결국 정적 CRT → `CreateFileA`(KERNEL32 import)로 모이므로 IAT 한 곳에서 처리하면 빠짐이 없다.

### 시점

`version.dll`은 exe의 정적 import라서, `DllMain`이 실행될 때 exe IAT는 이미 채워져 있고 `WinMain`은 아직 시작 전이다.
그래서 스레드(`InitHookThread`)가 아니라 `DllMain`에서 바로 설치한다 (경쟁 없음). 설정도 `IniConfig` 대신 `GetPrivateProfile*`로 직접 읽는다.

## 설정 (`<게임 폴더>\language.ini`, 없어도 됨)

```ini
[Settings]
DataPath=Data_kr      ; 대체 폴더 이름 (게임 폴더 기준), 기본 Data_kr
DataRedirect=1        ; 0 이면 리다이렉트 끔
```

## 설치 레이아웃 (`install/install.py`)

```
<게임 폴더>\
  version.dll, minhook.x32.dll
  Data_kr\
    sfDat.dat
    Fonts\font0~4.bmp/.dff      (원본 Data\Fonts + 한글 글리프로 생성)
    Dialogue\*.por              (1398개)
    STR\*.str                   (44개)
    Bink\*.sbv                  (10개)
```

- 이전 버전(v1.0) 인스톨러가 만든 `Data\Fonts_backup`, `Dialogue_backup`, `STR_backup`, `sfDat.dat.backup`이 있으면
  원본으로 되돌릴지 묻는다 (되돌리면 `Data\Bink`에 넣었던 `.sbv`도 지움). 되돌리지 않으면 폰트는 `Fonts_backup`의 원본으로 만든다.
- `Data_kr`가 이미 있으면 지우고 새로 설치할지 묻는다.
- 제거: `Data_kr`, `version.dll`, `minhook.x32.dll` 삭제.

## 빌드/배포 체크

- `install/Data/version.dll`은 **이 변경을 넣어 다시 빌드한 DLL로 바꿔야 한다** (지금 들어 있는 건 이전 빌드).
- 이번 변경에서 `dllmain.cpp`의 `LoadSettings()`와 `ApplyMemoryPatches()` 호출을 다시 켰다 (steam 작업 때 주석 처리돼 있었음).

## 테스트

- [ ] 원본 Data 폴더 그대로 + Data_kr 설치 후 대사/아이템 이름/폰트/자막이 한글로 나오는지
- [ ] 저장/불러오기 정상 (쓰기는 리다이렉트하지 않음)
- [ ] `Data_kr` 폴더 이름을 바꾸면 원본 영어로 실행되는지
- [ ] 디버그 로그(`LogLevel` 5)에 `[DataRedirect] ... -> ...` 가 찍히는지
