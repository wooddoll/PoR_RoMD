# 🕹️ Pool of Radiance: Ruins of Myth Drannor (steam build id: 26503525) 유저 한글 패치 v2026.11.15

> [!IMPORTANT]
> **버전 호환성 안내**
> 본 한글패치는 2026년 SNEG 재출시판(Steam 등) 전용입니다.
> 선택한 게임 폴더에 `sneg_ui` 폴더가 있어야 설치됩니다. (2001년 원본은 지원하지 않습니다)

수정 및 업데이트 버전은 항상 <https://github.com/wooddoll/PoR_RoMD> 에서 배포하고 있습니다.

- 🌿 게임 중 출력되는 모든 대사와 아이템 이름, 오브젝트 이름이 번역되었습니다.
- 🌿 설정창, 툴팁, 튜토리얼 등 재출시판의 새 UI 문구도 번역되었습니다.
- 🌿 삽입된 시네마틱 동영상에 자막이 추가되었습니다.
- 🌿 휴식을 취하면 자동으로 'AUTOSAVE'라는 이름으로 저장됩니다.
- 🌿 F5를 누르면 'QUICKSAVE'라는 이름으로 저장되고, F6을 누르면 이 파일을 불러옵니다. 
이 키를 단축키로 지정했다면 지정된 명령과 SAVE/LOAD가 같이 실행됩니다. 
예기치 못한 문제가 있을 수 있으므로 해당 키의 단축키 지정은 피하세요.
---

## 📢 필독 및 동의 안내

본 한글패치 프로그램을 설치 및 사용하는 것은 아래의 모든 이용약관 및 주의사항에 동의한 것으로 간주됩니다.
내용을 반드시 읽고 동의하시는 경우에만 설치를 진행해 주시기 바랍니다.

### ✔ 지원 버전
2026년 SNEG 재출시판에 맞춰 제작되었습니다.
이후 게임 업데이트로 내부 구조가 바뀌면 동작하지 않을 수 있습니다.

### ✔ 정품 소유자 전용
본 한글패치는 해당 게임의 정품 패키지 또는 디지털 에디션 등을 정상적으로 구매한 사용자만 이용할 수 있습니다.

### ✔ 상업적 이용 및 유료 판매 금지
본 한글패치는 순수한 비영리 목적으로 제작된 창작물입니다.
본 패치 파일의 전체 또는 일부를 유료로 판매, 대여하거나 상업적인 목적으로 이용하는 행위는 엄격히 금지됩니다.

### ✔ 저작권 준수 안내
본 패치 파일은 자체 제작한 기술 코드와 순수 번역 데이터, 오픈소스 폰트로만 구성되어 있습니다.
이외 원본 게임의 데이터 등 타인의 저작물은 일절 포함하지 않았음을 밝힙니다.

### ✔ 면책 조항
본 패치는 비공식 유저 번역이므로, 시스템 환경이나 패치에 따라 구동이 불가해지거나 오류가 발생할 수 있습니다.
본 패치 설치 및 사용 과정에서 발생할 수 있는 게임 데이터 손상, 세이브 파일 충돌, 시스템 오류 등의 문제에 대해
제작자는 어떠한 법적·물질적 책임도 지지 않습니다.
(설치 전 원본 파일 및 세이브 데이터 백업을 권장합니다.)

---

## ✒️ 사용 폰트 안내

