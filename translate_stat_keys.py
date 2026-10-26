import json
import re
import sys
import os

# ============================================================
# Translation mappings
# ============================================================

# Stat name translations
STAT_TRANSLATIONS = {
    "STR": "STR",
    "DEX": "DEX",
    "CON": "CON",
    "WIS": "WIS",
    "INT": "INT",
    "CHA": "CHA",
    "AC": "AC",
    "All Saving Throws": "모든 내성 굴림",
    "Fortitude Saving Throws": "인내 내성 굴림",
    "Reflex Saving Throws": "반사 내성 굴림",
    "Will Saving Throws": "의지 내성 굴림",
    "Fortitude Save": "인내 내성",
    "Reflex Save": "반사 내성",
    "Will Save": "의지 내성",
    "Spell Resistance": "주문 저항",
    "Spell Res.": "주문 저항",
    "Initiative": "우선권",
    "To Hit": "명중",
    "Damage": "피해",
    "Undead Protection": "언데드 보호",
    "Vs. Undead Protection": "언데드 보호",
    "Double Damage": "두배 피해",
    "Triple Damage": "세배 피해",
    "Triple damage": "세배 피해"
}

# Race name translations for "Vs. <Race>: <STAT>/<STAT>..." keys
VS_RACE_TRANSLATIONS = {
    "Constructs": "피조물",
    "Dragonkind": "용아종",
    "Drow": "드로우",
    "Evil": "악",
    "Gargoyles": "가고일",
    "Humanoid": "인간형",
    "Light Sensitive": "빛 민감성",
    "Light Sensitive & Undead": "빛 민감성 & 언데드",
    "Orcs and Orog": "오크 & 오로그",
    "Reptiles": "파충류",
    "Undead": "언데드",
}

# Cleric scroll spell name translations
CLERIC_SPELL_TRANSLATIONS = {
    "Bestow Curse": "저주 부여",
    "Blindness": "실명",
    "Command": "명령",
    "Cure Critical Wounds": "치명상 치료",
    "Cure Light Wounds": "가벼운 상처 치료",
    "Cure Serious Wounds": "심각한 상처 치료",
    "Destruction": "파멸",
    "Entangle": "얽어매기",
    "Find Traps": "함정 감지",
    "Fire Storm": "화염 폭풍",
    "Harm": "해악",
    "Heal": "치유",
    "Inflict Serious Wounds": "심각한 상처 입히기",
    "Neutralize Poison": "독 중화",
    "Prayer": "기도",
    "Raise Dead": "죽은 자 살리기",
    "Remove Paralysis": "마비 해제",
    "Resurrection": "부활",
    "Searing Light": "작열하는 빛",
    "Shield of Faith": "신앙의 방패",
    "Silence": "침묵",
    "Slay Living": "생명 살해",
    "Slow Poison": "독 지연",
    "Spiritual Weapon": "영체 무기",
}

