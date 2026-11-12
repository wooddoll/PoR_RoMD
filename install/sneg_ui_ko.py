"""SNEG UI 문자열(sneg_ui_strings.dat) 한국어 번역 도구.

번역 원본: translation/sneg_ui_strings_ko.json  (항목: key, en, ko, ref)
  - ko 가 비어 있으면 영어 그대로 쓴다.
  - ref: 튜토리얼 항목(ui.tutorial.<kbm|pad>.<파일>.<줄ID>)의 기존 한국어 대사 (.por) — 참고용, 빌드에 쓰지 않음.
  - [MOUSE_LEFT] 같은 아이콘 토큰은 영어와 같은 종류/개수로 넣어야 한다 (build 가 확인).

사용:
  python sneg_ui_ko.py extract <게임폴더>   # 게임의 languages/ko/sneg_ui_strings.dat 에서 JSON 생성/갱신 (기존 ko 유지)
  python sneg_ui_ko.py build   <게임폴더> [출력 dat]
        # JSON → dat (기본: <게임폴더>/languages/ko/sneg_ui_strings.dat),
        # 번역에 쓴 글자 중 languages/ko/sneg_ui_cjk_characters.txt 에 없는 글자를 추가
  python sneg_ui_ko.py status                # 번역 진행 상황
"""
import collections
import json
import os
import re
import sys

import sneg_ui_strings as suis

HERE = os.path.dirname(os.path.abspath(__file__))
JSON_PATH = os.path.join(HERE, '..', 'translation', 'sneg_ui_strings_ko.json')
TOKEN_RE = re.compile(r'\[[A-Z][A-Z0-9_]*\]')
TUT_RE = re.compile(r'^ui\.tutorial\.(kbm|pad)\.([a-z0-9]+)\.([a-z0-9]+)$')


def load_por_lines(game, name):
    p = os.path.join(game, 'languages', 'ko', 'data', 'Dialogue', name + '.por')
    if not os.path.exists(p):
        return {}
    out = {}
    for line in open(p, 'rb').read().split(b'\r\n'):
        parts = line.split(b'\t')
        if len(parts) >= 2:
            out[parts[0].decode('ascii', 'replace')] = parts[1].decode('cp949', 'replace')
    return out


def load_json():
    if not os.path.exists(JSON_PATH):
        return []
    return json.load(open(JSON_PATH, encoding='utf-8'))


def save_json(items):
    os.makedirs(os.path.dirname(JSON_PATH), exist_ok=True)
    with open(JSON_PATH, 'w', encoding='utf-8', newline='\n') as f:
        json.dump(items, f, ensure_ascii=False, indent=1)
        f.write('\n')


def dat_path(game):
    return os.path.join(game, 'languages', 'ko', 'sneg_ui_strings.dat')


def extract(game):
    _, items = suis.load(dat_path(game))
    old = {it['key']: it for it in load_json()}
    por_cache = {}
    out = []
    for k, cp, v in items:
        en = suis.text(cp, v)
        prev = old.get(k, {})
        if prev.get('en') and prev['en'] != en:
            en = prev['en']             # 이미 번역된 dat 에서 다시 뽑는 경우: 영어 원문은 JSON 것을 유지
        ent = {'key': k, 'en': en, 'ko': prev.get('ko', '')}
        m = TUT_RE.match(k)
        if m:
            name, lid = m.group(2), m.group(3)
            if name not in por_cache:
                por_cache[name] = load_por_lines(game, name)
            ref = por_cache[name].get(lid, '')
            if ref and ref != 'null':
                ent['ref'] = ref
        out.append(ent)
    save_json(out)
    print(f'{len(out)} items -> {os.path.normpath(JSON_PATH)}')


def tokens(s):
    return collections.Counter(TOKEN_RE.findall(s))


def build(game, out_path=None):
    lang, items = suis.load(dat_path(game))
    tr = {it['key']: it for it in load_json()}
    errors, used, n = [], set(), 0
    new_items = []
    for k, cp, v in items:
        t = tr.get(k)
        if t and t.get('ko'):
            if tokens(t['ko']) != tokens(t['en']):
                errors.append(f'{k}: token mismatch {dict(tokens(t["en"]))} != {dict(tokens(t["ko"]))}')
            new_items.append([k, 65001, t['ko'].encode('utf-8')])
            used.update(c for c in TOKEN_RE.sub('', t['ko']) if ord(c) >= 0x80)
            n += 1
        elif t:
            new_items.append([k, 65001, t['en'].encode('utf-8')])
        else:
            new_items.append([k, cp, v])
    if errors:
        print('\n'.join(errors))
        sys.exit(1)
    out_path = out_path or dat_path(game)
    suis.save(out_path, lang, new_items)
    print(f'{n}/{len(new_items)} translated -> {out_path}')

    # UI 글꼴 글자 목록에 없는 글자 추가 (UTF-8 BOM, 한 줄)
    lst = os.path.join(game, 'languages', 'ko', 'sneg_ui_cjk_characters.txt')
    have = set(open(lst, encoding='utf-8-sig').read()) if os.path.exists(lst) else set()
    missing = sorted(c for c in used if c not in have)
    if missing:
        with open(lst, 'a', encoding='utf-8') as f:
            f.write(''.join(missing))
        print(f'added {len(missing)} chars to {lst}: {"".join(missing)}')


def apply(patch_path):
    """{key: ko} JSON 을 번역 원본에 합친다 (토큰 확인)."""
    patch = json.load(open(patch_path, encoding='utf-8'))
    items = load_json()
    by_key = {it['key']: it for it in items}
    bad = []
    for k, ko in patch.items():
        it = by_key.get(k)
        if not it:
            bad.append(f'{k}: unknown key')
        elif tokens(ko) != tokens(it['en']):
            bad.append(f'{k}: token mismatch {dict(tokens(it["en"]))} != {dict(tokens(ko))}')
        else:
            it['ko'] = ko
    if bad:
        print('\n'.join(bad))
    save_json(items)
    print(f'applied {len(patch) - len(bad)}/{len(patch)}')


def status():
    items = load_json()
    groups = collections.defaultdict(lambda: [0, 0])
    for it in items:
        g = '.'.join(it['key'].split('.')[:3]) if it['key'].startswith('ui.tutorial') else '.'.join(it['key'].split('.')[:2])
        groups[g][1] += 1
        if it.get('ko'):
            groups[g][0] += 1
    for g, (d, t) in sorted(groups.items(), key=lambda x: -x[1][1]):
        print(f'{g:40s} {d:4d}/{t}')
    print('total', sum(d for d, _ in groups.values()), '/', len(items))


if __name__ == '__main__':
    cmd = sys.argv[1] if len(sys.argv) > 1 else 'status'
    if cmd == 'extract':
        extract(sys.argv[2])
    elif cmd == 'build':
        build(sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else None)
    elif cmd == 'apply':
        apply(sys.argv[2])
    else:
        status()
