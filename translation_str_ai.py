import sys
import os
import struct
import base64
import json
import time
import re
#import typing_extensions as typing

from typing import Any
#from google import genai
#from google.genai import types
from pydantic import BaseModel, Field


def load_API_KEY(file_path):
    with open(file_path, 'r', encoding='utf-8') as file:
        for line in file:
            # strip()으로 앞뒤 공백을 제거한 후 ';'로 시작하는지 확인합니다.
            clean_line = line.strip()
            
            # 빈 줄이 아니고 ';'로 시작하지 않는 줄을 찾으면 리턴
            if clean_line and not clean_line.startswith(';'):
                return line.strip() # 혹은 줄바꿈을 포함하려면 line 그대로 리턴
    return None # 조건에 맞는 줄이 없을 경우


GEMINI_API_KEY = load_API_KEY("google_ai_key.txt")
GEMINI_MODEL = 'gemini-3.5-flash-lite'
#client = genai.Client(api_key=GEMINI_API_KEY)

# 3. 응답 받을 구조 정의
class WordOptions(BaseModel):
    # 한 단어당 3개의 후보를 담을 문자열 리스트 (Pydantic이 길이 제약을 검증하도록 설정 가능)
    options: list[str] = Field(description="단어 하나에 대한 서로 다른 3개의 한국어 번역 후보 리스트")

class MultiTranslationResponse(BaseModel):
    # 입력된 영어 단어 리스트와 순서가 일치하는 결과 리스트
    translated_data: list[WordOptions] = Field(description="입력된 영어 단어 순서와 일치하는 번역 후보들의 리스트")

# ==========================================
# Core Translation Call
# ==========================================
def _request_gemini_translation(
    to_translate_list: list[str], word_type: str
) -> list[list[str]]:
    prompt_data = f"""
1. 이 게임은 'Dungeons&Dragons'룰북을 사용하는 'Forgotten Realms'세계관의 'Pool of Radiance: Ruins of Myth Drannor'이다.
2. 제공된 {word_type}들을 한국어로 자연스러운 게임 톤 앤 매너에 맞게 번역해라.
3. 각 단어에 대해 게임 맥락에 어울리는 자연스러운 한국어 번역 후보를 '정확히 3개씩' 제안해라.
4. 후보들은 서로 다른 느낌(예: 의역, 직역, 판타지풍 등)을 담고 있어야 합니다.
번역할 목록": 
{to_translate_list}
    """

    try:
        response = client.models.generate_content(
            model=GEMINI_MODEL,
            contents=json.dumps(prompt_data, ensure_ascii=False),
            config=types.GenerateContentConfig(
                response_mime_type='application/json',
                response_schema=MultiTranslationResponse,
                temperature=0.4, # 다양한 후보를 얻기 위해 온도를 조금 올림
                system_instruction="당신은 게임 현지화 전문가다. 입력된 단어 순서를 절대 섞지 말고, 각 단어마다 가장 어울리는 서로 다른 번역 후보 3개를 배열로 제공하라."
            ),
        )
        # 5. 결과 변환 및 구조화
        result = MultiTranslationResponse.model_validate_json(response.text)
        # Python 2차원 리스트(또는 튜플 구조)로 최종 변환
        # 결과 형태: [['대지바람', '대지 태생의', '어스본'], ['가죽 갑옷', '레더 아머', '가죽 방어구'], ...]
        final_candidates = [item.options for item in result.translated_data]
        return final_candidates

    except Exception as e:
        print(f"[API 호출 오류 발생]: {e}")
        return []


# ==========================================
# Main Batch Processing
# ==========================================
def translation_dialogue(glossary: dict, batch_size: int = 10):
    pending_items = []
    pending_NPCs = []
    pending_LOCs = []
    for words, value in glossary.items():
        if not value.get('korean', ''):
            if value.get('category') == 'Item':
                pending_items.append(words)
            elif value.get('category') == 'Location':
                pending_LOCs.append(words)
            else:
                pending_NPCs.append(words)

    total_pending = len(pending_items)+len(pending_LOCs)+len(pending_NPCs)
    print(f"총 번역 대상 수: {total_pending}개 (배치 크기: {batch_size})")
    processed_count = 0

    result = {}
    def sub_proc(text_list, word_type):
        # 2. batch_size 단위로 끊어서 번역 진행
        for i in range(0, len(text_list), batch_size):
            words_to_send = text_list[i : i + batch_size]

            time.sleep(2)  # Rate Limit 완화
            kor_words = _request_gemini_translation(words_to_send, word_type)

            # 번역 결과 매핑 및 갱신
            if kor_words:
                for item, trans_result in zip(words_to_send, kor_words):
                    result[item] = trans_result
                    processed_count += 1

                print(f"[{processed_count}/{total_pending}] 번역 완료 (진행률: {processed_count/total_pending*100:.1f}%)")
                # 매 배치 완료 후 중간 저장
                save_json("translation/glossary_str_temp.json", result)
            else:
                print(f"배치 번역 실패 ({i} ~ {i + len(batch)} 항목). 다음 배치를 진행합니다.")
    
    sub_proc(pending_items, "아이템명")
    sub_proc(pending_LOCs, "장소명 또는 고정된 사물명")
    #sub_proc(pending_NPCs, "영문 NPC 이름")
    print("모든 번역 작업이 완료되었습니다.")

    return result

def load_json(json_path):
    with open(json_path, 'rb') as f:
        raw_data = f.read()
        data = json.loads(raw_data.decode('utf-8'))
        return data
    return {}

def save_json(json_path, json_data):
    with open(json_path, 'w', encoding='utf-8') as f:
        json.dump(json_data, f, ensure_ascii=False, indent=2) 

if __name__ == '__main__':
    glossary_json_path = "translation/str_glossary_latest.json"
    glossary_data = load_json(glossary_json_path)    
    result = translation_dialogue(glossary_data, 10)

    save_json("translation/glossary_str_temp.json", result)
    
    
