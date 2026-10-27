#!/usr/bin/env python3
"""
Merge translation data from glossary_latest.json (file A) into glossary.json (file B).

Rules:
1. If a name in B exactly matches a name in A (case-insensitive),
   copy A's content (korean, category, description) into B.
2. If any word from A's name appears as a whole word in B's name, or any word
   from B's name appears as a whole word in A's name (case-insensitive),
   fill B's description with '[korean] / [description]' format.

Usage:
    python merge_glossary.py [--source FILE] [--target FILE] [--output FILE]

Options:
    --source FILE  Source glossary file A (default: translation/glossary_latest.json)
    --target FILE  Target glossary file B (default: glossary.json)
    --output FILE  Output file (default: glossary_merged.json)
"""

import argparse
import json
import os
import sys

DEFAULT_SOURCE = 'translation/glossary_latest.json'
DEFAULT_TARGET = 'glossary.json'
DEFAULT_OUTPUT = 'glossary_merged.json'


def load_json(filepath: str) -> dict:
    """Load a JSON file and return its contents as a dict."""
    if not os.path.isfile(filepath):
        print(f'Error: File not found: {filepath}', file=sys.stderr)
        sys.exit(1)

    with open(filepath, 'r', encoding='utf-8') as f:
        data = json.load(f)

    if not isinstance(data, dict):
        print(f'Error: {filepath} must be a JSON object', file=sys.stderr)
        sys.exit(1)

    return data


# Common English stop words to exclude from word matching
STOP_WORDS = {
    'a', 'an', 'the', 'and', 'or', 'of', 'to', 'in', 'on', 'at', 'for',
    'with', 'by', 'from', 'as', 'is', 'are', 'was', 'were', 'be', 'been',
    'can', 'could', 'may', 'might', 'must', 'shall', 'should', 'will',
    'would', 'this', 'that', 'these', 'those', 'it', 'its', 'his', 'her',
    'their', 'our', 'your', 'my', 'me', 'him', 'them', 'us', 'we', 'you',
    'he', 'she', 'they', 'i', 'not', 'no', 'yes', 'only', 'all', 'any',
    'both', 'each', 'few', 'more', 'most', 'other', 'some', 'such',
    'than', 'too', 'very', 'just', 'but', 'so', 'if', 'then', 'than',
    'also', 'into', 'over', 'under', 'again', 'further', 'once', 'here',
    'there', 'when', 'where', 'why', 'how', 'up', 'down', 'out', 'off',
    'about', 'above', 'below', 'between', 'through', 'during', 'before',
    'after', 'against', 'among', 'around', 'because', 'until', 'while',
    'per', 'via', 'upon', 'within', 'without', 'has', 'have', 'had',
    'does', 'did', 'do', 'use', 'used', 'using', 'can', 'cannot',
}


def _words(name: str) -> list:
    """Split a name into lowercase words, excluding stop words."""
    return [w for w in name.lower().split() if w not in STOP_WORDS]


def _is_subsequence(sub: list, full: list) -> bool:
    """Check if all words in `sub` appear in order within `full`."""
    it = iter(full)
    return all(w in it for w in sub)

def cvt_tmp(target_file: str):
    _target = load_json(target_file)
    target = {}
    for k, v in _target.items():
        target[k] = { 'korean': v[0], "category": "", 'description': '' }
    return target

def merge_glossaries(source_file: str, target_file: str, output_file: str) -> None:
    """Merge data from source glossary into target glossary."""
    source = cvt_tmp(source_file)
    target = load_json(target_file)

    # Build case-insensitive lookup for source names
    source_lookup = {}
    for name, info in source.items():
        source_lookup[name.lower()] = (name, info)

    # Track statistics
    exact_matches = 0
    word_matches = 0
    no_match = 0

    # Process each entry in target
    for target_name in list(target.keys()):
        target_lower = target_name.lower()
        target_info = target[target_name]

        matched = False
        # Rule 1: Exact match (case-insensitive)
        if target_lower in source_lookup:
            source_name, source_info = source_lookup[target_lower]
            target_info['korean'] = source_info.get('korean', '')
            #target_info['category'] = source_info.get('category', '')
            #target_info['description'] = source_info.get('description', '')
            exact_matches += 1
            matched = True
            continue

        '''# Rule 2: Sentence-level match (case-insensitive)
        # Check if one name's entire word sequence is contained within the other.
        target_words = _words(target_name)
        matched = False
        for source_lower, (source_name, source_info) in source_lookup.items():
            source_words = _words(source_name)
            # One name must be fully contained within the other (word subsequence)
            if _is_subsequence(target_words, source_words) or _is_subsequence(source_words, target_words):
                # Fill B's description with '[korean] / [description]' format
                korean = source_info.get('korean', '')
                description = source_info.get('description', '')
                target_info['description'] = f'{korean} / {description}'
                word_matches += 1
                matched = True
                break'''

        if not matched:
            no_match += 1

    # Write output
    with open(output_file, 'w', encoding='utf-8') as f:
        json.dump(target, f, ensure_ascii=False, indent=2)

    print(f'Merged {source_file} into {target_file}')
    print(f'  Exact matches: {exact_matches}')
    print(f'  Word matches: {word_matches}')
    print(f'  No match: {no_match}')
    print(f'Output written to: {output_file}')


def main():
    parser = argparse.ArgumentParser(
        description='Merge translation data from glossary_latest.json into glossary.json'
    )
    parser.add_argument('--source', default=DEFAULT_SOURCE,
                        help=f'Source glossary file A (default: {DEFAULT_SOURCE})')
    parser.add_argument('--target', default=DEFAULT_TARGET,
                        help=f'Target glossary file B (default: {DEFAULT_TARGET})')
    parser.add_argument('--output', default=DEFAULT_OUTPUT,
                        help=f'Output file (default: {DEFAULT_OUTPUT})')

    args = parser.parse_args()

    merge_glossaries(args.source, args.target, args.output)


if __name__ == '__main__':
    main()