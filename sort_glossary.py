import json


def load_json(json_path):
    with open(json_path, 'rb') as f:
        raw_data = f.read()
    # Read as latin-1 to preserve all bytes, then parse JSON
    data = json.loads(raw_data.decode('utf-8'))
    return data

def save_json(json_path, json_data):
    with open(json_path, 'w', encoding='utf-8') as f:
        json.dump(json_data, f, ensure_ascii=False, indent=2) 

if __name__ == '__main__':
    glossary_json_path = "translation/glossary10.json"
    glossary = load_json(glossary_json_path)    
    
    sorted_glossary = dict(
        sorted(glossary.items(), key=lambda item: (item[1]['category'], item[0]))
    )

    save_json("translation/glossary10a.json", sorted_glossary)
    