# Pool of Radiance: Ruins of Myth Drannor
## 한글화 테스트

- windows xp  // ?
- directx8.0a // dgVoodoo2 

* font관련은 Pool of Radiance/data/fonts/font%d.bmp / .idx
* dialogue관련은 Pool of Radiance/data/Dialogue/* // test. 파일들 제외
* 기타 지명, 인명 등과 관련 있어 보이는 파일들. 
- Pool of Radiance/data/str/*.str
- Pool of Radiance/data/Rand/STR/*.str


## font
* Data/Fonts/
* font.md참고

## dialogue
* Data/Dialogue/
* dialogue.md 참고

## 자막 번역
1. [RAD Video Tools](https://www.radgametools.com/bnkdown.htm)로 Bink(Data/Bink/.bik)동영상을 .avi로 변환
1. [WhisperSubTranslate](https://github.com/Blue-B/WhisperSubTranslate)로 자막 파일을 생성
1. [SubtitleEdit](https://github.com/SubtitleEdit/subtitleedit)로 자막의 싱크를 조절하고 직접 들어보면서 고유 명사나 잘못 인식한 내용을 수정
1. AI가 번역하기 쉽도록 텍스트 파일(subtitles/description.txt)로 다시 정리한다.
1.1 말하는 사람의 이름을 추가
1.1 대사를 모아서 문장으로 재구성
1.1 장면 전환 시점 및 현재 장면에 대한 묘사 등을 추가
1. AI(gemini-3.5-flash-lite)를 사용해서 description.txt 번역
1. 번역한 내용을 다시 자막 파일로 옮기고 SubtitleEdit로 적절한 위치에 올라갔는지 확인

## 자막 출력 패치
1. proxy version.dll에서 binkw32에 대한 hook(PoR_RoMD_patch2b\bink_hook.cpp)을 만들어서 
1.1 DetourBinkOpen()을 가로채서 .bik을 로딩할 때 자막 파일도 로딩
1.1 DetourBinkCopyToBuffer()을 가로채서 프레임을 복사할 때 자막을 렌더링