# Cast spell name translations
CAST_SPELL_TRANSLATIONS = {
    "Bestow Curse": "저주 부여",
    "Bless": "축복",
    "Blindness": "실명",
    "Blink": "점멸",
    "Bull's Strength": "황소의 힘",
    "Burning Hands": "타오르는 손길",
    "Cat's Grace": "고양이의 우아함",
    "Chain Lightning": "연쇄 번개",
    "Charm Monster": "괴물 매혹",
    "Charm Person": "인간형 매혹",
    "Chill Touch": "소름끼치는 손길",
    "Circle of Death": "죽음의 원",
    "Cloudkill": "죽음구름",
    "Command": "명령",
    "Cone of Cold": "냉기 원뿔",
    "Confusion": "혼란",
    "Control Undead": "언데드 지배",
    "Cure Critical Wounds": "치명상 치료",
    "Cure Serious Wounds": "심각한 상처 치료",
    "Cure Moderate Wounds": "상처 치료",
    "Cure Light Wounds": "가벼운 상처 치료",
    "Darkness": "어둠",
    "Delay Poison": "독 지연",
    "Destruction": "파멸",
    "Disintegrate": "분해",
    "Dispel Magic": "마법 해제",
    "Displacement": "위치 변위",
    "Divine Power": "신성한 힘",
    "Dominate Person": "인간형 지배",
    "Entangle": "얽어매기",
    "Fear": "공포",
    "Feeblemind": "정신 박약",
    "Find Traps": "함정 감지",
    "Finger of Death": "죽음의 손가락",
    "Fire Shield": "화염 방패",
    "Fire Storm": "화염 폭풍",
    "Fireball": "화염구",
    "Flame Strike": "불길 강타",
    "Freedom of Movement": "이동의 자유",
    "Globe of Invulnerability": "피해 면역 구체",
    "Halt Undead": "언데드 정지",
    "Harm": "해악",
    "Haste": "가속",
    "Heal": "치유",
    "Hold Monster": "괴물 포박",
    "Hold Person": "인간형 포박",
    "Hold Poison": "독 포박",
    "Improved Invisibility": "상급 투명화",
    "Incendiary Cloud": "화염 구름",
    "Inflict Critical Wounds": "치명상 입히기",
    "Inflict Serious Wounds": "심각한 상처 입히기",
    "Invisibility": "투명화",
    "Invisibility Sphere": "투명화 구체",
    "Light": "빛",
    "Lightning Bolt": "번개 줄기",
    "Mage Armor": "마법사 갑옷",
    "Magic Missile": "매직 미사일",
    "Mass Suggestion": "광역 암시",
    "Melf's Acid Arrow": "멜프의 산성 화살",
    "Minor Globe of Invulnerability": "하급 피해 면역 구체",
    "Neutralize Poison": "독 중화",
    "Poison": "독",
    "Prayer": "기도",
    "Protection from Evil": "악으로부터의 보호",
    "Protection from Poison": "독으로부터의 보호",
    "Raise Dead": "죽은 자 살리기",
    "Remove Blindness": "실명 해제",
    "Remove Fear": "공포 해제",
    "Remove Paralysis": "마비 해제",
    "Repulsion": "척력장",
    "Restoration": "회복",
    "Resurrection": "부활",
    "Searing Light": "작열하는 빛",
    "See Invisibility": "투명체 감지",
    "Shield": "방패",
    "Shield of Faith": "신앙의 방패",
    "Shocking Grasp": "전격 손아귀",
    "Silence": "침묵",
    "Slay Living": "생명 살해",
    "Sleep": "수면",
    "Slow": "감속",
    "Spell Resistance": "주문 저항",
    "Spiritual Weapon": "영체 무기",
    "Stinking Cloud": "악취 구름",
    "True Seeing": "진실의 시야",
    "Vampiric Touch": "흡혈의 손길",
    "Web": "거미줄",
}

# ============================================================
# Pattern matching
# ============================================================

# Stat key pattern: stat name + +/- number, multiple separated by /
STAT_NAMES = sorted(STAT_TRANSLATIONS.keys(), key=len, reverse=True)
STAT_PATTERN = re.compile(
    r'^(?:' + '|'.join(re.escape(name) for name in STAT_NAMES) + r')\s*[+-]\s*\d+'
    r'(?:/(?:' + '|'.join(re.escape(name) for name in STAT_NAMES) + r')\s*[+-]\s*\d+)*$'
)

# Cleric scroll key pattern: "Cleric Scroll of <Spell Name> Level <number>[/additional text]"
CLERIC_SPELL_NAMES = sorted(CLERIC_SPELL_TRANSLATIONS.keys(), key=len, reverse=True)
CLERIC_SCROLL_PATTERN = re.compile(
    r'^Cleric Scroll of (?:' + '|'.join(re.escape(name) for name in CLERIC_SPELL_NAMES) + r') Level \d+'
    r'(?:/.*)?$'
)

# Cast key pattern: "Cast <Spell Name> Level <number>[/stat combos]"
CAST_SPELL_NAMES = sorted(CAST_SPELL_TRANSLATIONS.keys(), key=len, reverse=True)
CAST_PATTERN = re.compile(
    r'^Cast (?:' + '|'.join(re.escape(name) for name in CAST_SPELL_NAMES) + r')\s+Level \d+'
    r'(?:/.*)?$'
)

