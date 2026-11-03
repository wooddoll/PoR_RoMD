# Pool of Radiance: Ruins of Myth Drannor 한글 패치 코드베이스 분석 보고서

본 보고서는 프로젝트 내 C++ 후킹 모듈(`PoR_RoMD_patch2b`), 데이터 처리 및 변환 Python 스크립트, 그리고 인스톨러(`install/`)의 소스 코드를 심층 분석하여 발견된 잠재적 버그, 안전성 취약점, 데이터 불일치 및 유지보수 개선점을 정리한 문서입니다.

---

## 1. 개요 및 요약

| 분류 | 심각도 | 항목 수 | 주요 내용 |
| :--- | :---: | :---: | :--- |
| **C++ 훅 패치 (`PoR_RoMD_patch2b`)** | Major ~ Minor | 4건 | 서피스 Lock 실패 처리 누락, CP949 전각/특수문자 판정 누락, MinHook 활성화 플래그 오류, WordWrap 줄바꿈 인덱스 언더플로우 |
| **Python 데이터/번역 스크립트** | Critical ~ Major | 4건 | `translation_str_ai.py` 미정의 식별자 및 스코프 크래시, `all.idx` 파서와 빌더 간의 null 바이트 종료 처리 불일치, `rebuild_all_db.py` 함수 재정의 누락 |
| **인스톨러 및 폰트 빌더 (`install/`, `build_fonts.py`)** | Major ~ Minor | 3건 | 인스톨러 파일 수 하드코딩 및 경로 불일치, 이미지 처리 모듈 임포트 비표준화, 누락된 f-string |

---

## 2. 모듈별 상세 분석

### 2.1 C++ 훅 모듈 (`PoR_RoMD_patch2b`)

#### [Major] 1. DirectDraw 서피스 Lock 실패 처리 누락 및 자원 누수
- **위치:** `PoR_RoMD_patch2b/DrawMenu.cpp:39`
- **현상:**
  ```cpp
  pSrc->Lock(NULL, &ddsdSrc, 0, NULL);
  BYTE* srcBase = (BYTE*)ddsdSrc.lpSurface;
  ```
  `DrawMenu.cpp`에서 `pDst->Lock()`은 반환값을 검사하지만, 바로 뒤의 `pSrc->Lock()`은 리턴값을 검사하지 않습니다. 서피스 락에 실패할 경우 `ddsdSrc.lpSurface`는 `nullptr` 또는 가비지 값이 되어 `srcBase + ...` 접근 시 즉시 크래시(Access Violation)가 발생합니다. 또한 `pSrc->Lock()` 실패 시 앞서 잠근 `pDst`를 언락하지 않고 반환해야 하는 예외 처리가 없습니다.
- **영향:** 비디오 메모리 부족 또는 DirectDraw 서피스 손실(Lost Surface) 상황에서 게임 렌더링 스레드가 비정상 종료됨.
- **해결 방안:**
  ```cpp
  if (pSrc->Lock(NULL, &ddsdSrc, 0, NULL) != 0) {
      pDst->Unlock(NULL);
      return FALSE;
  }
  ```

#### [Major] 2. CP949 전각/기호 특수문자 인식 누락 (`IsHangul`)
- **위치:** `PoR_RoMD_patch2b/por2.h:64-73`
- **현상:**
  ```cpp
  inline bool IsHangul(const uint8_t c1, const uint8_t c2) {     // EUC-KR
      return (c1 >= 0xA4 && c1 <= 0xC8 && c2 >= 0xA1 && c2 <= 0xFE);
  }
  ```
  `cp949_kor2350ext.txt` 및 게임 리소스에는 문장 부호와 기호(‘ ’, “ ”, ·, …, ―, ★ 등)를 포함하여 첫 번째 바이트가 `0xA1 ~ 0xA3`인 2바이트 완성형 문자가 다수 사용됩니다. 그러나 `IsHangul`은 `0xA4`부터만 검사하므로 `0xA1~0xA3` 범위의 2바이트 문자가 1바이트 문자로 처리되어 `getCharCode`가 상위 바이트만 반환하고, 후속 바이트가 분리되어 글자가 깨지거나 렌더링 위치/단어 줄바꿈(WordWrap) 계산이 어긋납니다.
- **영향:** 대화창이나 아이템 설명에 따옴표(‘ ’), 말줄임표(…), 가운데점(·) 등 특수문자가 들어갈 경우 문자 깨짐 및 비정상 줄바꿈 발생.
- **해결 방안:**
  EUC-KR/CP949 완성형 2바이트 전체 영역(특수문자 `0xA1~0xAC` 및 한글 음절 `0xB0~0xC8`, 확장 한글 대역 등)을 포괄하도록 범위 확장:
  ```cpp
  inline bool IsHangul(const uint8_t c1, const uint8_t c2) {
      return (c1 >= 0xA1 && c1 <= 0xFE && c2 >= 0xA1 && c2 <= 0xFE);
  }
  ```

