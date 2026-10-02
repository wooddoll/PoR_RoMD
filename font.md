폰트 파일 폴더에는 2쌍의 파일이 있다.
[fontX.bmp, fontX.dff] 5쌍

file path: \Pool of Radiance\data\fonts\

font0.bmp, font1.bmp, font2.bmp, font3.bmp, font4.bmp // font0.bmp(565x80x3B), 파일별로 크기가 다름
흑백 이미지 파일이며, 비트맵 폰트가 있음. 4줄로 구분, 아스키코드 순서
1: 0 ~ 63
2: 64 ~ 127
3: 128 ~ 191
4: 192 ~ 255

font0.dff, font1.dff, font2.dff, font3.dff, font4.dff // 동일하게 7,184 Byte
4바이트 단위 정수, 리틀 엔디언
256, 565, 80, 0,       // 폰트 개수, 이미지 크기 가로, 세로, ?(모든 파일에서 0, 용도 불명)
x0, y0, x1, y1, index  // index값은 1부터 시작해서 256까지 증가. 모든 파일에서 동일
                       // 128~255는 0x80FFFFFF ~ 0xFFFFFFFF이고 256은 0x00010000. 버그로 보임
이후 모든 파일에서 일정한 숫자가 반복됨. 용도 불명
[127, 127], [126, 126] ... [0, 0] // 128쌍, 일정하게 감소
[0xFFFFFFFF, 0xFF000000], [0xFEFFFFFF, 0xFE000000] ... [0x80FFFFFF, 0x80000000] // 128쌍