# Vs. key pattern: "Vs. <Race>: <STAT>[/<STAT>...]"
# STAT can be "Name +N" or just "Name" (e.g., "Double Damage", "Triple Damage")
VS_RACE_NAMES = sorted(VS_RACE_TRANSLATIONS.keys(), key=len, reverse=True)
VS_PATTERN = re.compile(
    r'^Vs\. (?:' + '|'.join(re.escape(name) for name in VS_RACE_NAMES) + r'):\s*'
    r'(?:' + '|'.join(re.escape(name) for name in STAT_NAMES) + r')(?:\s*[+-]\s*\d+)?'
    r'(?:/(?:' + '|'.join(re.escape(name) for name in STAT_NAMES) + r')(?:\s*[+-]\s*\d+)?)*\s*$'
)


def is_stat_key(key):
    """Check if the key matches the stat pattern (e.g., 'WIS +1/Spell Resistance +10')."""
    return bool(STAT_PATTERN.match(key.strip()))


def is_cleric_scroll_key(key):
    """Check if the key matches the cleric scroll pattern (e.g., 'Cleric Scroll of Cure Light Wounds Level 5')."""
    return bool(CLERIC_SCROLL_PATTERN.match(key.strip()))


def is_cast_key(key):
    """Check if the key matches the cast pattern (e.g., 'Cast Bestow Curse Level 10/To Hit +5/Damage +5')."""
    return bool(CAST_PATTERN.match(key.strip()))


def is_vs_key(key):
    """Check if the key matches the vs pattern (e.g., 'Vs. Undead: To Hit +3/Damage +3')."""
    return bool(VS_PATTERN.match(key.strip()))


def translate_stat_key(key):
    """Translate a stat key to Korean.

    Handles both stats with numbers (e.g., 'To Hit +3') and without
    (e.g., 'Double Damage', 'Triple Damage').
    """
    parts = key.split('/')
    translated_parts = []
    for part in parts:
        part = part.strip()
        # Match stat name and optional number
        for name in STAT_NAMES:
            if part.startswith(name):
                rest = part[len(name):].strip()
                # rest can be like "+1", "-3", or empty (for Double/Triple Damage)
                if rest == '' or re.match(r'^[+-]\s*\d+$', rest):
                    if rest:
                        translated_parts.append(f"{STAT_TRANSLATIONS[name]} {rest}")
                    else:
                        translated_parts.append(STAT_TRANSLATIONS[name])
                    break
        else:
            # If no stat name matched, keep the part as-is
            translated_parts.append(part)
    return ' / '.join(translated_parts)


def translate_cleric_scroll_key(key):
    """Translate a cleric scroll key to Korean.

    Format: "Cleric Scroll of <Spell Name> Level <number>[/additional text]"
    Result: "성직자 두루마리: <주문명> Level <number> / 추가 문장"
    """
    key = key.strip()
    # Extract spell name and the rest
    for name in CLERIC_SPELL_NAMES:
        prefix = f"Cleric Scroll of {name} Level "
        if key.startswith(prefix):
            rest = key[len(prefix):]
            # Split additional text if present (after /)
            if '/' in rest:
                level_part, additional = rest.split('/', 1)
                level_part = level_part.strip()
                additional = additional.strip()
                return f"성직자 두루마리: {CLERIC_SPELL_TRANSLATIONS[name]}, Level {level_part} / {additional}"
            else:
                level_part = rest.strip()
                return f"성직자 두루마리: {CLERIC_SPELL_TRANSLATIONS[name]}, Level {level_part}"
    # Should not reach here if is_cleric_scroll_key was checked first
    return key


def translate_cast_key(key):
    """Translate a cast key to Korean.

    Format: "Cast <Spell Name> Level <number>[/stat combos]"
    Result: "시전: <주문명>, Level <number> / stat combos"
    """
    key = key.strip()
    # Extract spell name and the rest
    for name in CAST_SPELL_NAMES:
        prefix = f"Cast {name}"
        if key.startswith(prefix):
            rest = key[len(prefix):].strip()
            # rest should be like "Level 10" or "Level 10/To Hit +5/Damage +5"
            if rest.startswith("Level "):
                rest = rest[len("Level "):]
                if '/' in rest:
                    level_part, additional = rest.split('/', 1)
                    level_part = level_part.strip()
                    additional = additional.strip()
                    # Translate the stat combos in additional
                    translated_additional = translate_stat_key(additional)
                    return f"시전: {CAST_SPELL_TRANSLATIONS[name]}, Level {level_part} / {translated_additional}"
                else:
                    level_part = rest.strip()
                    return f"시전: {CAST_SPELL_TRANSLATIONS[name]}, Level {level_part}"
    # Should not reach here if is_cast_key was checked first
    return key