#### [Minor] 3. `por2_hook.cpp` 내 MinHook 상태 변수 체크 오류
- **위치:** `PoR_RoMD_patch2b/por2_hook.cpp:103, 110, 117`
- **현상:**
  ```cpp
  MH_CreateHook((LPVOID)ptrRenderShadow, &_cTextLayout_RenderShadow_Hook, (LPVOID*)&g_origRenderShadow);
  if (st == MH_OK) { // st에 대입하지 않고 이전 ptrWordWrap의 st를 검사함
      MH_EnableHook((LPVOID)ptrRenderShadow);
  }
  ```
  `USING_MINHOOK` 활성화 시 `MH_CreateHook` 호출 결과를 `st`에 갱신하지 않고 검사하여, 두 번째 이후 훅의 성공 여부를 올바르게 판단하지 못함.
- **영향:** 디버그 목적으로 `USING_MINHOOK 1`로 빌드할 경우 실패한 훅도 무조건 `MH_EnableHook`을 시도하거나 로깅이 잘못됨.

#### [Minor] 4. `WordWrap.cpp` 문장 선두 줄바꿈 시 음수 인덱스 접근 잠재성
- **위치:** `PoR_RoMD_patch2b/WordWrap.cpp:52`
- **현상:**
  ```cpp
  PushLineValue(&self->m_pLineEndIndices, nLineCount, charIdx - 1); // WARN!!: 문장의 시작위치에 '\n'이 있으면 -1 위치를 읽게 됨
  ```
  텍스트가 `\n`으로 시작할 경우 `charIdx`가 0이므로 `m_pLineEndIndices`에 `-1`이 저장됩니다. 이후 `RenderShadow.cpp:52`에서 `charIdx <= self->m_pLineEndIndices[lineIdx]` 비교 시 예기치 않은 루프 종료가 발생할 수 있습니다.
- **해결 방안:** `std::max(0, charIdx - 1)` 등으로 하한 보정 처리.

---

### 2.2 Python 데이터 및 번역 도구

#### [Critical] 1. `translation_str_ai.py` 런타임 NameError 및 UnboundLocalError
- **위치:** `translation_str_ai.py:57, 60, 112, 118`
- **현상:**
  1. 10~11행에서 `client`, `types` import 및 인스턴스화가 주석 처리되어 있으나, 57행과 60행에서 전역 변수로 직접 호출하여 `NameError: name 'client' is not defined` 발생.
  2. 112행 `processed_count += 1`: `sub_proc` 내부 함수에서 바깥 스코프의 `processed_count`를 수정하면서 `nonlocal processed_count`가 선언되지 않아 `UnboundLocalError: cannot access local variable 'processed_count'` 발생.
  3. 118행: 실패 안내 메시지 출력 시 정의되지 않은 루프 변수 `batch`를 참조하여(`len(batch)`) `NameError` 발생 (`len(words_to_send)`여야 함).
- **영향:** 해당 스크립트 실행 시 첫 번째 요청 및 배치 처리 과정에서 즉시 크래시되어 번역 작업 진행 불가.
- **해결 방안:** 주석 해제 및 `genai.Client` 초기화 복구, `sub_proc` 내부에 `nonlocal processed_count` 선언, `len(words_to_send)`로 변수명 교정.

#### [Major] 2. `rebuild_all_db.py`의 `write_all_idx`와 `read_all_idx.py` 간의 널(Null) 구분자 처리 불일치
- **위치:** `rebuild_all_db.py:141-149` vs `read_all_idx.py:70-77`
- **현상:**
  - `dialogue.md` 명세에 따르면 `all.idx`의 각 엔트리는 **고정 20바이트가 아니라 가변 크기(최대 12바이트의 섹션 이름 문자열 + 8바이트 예약/주소 데이터 = 최대 20바이트)** 구조입니다.
  - `read_all_idx.py`의 읽기 로직에서는 `0x00` 바이트를 만날 때까지(또는 최대 12바이트까지) 문자열을 읽고, 그 직후 위치부터 8바이트(`extra_bytes = data[pos:pos+8]`, 즉 4바이트 0x00 + 4바이트 주소)를 읽도록 파싱합니다.
  - 그러나 `rebuild_all_db.py`의 `write_all_idx` 구현에서는:
    ```python
    name_bytes = (sec_name + '.all').encode('ascii')
    f.write(name_bytes)
    # 널 문자(\x00) 종단 없이 바로 8바이트(4바이트 0x00000000 + 4바이트 주소) 작성
    f.write(b'\x00\x00\x00\x00')
    f.write(struct.pack('<I', sec_data['new_address']))
    ```
    섹션 이름 문자열이 12바이트 미만(예: `dw1c005.all`은 11바이트)인 경우, 문자열 끝에 널 종료 문자(`\x00`)를 넣지 않은 채 바로 뒤의 예약 영역 `0x00000000`의 첫 번째 `\x00`을 만나 루프를 탈출하게 됩니다. 이때 `pos`는 예약 영역의 첫 바이트 위치에 머물게 되어, 뒤이어 읽는 8바이트가 1바이트씩 앞으로 밀려(`hi_bytes`가 0이 아니게 되거나 주소가 왜곡됨) `Warning: first 4 bytes should be zero` 경고가 발생하고 파싱이 중단될 수 있습니다.