- 움돋 (Umdot, <https://monadabxy.com/fonts/umdot/>) - SIL Open Font License 1.1
- 갈무리 (Galmuri, <https://quiple.dev/font/galmuri>) - SIL Open Font License 1.1
- 마루 부리 (MaruBuri, <https://hangeul.naver.com/font>) - SIL Open Font License 1.1

## 📁 설치 방식

- 게임의 언어팩 구조를 이용해 `languages/ko` 폴더에 한국어 언어팩을 설치합니다.
  원본 파일은 수정하지 않고, `Data/Bink` 에 자막 파일(`.sbv`)만 추가합니다.
- 게임 설정을 한국어(코드페이지 949)로 지정합니다. 설정창 언어 목록에서 '한국어' 를 볼 수 있습니다.
- **패치 제거**: `languages/ko` 폴더, `version.dll`, `minhook.x32.dll` 을 지우고 설정창에서 다른 언어를 고르면 됩니다.

## 🔠 고해상도 글꼴

설치 중에 고해상도 글꼴을 사용할지 묻습니다.

- **켜면** 대사·메뉴·이름 등 게임 글자를 화면 해상도에 맞춰 매끈한 TTF(마루 부리)로 그립니다.
- **끄면** 원작처럼 800×600 비트맵 글꼴(움돋/갈무리)을 확대해서 보여 줍니다.
- 실험 중인 기능이라 화면에 따라 어색한 곳이 있을 수 있습니다.

나중에 끄려면 아래 두 값을 `0` 으로 바꾸거나, 인스톨러를 다시 실행해 `n` 을 고르세요.

| 파일 | 섹션 | 값 |
|---|---|---|
| `<게임 폴더>\languages\ko\language.ini` | `[Render]` | `HiRes=0` |
| `%LOCALAPPDATA%\SNEG\POR2\config.ini` | `[Text]` | `ScaledEngineText=0` |

## 🛠️ 기술 안내 및 알림

**백신 오진 관련 안내**
본 패치는 게임 실행 중 텍스트와 자막을 렌더링하기 위해 메모리 후킹(Hooking) 방식을 사용합니다.
이로 인해 일부 백신 프로그램에서 악성코드로 오진하여 파일(`version.dll`)을 차단할 수 있으니,
실행이 안 될 경우 백신 검사 예외 처리를 해주시기 바랍니다.

**오픈소스 및 직접 빌드 안내**
본 패치의 모든 소스 코드와 번역 데이터는 공개되어 있습니다.
인스톨러의 안전성이 우려되거나 직접 수정하고 싶으신 분들은 상단의 공식 깃허브 저장소를 방문하여
코드를 직접 확인하거나 개별적으로 다운로드/빌드하여 사용하실 수 있습니다.

---

> [!NOTE]
> **최종 동의 확인**
> 정상적인 이용을 위해 위 안내 사항을 반드시 숙지해 주시기 바랍니다.
> 본 프로그램을 계속 설치 및 이용하는 것은 위 내용에 동의함을 의미합니다.

---

# 🌐 English: Quick Save / Autosave add-on

If your Windows is not set to Korean, the installer does not install the Korean translation.
It copies only the quick save / autosave add-on (`version.dll`) to the game folder.

- 🌿 Press F5 to save to a slot named 'QUICKSAVE', and F6 to load it.
If you bound F5 or F6 to an in-game hotkey, that command runs together with save/load.
Avoid binding these keys to prevent unexpected issues.
- 🌿 After resting, the game is saved automatically as 'AUTOSAVE'.

Updated versions are always released at <https://github.com/wooddoll/PoR_RoMD>.

## 📢 Please read: terms of use

By installing or using this add-on, you agree to all of the terms and notes below.
Continue only if you have read and agree to them.

### ✔ Supported version
Made for the 2026 SNEG re-release (Steam, etc.). The 2001 original is not supported.
It may stop working if a game update changes the game's internals.

### ✔ For owners of the game only
This add-on may be used only by people who have legitimately purchased the game (retail package, digital edition, etc.).

### ✔ No commercial use or sale
This add-on is a non-profit fan work.
Selling, renting, or using all or part of it for commercial purposes is strictly prohibited.

### ✔ Copyright
This add-on contains only code written by the author.
It does not include any original game data or other third-party copyrighted works.

### ✔ Disclaimer
This is an unofficial fan-made add-on. Depending on your system, the game may fail to start or errors may occur.
The author takes no legal or material responsibility for any problems caused by installing or using it,
such as damaged game data, corrupted save files, or system errors.
(Backing up your game files and save data before installing is recommended.)

## 📁 Installation

- Copies `version.dll` and `minhook.x32.dll` to the game folder. No original game files are changed.
- **To remove**: delete `version.dll` and `minhook.x32.dll` from the game folder.

## 🛠️ Technical notes

**Antivirus false positives**
This add-on hooks game memory while the game is running.
Because of this, some antivirus programs may wrongly flag `version.dll` as malware and block it.
If the game does not start, add an exception for it.

**Open source**
All source code is public.
If you are concerned about the installer's safety or want to modify it, visit the GitHub repository above
to review the code, or download and build it yourself.

---

> [!NOTE]
> **Final confirmation**
> Please make sure you understand the notes above.
> Continuing the installation or use means you agree to them.
