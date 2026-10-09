# 위치: .\data\Dialogue\

	all.db
	all.idx
	*.por
	3종류의 파일이 존재

* all.idx의 내용
	[.por파일이름]['.all'][0x00000000][Address: 4byte]이 반복됨
	모든 .por파일의 이름이 순서대로 나옴. Address all.db파일에서 이 .por파일 내용이 시작하는 주소

* all.db의 내용: [Header][Direction][Selection][Key] 가 반복됨
	[Header]: 32b int값 4개 // 순서대로 [Direction]의 크기, [Selection]의 크기, [Key]의 크기, {[Header][Direction][Selection][Key]}의 전체 크기
	[Direction]: bytes array
	[Selection]: [in game내에서 볼수 있는 선택문][0x0D, 0x0A] 반복됨 // 이 부분이 없는 경우도 있음. .por파일의 내용에 dialogue가 'null'인 경우에 이렇게 되는 것 같다.
	[Key]: [.por파일에서 나오는 dialogue의 key값][0x0D, 0x0A] 반복됨 // 한 .por에 있는 key가 순서대로 모두 나옴, key의 길이는 8바이트 어레이, 7바이트일 경우 끝에 ':'가 붙어서 8바이트로 표시됨


* .por파일들의 내용, 다음 내용이 반복됨
	[dialogue의 key, 7~8바이트][tab][dialogue, 가끔 'null'이라는 텍스트도 있음][tab][말하는 사람의 key값으로 보이는 영문텍스트, 8자 전후][tab][날짜, 'null', 간단한 장면묘사등의 텍스트][tab][0x0D, 0x0A]
	// [dialogue의 key], 영문이 아니라 바이트 어레이, 중간에 출력할 수 없는 특수문자가 들어감
	// [날짜, 'null', 간단한 장면묘사등의 텍스트][tab] 이 없이 바로 [0x0D, 0x0A]로 끝나는 경우도 있음. 


* 모두 합쳐서 하나의 구조체로 관리할 것.
```
	[Section]
		[Direction]
		[Selections]
		[Keys]
			[Dialogue]
			[Speaker]
			[Desc]
```
***

	튜토리얼 시작시 나오는 텍스트는 tutd001a.por / ttadma02
	첫 장면에서 나오는 텍스트는 nrd000.por / kxadmaYA, 다음 텍스트는 mnrd000a.por / mqadma00
