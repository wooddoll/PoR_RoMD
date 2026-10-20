# Pool of Radiance: Ruins of Myth Drannor
## 한글화 테스트

* DxWnd에서 가장 잘 돌아가는 것 같다.
- windows xp
- directx8.0a

* font관련은 Pool of Radiance/data/fonts/font%d.bmp / .idx
* dialogue관련은 Pool of Radiance/data/Dialogue/* // test. 파일들 제외
* 기타 지명, 인명 등과 관련 있어 보이는 파일들. 
- Pool of Radiance/data/str/*.str
- Pool of Radiance/data/Rand/STR/*.str


## Font
* Data/Fonts/
* font.md참고

## Dialogue
* Data/Dialogue/
* dialogue.md 참고

## 자막 번역
1. [RAD Video Tools](https://www.radgametools.com/bnkdown.htm)로 Bink(Data/Bink/.bik)동영상을 .avi로 변환
1. [WhisperSubTranslate](https://github.com/Blue-B/WhisperSubTranslate)로 자막 파일을 생성
1. [SubtitleEdit](https://github.com/SubtitleEdit/subtitleedit)로 자막의 싱크를 조절하고 직접 들어보면서 고유 명사나 잘못 인식한 내용을 수정
1. AI가 번역하기 쉽도록 텍스트 파일(subtitles/description.txt)로 다시 정리한다.
    1. 말하는 사람의 이름을 추가
    1. 대사를 모아서 문장으로 재구성
    1. 장면 전환 시점 및 현재 장면에 대한 묘사 등을 추가
1. AI(gemini-3.5-flash-lite)를 사용해서 description.txt 번역
1. 번역한 내용을 다시 자막 파일로 옮기고 SubtitleEdit로 적절한 위치에 올라갔는지 확인

## 자막 출력 패치
1. proxy version.dll에서 binkw32.dll에 대한 hook(PoR_RoMD_patch2b\bink_hook.cpp)을 만들어서 
    1. DetourBinkOpen()을 가로채서 .bik을 로딩할 때 자막 파일도 로딩
    1. DetourBinkCopyToBuffer()을 가로채서 프레임을 복사할 때 자막을 렌더링
    1. bink_hook.cpp; binkw32 후킹
    1. sdv_parser.cpp; .sbv자막 로드
    1. subtitle_renderer.h; 자막 출력

## voice 
1. data/Sound/voice/*.wav 파일이 있다.
    1. 파일명은 read_all_idx.py로 생성한 .json의 key와 일치함. 
    1. 소문자로 변환해야함
    1. 일부 안맞는 파일이 있긴 한데 대사를 잘 들어보면 .json어딘가에 있기는 하다.
    1. speaker별로 그룹을 만들것.
    1. gather_sound_info.py로 .wav-speaker 정보 생성
1. 있는 목소리로 학습해서 유사한 톤으로 음성을 생성하는 방법에 대해 gemini가 'XTTS v2'를 추천해서 설치하고 테스트
    1. 옛날 버전이라 버전 맞추느라 설치하는데 많은 애로사항이 있음
* Anorweyn이 젤 처음에 나오게 되는데, 원래 귀신톤임(유령이니까). 한국말로 바꾸면... 톤을 잘 이해못하는지 이상한 목소리..
* 일단 포기하고 나중에 다른 툴을 사용해 보던지 할 것.
