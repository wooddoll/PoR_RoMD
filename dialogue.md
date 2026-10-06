.\data\Dialogue\

all.db
all.idx
*.por

3종류의 파일이 존재

all.idx의 내용
[.por파일이름].all[8byte]이 반복됨
모든 .por파일의 이름이 순서대로 나옴. 8byte는 대부분 0x0이고 일부 값들이 있음

all.db의 내용
좀 복잡함
[bytes ...]{[in game내에서 볼수 있는 선택문][0x0D, 0x0A] 여러개 나옴}{[.por파일에서 나오는 dialogue의 key값][0x0D, 0x0A] 한 .por에 있는 key가 순서대로 모두 나옴}
// {[in game내에서 볼수 있는 선택문][0x0D, 0x0A] 여러개 나옴} 이 부분이 없는 경우도 있음. .por파일의 내용에 dialogue가 'null'인 경우에 이렇게 되는 것 같다.

.por파일들의 내용, 다음 내용이 반복됨
[dialogue의 key값으로 보이는 영문텍스트, 8자 전후][tab][dialogue, 가끔 'null'이라는 텍스트도 있음][tab][말하는 사람의 key값으로 보이는 영문텍스트, 8자 전후][tab][날짜, 'null', 간단한 장면묘사등의 텍스트][tab][0x0D, 0x0A]
// [날짜, 'null', 간단한 장면묘사등의 텍스트][tab] 이 없이 바로 [0x0D, 0x0A]로 끝나는 경우도 있음. 