# Pool of Radiance: Ruins of Myth Drannor
## 한글화 테스트

- windows xp
- directx8.0a
    - fast memory copy인가? 키면 안됨

## TODO
1. .ini
    1. korean.ini -> language.ini
    1. .ini에 폴더가 없으면 패치하지 말 것
    1. .ini의 상대 경로를 절대 경로로 수정하지 말 것
1. installer
    1. 바이너리 검사할 때 경로 패치할 부분도 모두 추가하기
    1. 
1. 사용법 체크
    1. font
        ```
        #kor font sheet 생성
        python draw_font_sheet.py
        #kor font .bmp/.dff 생성
        python build_fonts.py "D:/workspace/Pool of Radiance"
        ```
    1. str
        ```
        #str to json
        python str_json_converter.py to-json --str-dir "D:/workspace/Pool of Radiance/Data/STR" --json-file "translation/str_data.json"
        #json to glossary
        python str_json_converter.py to-glossary --json-file "translation/str_data.json" --glossary-file "translation/str_glossary_test.json"
        #glossary to json, !! json을 덮어씀
        python str_json_converter.py from-glossary --glossary-file "translation/str_glossary_test.json" --json-file "translation/str_data.json" 
        #json to str
        python str_json_converter.py from-json --json-file "translation/str_data.json" --str-dir "D:/workspace/Pool of Radiance/Data_kr/STR"
        #rebuild json to compare
        python str_json_converter.py rebuild-glossary --str-dir "D:/workspace/Pool of Radiance/Data/STR" --patch-dir "D:/workspace/Pool of Radiance/Data_kr/STR" --glossary-file "translation/str_glossary_kor.json"
        ```
    1. dialoue
        ```
        #.por to json
        python read_por_files.py por-json "../Pool of Radiance/Data/Dialogue" "dump/str_data_en.json"
        #json to .por
        python read_por_files.py json-por "../Pool of Radiance/Data/Dialogue" "dump/str_data_kr.json" "../Pool of Radiance/Data_kr/Dialogue"
        #rebuild json to compare
        python read_por_files.py combined "../Pool of Radiance/Data/Dialogue" "../Pool of Radiance/Data_kr/Dialogue" "dump/merged_en_kr_re.json"
        ```

## Font
### 파일
* font.md참고
### 2바이트 문자 추가
### 입출력
1. 메모리상의 함수에 대한 후킹 방식으로 동작
    1. [Ghidra](https://github.com/nationalsecurityagency/ghidra)로 정적 분석
    1. Data/Fonts 검색으로 폰트 파일 로딩하는 함수로부터 추적
    1. 실제 디컴파일은 클로드 코드가 잘 함; gpt, gemini는 결과가 애매했음.
    1. proxy version.dll으로 후킹함
        1. 출력 관련; CalculateTextExtents.cpp, DrawTextLines.cpp, RenderShadow.cpp, WordWrap.cpp 
        1. 입력 관련; KeyInput_Draw.cpp
1. 출력은 현재 문제 없음.
1. 입력 테스트 중.

## Dialogue
### 파일
* dialogue.md 참고
* 별다른 태그나 기호가 없어서 내용만 번역하면 됨.
### 번역
* 구글 api를 통해 번역하고 검수함
### 출력
* 일괄 적용후 테스트 중; 별 이상 없는 것으로 보임

## Items / Location / NPC / Monster
## 파일
* ./Data/str/*.str
    - items 수식어? 이름?: ./data/str/TEE.str
    - NPC, Monster: ./data/Rand/STR/monsters.str
    - 기타 .str 파일들은 아마도 특정 맵에서 사물의 이름인듯하다.
## 번역
* 생각보다 양이 너무 많음
## 출력
* Item이름 직접 수정 후 테스트; 작동함

## 자막
### 번역
1. [RAD Video Tools](https://www.radgametools.com/bnkdown.htm)로 Bink(Data/Bink/.bik)동영상을 .avi로 변환
1. [WhisperSubTranslate](https://github.com/Blue-B/WhisperSubTranslate)로 자막 파일을 생성
1. [SubtitleEdit](https://github.com/SubtitleEdit/subtitleedit)로 자막의 싱크를 조절하고 직접 들어보면서 고유 명사나 잘못 인식한 내용을 수정
1. AI가 번역하기 쉽도록 텍스트 파일(subtitles/description.txt)로 다시 정리한다.
    1. 말하는 사람의 이름을 추가
    1. 대사를 모아서 문장으로 재구성
    1. 장면 전환 시점 및 현재 장면에 대한 묘사 등을 추가
1. AI(gemini-3.5-flash-lite)를 사용해서 description.txt 번역
1. 번역한 내용을 다시 자막 파일로 옮기고 SubtitleEdit로 적절한 위치에 올라갔는지 확인
### 출력
1. proxy version.dll에서 binkw32.dll에 대한 hook(PoR_RoMD_patch2b\bink_hook.cpp)을 만들어서 
    1. DetourBinkOpen()을 가로채서 .bik을 로딩할 때 자막 파일도 로딩
    1. DetourBinkCopyToBuffer()을 가로채서 프레임을 복사할 때 자막을 렌더링
    1. bink_hook.cpp; binkw32 후킹
    1. sdv_parser.cpp; .sbv자막 로드
    1. subtitle_renderer.h; 자막 출력

## voice 
1. data/Sound/voice/*.wav 파일
    1. 파일명은 read_all_idx.py로 생성한 .json의 key와 일치함. 
    1. 소문자로 변환해야함
    1. 일부 안맞는 파일이 있긴 한데 대사를 잘 들어보면 다른 이름으로 .json어딘가에 있기는 하다.
    1. speaker별로 그룹을 만들것.
    1. gather_sound_info.py로 .wav-speaker 정보 생성
1. 있는 목소리로 학습해서 유사한 톤으로 음성을 생성하는 방법에 대해 gemini가 'XTTS v2'를 추천해서 설치하고 테스트
    1. 옛날 버전이라 버전 맞추느라 설치하는데 많은 애로사항이 있음
* Anorweyn이 젤 처음에 나오게 되는데, 원래 귀신톤임(유령이니까). 한국말로 바꾸면... 톤을 잘 이해못하는지 엉망이다.
* *포기*; 나중에 다른 툴을 사용해 보던지 할 것.


## License & Copyright (라이선스 및 저작권)

### Source Code
* 본 저장소의 소스 코드는 [MIT License](LICENSE) 하에 오픈소스로 제공됩니다. 자유로운 연구, 수정 및 빌드가 가능합니다.

### Translation & Resources (번역 및 패치 파일)
* **상업적 이용 금지:** 본 한글패치 데이터 및 번역 텍스트의 전체 또는 일부를 유료로 판매, 대여하거나 상업적인 목적으로 이용하는 행위는 엄격히 금지됩니다.
* **정품 소유자 전용:** 본 패치는 오직 정상적인 경로로 게임을 구매한 정품 소유자만을 위한 비영리 창작물입니다.
* **저작권 고지:** 본 패치 파일은 자체 제작한 기술 코드와 순수 번역 데이터로만 구성되어 있으며, 원본 게임의 데이터 등 타인의 저작물은 일절 포함하고 있지 않습니다.

### Used Fonts (사용 폰트 라이선스)
* [움돋 (Umdot)](https://monadabxy.com) - SIL Open Font License 1.1
* [갈무리 (Galmuri)](https://quiple.dev) - SIL Open Font License 1.1