def translate_vs_key(key):
    """Translate a vs key to Korean.

    Format: "Vs. <Race>: <STAT>[/<STAT>...]"
    Result: "Vs. <종족>: <STAT> / <STAT> ..."
    """
    key = key.strip()
    # Extract race name and the rest
    for name in VS_RACE_NAMES:
        prefix = f"Vs. {name}:"
        if key.startswith(prefix):
            rest = key[len(prefix):].strip()
            # Translate the stat combos
            translated_rest = translate_stat_key(rest)
            return f"Vs. {VS_RACE_TRANSLATIONS[name]}: {translated_rest}"
    # Should not reach here if is_vs_key was checked first
    return key


def process_file(filepath, mode):
    """Process the JSON file with the given mode.

    mode: 'stat', 'cleric_scroll', 'cast', or 'vs'
    """
    if not os.path.exists(filepath):
        print(f"Error: File not found: {filepath}")
        sys.exit(1)

    with open(filepath, 'r', encoding='utf-8') as f:
        data = json.load(f)

    if mode == 'stat':
        key_checker = is_stat_key
        translator = translate_stat_key
        mode_name = "stat"
    elif mode == 'cleric_scroll':
        key_checker = is_cleric_scroll_key
        translator = translate_cleric_scroll_key
        mode_name = "cleric scroll"
    elif mode == 'cast':
        key_checker = is_cast_key
        translator = translate_cast_key
        mode_name = "cast"
    elif mode == 'vs':
        key_checker = is_vs_key
        translator = translate_vs_key
        mode_name = "vs"
    else:
        print(f"Error: Unknown mode '{mode}'. Use 'stat', 'cleric_scroll', 'cast', or 'vs'.")
        sys.exit(1)

    updated_count = 0
    warning_count = 0
    unchanged_count = 0

    for key, value in data.items():
        if not isinstance(value, dict):
            continue

        if not key_checker(key):
            continue

        generated = translator(key)
        existing = value.get('korean', '')

        if not existing:
            # No existing translation, set it
            value['korean'] = generated
            updated_count += 1
            print(f"[SET] {key} -> {generated}")
        elif existing == generated:
            # Existing translation matches
            unchanged_count += 1
            print(f"[OK] {key} -> {existing}")
        else:
            # Existing translation doesn't match
            warning_count += 1
            print(f"[WARNING] {key}")
            print(f"    Existing: {existing}")
            print(f"    Generated: {generated}")

    # Write back to file
    with open(filepath, 'w', encoding='utf-8') as f:
        json.dump(data, f, ensure_ascii=False, indent=2)

    print(f"\n=== Summary ({mode_name}) ===")
    print(f"Updated: {updated_count}")
    print(f"Unchanged (already correct): {unchanged_count}")
    print(f"Warnings (mismatch): {warning_count}")


def main():
    if len(sys.argv) < 2:
        print("Usage: python translate_stat_keys.py <mode> [filepath]")
        print("  mode: 'stat'           - translate stat keys (e.g., 'WIS +1/Spell Resistance +10')")
        print("        'cleric_scroll'  - translate cleric scroll keys (e.g., 'Cleric Scroll of Cure Light Wounds Level 5')")
        print("        'cast'           - translate cast keys (e.g., 'Cast Bestow Curse Level 10/To Hit +5/Damage +5')")
        print("        'vs'             - translate vs keys (e.g., 'Vs. Undead: To Hit +3/Damage +3')")
        print("  filepath: optional, default is 'translation/str_glossary_latest.json'")
        sys.exit(1)

    mode = sys.argv[1]
    filepath = sys.argv[2] if len(sys.argv) > 2 else 'translation/str_glossary_latest.json'
    process_file(filepath, mode)


if __name__ == '__main__':
    main()