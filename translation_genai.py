import sys
import os
import struct
import base64
import json
import time
import typing_extensions as typing
import re
from google import genai
from google.genai import types
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
client = genai.Client(api_key=GEMINI_API_KEY)
import json
import re
import time
from typing import Any
from pydantic import BaseModel, Field

# ==========================================
# Pydantic Schemas
# ==========================================
class DialogueDetail(BaseModel):
    person_key: str = Field(description="캐릭터를 구별하는 key값")
    dialogue: str = Field(description="캐릭터가 말하는 대사 또는 행동")
    description: str = Field(description="캐릭터가 말하는 분위기나 상황")

class PersonDetail(BaseModel):
    person_key: str = Field(description="캐릭터를 구별하는 key값")
    name: str = Field(description="캐릭터의 이름")
    toneandmanner: str = Field(description="캐릭터의 말투")
    description: str = Field(description="배경, 나이, 성별, 직업, 과거, 성격 등 캐릭터의 특징")

class GlossaryDetail(BaseModel):
    term: str = Field(description="영어 단어")
    korean: str = Field(description="한국어 번역")
    category: str = Field(description="단어의 종류. Character / Location / Monster / Magic / Organization / Item 등")
    description: str = Field(description="단어의 설명")

class TranslatedDialogue(BaseModel):
    translated_dialogues: list[DialogueDetail]
    updated_persons: list[PersonDetail]
    updated_glossary: list[GlossaryDetail]


# ==========================================
# Core Translation Call
# ==========================================
def _request_gemini_translation(
    dialogues: list[dict],
    glossary: list[dict],
    persons: list[dict]
) -> tuple[list[dict], dict, dict]:
    """Gemini API를 호출하여 번역 및 용어/인물 업데이트 정보를 가져옵니다."""
    
    prompt_data = {
        "background": (
            "너는 전문 게임 번역가로서 게임의 대사를 번역하고 있다.\n"
            "이 게임은 'Dungeons & Dragons'룰북을 사용하는 'Forgotten Realms' 세계관의 "
            "'Pool of Radiance: Ruins of Myth Drannor'이다."
        ),
        "instructions": (
            "1. 제공된 대사(dialogues)를 [한국어]로 자연스러운 게임 톤 앤 매너에 맞게 번역하라.\n"
            "2. 번역 시 용어집(glossary)에 정의된 단어는 반드시 지정된 한국어(korean)로 번역하라.\n"
            #"3. 새로 추가할 용어가 있다면 'updated_glossary'에 항목을 추가하라.\n"
            "3. 기존 용어 중 대사를 통해 더 명확해진 내용('category', 'description')이 있다면 'updated_glossary'기존 내용을 업데이트하여 반환하라. 단 절대로 'korean'을 수정하면 안된다.\n"
            "4. 인물 정보('persons')를 참고하여 각 인물의 성격과 말투('toneandmanner')를 반영하라.\n"
            #"6. 정보가 부족하거나 없는 인물은 대사 맥락을 분석하여 이름, 말투, 인물 설명을 추정하여 'updated_persons'에 새로 추가하라.\n"
            "5. 기존 인물 정보 중 대사를 통해 더 명확해진 내용이 있다면 'updated_persons'에 기존 내용을 업데이트하여 반환하라."
        ),
        "input_data": {
            "dialogues": dialogues,
            "glossary": glossary,
            "persons": persons
        }
    }

    try:
        response = client.models.generate_content(
            model=GEMINI_MODEL,
            contents=json.dumps(prompt_data, ensure_ascii=False),
            config=types.GenerateContentConfig(
                response_mime_type='application/json',
                response_schema=TranslatedDialogue,
            ),
        )
        result = json.loads(response.text)

        translated_dialogues = result.get('translated_dialogues', [])
        
        # 딕셔너리 형태로 정규화
        updated_persons = {
            p['person_key']: {
                'name': p['name'],
                'toneandmanner': p['toneandmanner'],
                'description': p['description']
            }
            for p in result.get('updated_persons', [])
        }

        updated_glossary = {
            g['term']: {
                'korean': g['korean'],
                'category': g['category'],
                'description': g['description']
            }
            for g in result.get('updated_glossary', [])
        }

        return translated_dialogues, updated_glossary, updated_persons

    except Exception as e:
        print(f"[API 호출 오류 발생]: {e}")
        return [], {}, {}