- **영향:** `write_all_idx`로 저장한 파일을 `read_all_idx.py`나 게임 엔진의 널 종단 문자열 기반 파서가 읽을 때 예약 영역 바이트가 널 종단 문자로 오인되어 오프셋 어긋남이 발생할 수 있음.
- **해결 방안:**
  섹션 이름 문자열 뒤에 명시적으로 널 바이트(`b'\x00'`)를 추가하거나 규격에 맞게 기록하도록 보완:
  ```python
  name_bytes = (sec_name + '.all').encode('ascii') + b'\x00'
  f.write(name_bytes)
  f.write(b'\x00\x00\x00\x00')
  f.write(struct.pack('<I', sec_data['new_address']))
  ```

#### [Major] 3. `read_por_files.py`와 `rebuild_all_db.py`의 함수 시그니처 충돌
- **위치:** `rebuild_all_db.py:198, 210` vs `read_por_files.py:57`
- **현상:**
  `read_por_files.py`의 `write_por_files(en_dialogue_dir, json_path, dialogue_dir)`는 인자를 3개 받는 반면, `rebuild_all_db.py`의 `write_por_files(sections, output_dir)`는 인자를 2개 받습니다. `merge_dialogue.py`에서는 두 모듈을 혼합 임포트할 때 어떤 함수가 덮어씌워지느냐에 따라 `TypeError`가 유발될 위험이 있습니다.

---

### 2.3 인스톨러 및 폰트 생성 도구

#### [Major] 1. `install/install.py` 대화/STR 파일 개수 하드코딩 검증
- **위치:** `install/install.py:147, 174, 193`
- **현상:**
  ```python
  return copied_count == 1398  # Dialogue .por
  return copied_count == 44    # STR .str
  return copied_count == 10    # Bink .sbv
  ```
  번역 패치 파일 수가 변경되거나 확장팩/패치 버전에 따라 파일 수가 다를 경우 검증에 실패하여 인스톨러가 false를 리턴하거나 중단될 위험이 있습니다.
- **해결 방안:** 고정 숫자 비교 대신 소스 디렉토리 내 실제 유효 파일 수(`len(valid_files) > 0`)와 비교하도록 유연화.

#### [Major] 2. `build_fonts.py` vs `install/build_fonts.py` 출력 경로 불일치
- **위치:** `build_fonts.py:349` vs `install/build_fonts.py:349`
  - 루트의 `build_fonts.py`: `output_dir = os.path.join(game_dir, "Data_kr2/Fonts")`
  - 인스톨러 측은 인자로 경로를 전달받아 처리함.
  - 루트 스크립트 실행 시 `Data_kr`이 아닌 `Data_kr2`로 엉뚱한 폴더에 파일이 생성되는 잔여 테스트 코드가 남아있음.

#### [Minor] 3. PIL / Image 임포트 방식 비표준화
- **위치:** `build_fonts.py:26` (`import Image`)
  - 최신 Pillow 환경에서는 `from PIL import Image`로 임포트해야 표준 동작을 보증합니다.

#### [Minor] 4. F-string 포맷 플레이스홀더 누락 경고 다수
- **위치:** `fix_encode.py:319`, `read_all_idx.py:84, 99`, `install/install.py:94, 129, 158` 등
  - `f"..."` 문자열 내부에 변수 치환자(`{}`)가 없어 단순 문자열로 사용되고 있음. 가독성 및 린트 경고 개선 대상.

---

## 3. 종합 개선 제안 로드맵

1. **안정성 최우선 패치 (1단계)**
   - `rebuild_all_db.py`: `all.idx` 섹션명 기록 시 널 구분자 명시 및 읽기 로직과의 정합성 유지.
   - `translation_str_ai.py`: API 클라이언트 초기화 및 스코프 변수 수정.
   - `PoR_RoMD_patch2b/DrawMenu.cpp`: 서피스 Lock 반환값 검증 및 Unlock 누수 보완.
   - `PoR_RoMD_patch2b/por2.h`: 완성형 특수기호 대역(`0xA1~0xA3`) 포함하도록 `IsHangul` 보정.

2. **빌드 및 인스톨러 환경 정돈 (2단계)**
   - `build_fonts.py`의 출력 폴더명(`Data_kr`) 통일 및 `from PIL import Image` 표준화.
   - `install.py`의 파일 수 하드코딩 조건식 개선.

3. **기능 확장 및 README TODO 대응 (3단계)**
   - `readme.md`에 명시된 `korean.ini` -> `language.ini` 리팩토링 및 상대 경로 보존 처리.
   - 미사용 import문 및 f-string 경고 정리.
