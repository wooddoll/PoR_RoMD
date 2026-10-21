import json

# 1. 파일 경로 설정
json_file_path = "translation/glossary_latest.json"
tmx_file_path = "translation/glossary.tmx"

# 2. JSON 데이터 로드
with open(json_file_path, "r", encoding="utf-8") as f:
    json_data = json.load(f)

# 3. Matecat 표준 TMX 템플릿 빌드 시작
# 상위 tmx 태그에 필요한 xml 네임스페이스(xmlns:xml)를 명확히 선언합니다.
tmx_lines = [
    '<?xml version="1.0" encoding="utf-8"?>',
    '<tmx version="1.4" xmlns:xml="http://w3.org">',
    '  <header creationtool="CustomGlossaryConverter" creationtoolversion="1.0" segtype="phrase" adminlang="en-US" srclang="en-US" o-tmf="json"/>',
    '  <body>'
]

# 특수문자(&, <, >)가 XML을 깨뜨리지 않도록 이스케이프 함수 정의
def escape_xml(text):
    return text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")

# 4. JSON 순회 및 데이터 매핑
for eng_word, info in json_data.items():
    kor_word = info.get("korean", "").strip()
    category = info.get("category", "").strip()
    description = info.get("description", "").strip()
    
    if not kor_word:
        continue
    
    # 에러 유발 가능성이 높은 독립 <note> 태그 대신,
    # Matecat이 완벽히 인식할 수 있도록 번역문 뒤에 [카테고리: 설명] 정보를 깔끔하게 결합합니다.
    extra_info = ""
    if category or description:
        extra_info = f" ({category} - {description})" if category and description else f" ({category}{description})"
    
    # 이스케이프 처리
    safe_eng = escape_xml(eng_word.strip())
    safe_kor = escape_xml(f"{kor_word}{extra_info}".strip())
    
    # 세그먼트 생성
    tmx_lines.append('    <tu>')
    tmx_lines.append(f'      <tuv xml:lang="en-US"><seg>{safe_eng}</seg></tuv>')
    tmx_lines.append(f'      <tuv xml:lang="ko-KR"><seg>{safe_kor}</seg></tuv>')
    tmx_lines.append('    </tu>')

# 5. 하단 닫는 태그 추가
tmx_lines.append('  </body>')
tmx_lines.append('</tmx>')

# 6. 최종 파일 저장
with open(tmx_file_path, "w", encoding="utf-8") as f:
    f.write("\n".join(tmx_lines))

print(f"Matecat 완벽 호환 TMX 파일 생성 완료: {tmx_file_path}")