# ==========================================
# Helper Functions
# ==========================================
def _extract_filtered_context(
    batch_dialogues: list[dict],
    full_glossary: dict,
    full_persons: dict
) -> tuple[list[dict], list[dict]]:
    """배치에 포함된 대사들을 기반으로 필요한 인물과 용어만 추출합니다."""
    
    # 1. 인물 필터링
    speaker_keys = {talk['person_key'] for talk in batch_dialogues if talk.get('person_key')}
    filtered_persons = [
        {'person_key': k, **full_persons[k]}
        for k in speaker_keys if k in full_persons
    ]

    # 2. 용어집 필터링 (단어 경계 \b를 고려한 정규식 매칭)
    all_text = " ".join(talk['dialogue'] for talk in batch_dialogues)
    filtered_glossary = []
    
    for term, data in full_glossary.items():
        base_term = re.escape(term)
        
        # 기본 패턴: 단어 + (s, es, 's 허용)
        pattern = rf"\b{base_term}(?:'s|’s|es|s)?\b"

        if re.search(pattern, all_text, re.IGNORECASE):
            filtered_glossary.append({
                'term': term,
                'korean': data['korean'],
                'category': data['category'],
                'description': data['description']
            })

    return filtered_glossary, filtered_persons


def _save_progress(persons: dict, glossary: dict, translations: dict):
    """중간 진행 상황 저장"""
    save_json("translation/persons11.json", persons)
    save_json("translation/glossary11.json", glossary)
    save_text_to_json("translation/translated11.json", translations)


# ==========================================
# Main Batch Processing
# ==========================================
def translation_dialogue(to_translation: dict, glossary: dict, person_info: dict, batch_size: int = 10):
    """
    섹션(sec_name)을 가로질러 대사를 batch_size 단위로 모아 번역을 수행합니다.
    """
    # 1. 번역이 필요한 항목들을 (sec_name, key, dialogue_payload) 구조로 수집 (Flatten)
    pending_items = []
    for sec_name, sec_data in to_translation.items():
        if not sec_data.get('_check', False):
            selections = sec_data.get('selections', [])
            for idx, text in enumerate(selections):
                pending_items.append({
                    'sec_name': sec_name,
                    'key': f'_sel_{idx}',
                    'payload': {
                        'person_key': 'Protagonist',
                        'dialogue': text,
                        'description': '플레이어가 선택할 수 있는 행동'
                    }
                })
        
        keys = sec_data.get("keys", {})
        for key, key_data in keys.items():
            if not key_data.get('_check', False):
                pending_items.append({
                    'sec_name': sec_name,
                    'key': key,
                    'payload': {
                        'person_key': key_data.get('speaker', ''),
                        'dialogue': key_data.get('dialogue', ''),
                        'description': key_data.get('desc', '')
                    }
                })

    total_pending = len(pending_items)
    print(f"총 번역 대상 대사 수: {total_pending}개 (배치 크기: {batch_size})")

    # 2. batch_size 단위로 끊어서 번역 진행
    processed_count = 0
    for i in range(0, total_pending, batch_size):
        batch = pending_items[i : i + batch_size]
        dialogues_to_send = [item['payload'] for item in batch]

        # 컨텍스트 추출 (필터링)
        filtered_glossary, filtered_persons = _extract_filtered_context(
            dialogues_to_send, glossary, person_info
        )

        time.sleep(2)  # Rate Limit 완화
        translated_dialogues, new_glossary, new_persons = _request_gemini_translation(
            dialogues_to_send, filtered_glossary, filtered_persons
        )

        # 번역 결과 매핑 및 갱신
        if translated_dialogues:
            for item, trans_result in zip(batch, translated_dialogues):
                s_name = item['sec_name']
                k = item['key']
                match = re.match(r'^_sel_(\d+)$', k)
                if match:
                    idx = int(match.group(1))
                    to_translation[s_name]['selections'][idx] = trans_result['dialogue']
                    if len(to_translation[s_name]['selections']) == idx+1:
                        to_translation[s_name]['_check'] = True
                else:
                    to_translation[s_name]['keys'][k]['dialogue'] = trans_result['dialogue']
                    to_translation[s_name]['keys'][k]['_check'] = True
                processed_count += 1

            glossary.update(new_glossary)
            person_info.update(new_persons)

            print(f"[{processed_count}/{total_pending}] 번역 완료 (진행률: {processed_count/total_pending*100:.1f}%)")
            
            # 매 배치 완료 후 중간 저장
            _save_progress(person_info, glossary, to_translation)
        else:
            print(f"배치 번역 실패 ({i} ~ {i + len(batch)} 항목). 다음 배치를 진행합니다.")

    print("모든 번역 작업이 완료되었습니다.")

def load_text_from_json(json_path):
    """
    Loads combined Section data from a JSON file.

    Decoding:
    - key: latin-1 decoded to bytes
    - selections, dialogue: euc-kr decoded to str
    """
    with open(json_path, 'rb') as f:
        raw_data = f.read()

    # Read as latin-1 to preserve all bytes, then parse JSON
    data = json.loads(raw_data.decode('latin-1'))

    combined = {}
    for sec_name, sec_data in data.items():
        # Decode selections from euc-kr
        if sec_data.get('selections'):
            selections = []
            for sel in sec_data['selections']:
                try:
                    selections.append(sel.encode('latin-1').decode('euc-kr', errors='replace'))
                except (UnicodeDecodeError, UnicodeEncodeError):
                    selections.append(sel)

            section = {
                'selections': selections,
                '_check': sec_data.get('_check', False)
            }
            
            
        else:
            section = {}
        
        if sec_data.get('keys'):
            section['keys'] = {}
            for key_name, key_data in sec_data['keys'].items():
                # Decode dialogue from euc-kr
                dialogue = key_data['dialogue']
                try:
                    dialogue = dialogue.encode('latin-1').decode('euc-kr', errors='replace')
                except (UnicodeDecodeError, UnicodeEncodeError):
                    pass

                section['keys'][key_name] = {
                    'dialogue': dialogue,
                    'speaker': key_data['speaker'],
                    '_check': key_data.get('_check', False)
                }
                if key_data.get('desc'):
                    section['keys'][key_name]['desc'] = key_data['desc']
                    
        combined[sec_name] = section

    print(f"Combined data loaded from JSON: {json_path}")
    print(f"Total sections: {len(combined)}")

    return combined

def save_text_to_json(json_path, data):
    """
    save combined Section data to a JSON file.

    Decoding:
    - key: latin-1 decoded to bytes
    - selections, dialogue: euc-kr decoded to str
    """
    
    combined = {}
    for sec_name, sec_data in data.items():
        # Decode selections from euc-kr
        if sec_data.get('selections'):
            selections = []
            for sel in sec_data['selections']:
                try:
                    selections.append(sel.encode('euc-kr', errors='replace').decode('latin-1'))
                except (UnicodeDecodeError, UnicodeEncodeError):
                    selections.append(sel)

            section = {
                'selections': selections,
                '_check': sec_data.get('_check')
            }
        else:
            section = {}
            
        if sec_data.get('keys'):
            section['keys'] = {}
            for key_name, key_data in sec_data['keys'].items():
                # Decode dialogue from euc-kr
                dialogue = key_data['dialogue']
                try:
                    dialogue = dialogue.encode('euc-kr', errors='replace').decode('latin-1')
                except (UnicodeDecodeError, UnicodeEncodeError):
                    pass

                section['keys'][key_name] = {
                    'dialogue': dialogue,
                    'speaker': key_data['speaker'],
                    '_check': key_data.get('_check')
                }
                if key_data.get('desc'):
                    section['keys'][key_name]['desc'] = key_data['desc']
                    
        combined[sec_name] = section

    with open(json_path, 'w', encoding='latin-1') as f:
        json.dump(combined, f, ensure_ascii=False, indent=2)
    
    print(f"Combined data saved to JSON: {json_path}")
    print(f"Total sections: {len(combined)}")


def load_json(json_path):
    with open(json_path, 'rb') as f:
        raw_data = f.read()
    # Read as latin-1 to preserve all bytes, then parse JSON
    data = json.loads(raw_data.decode('utf-8'))
    return data

def save_json(json_path, json_data):
    with open(json_path, 'w', encoding='utf-8') as f:
        json.dump(json_data, f, ensure_ascii=False, indent=2) 

def init_transl_check(to_translation):
    for sec_name, sec_data in to_translation.items():
        if sec_data.get("selections"):
            to_translation[sec_name]['_check'] = False
        
        if sec_data.get("keys"):
            for key_name in sec_data['keys'].keys():
                to_translation[sec_name]['keys'][key_name]['_check'] = False
        

if __name__ == '__main__':
    #if len(sys.argv) < 3:
    #    print("Usage: python analuze_text.py <to_translation.json> <glossary.json>")
    #    sys.exit(1)

    #data_json_path = sys.argv[1]
    #glossary_json_path = sys.argv[2]
    data_json_path = "translation/translated11.json"
    glossary_json_path = "translation/glossary11.json"
    person_json_path = "translation/persons11.json"
    
    sec_data = load_text_from_json(data_json_path)
    #init_transl_check(sec_data)
    
    glossary_data = load_json(glossary_json_path)    
    #person_info = { 'Protagonist' : {'name': 'Player', 'toneandmanner': '담백하고 중립적이며, 질문에 명확하게 답하거나 상황을 정리하는 간결한 말투', 'description': '게임을 진행하고 있는 Player 파티의 리더'} }
    person_info = load_json(person_json_path)
    
    translation_dialogue(sec_data, glossary_data, person_info, 30)
    
    save_json("translation/persons11.json", person_info)
    save_json("translation/glossary11.json", glossary_data)
    save_text_to_json("translation/translated11.json", sec_data)
    
    
