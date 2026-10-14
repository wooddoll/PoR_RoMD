#!/usr/bin/env python3
"""
Subtitle Converter - .sbv / .srt <-> .json

Usage:
    python subtitle_converter.py <input_file> [output_file]

Supported conversions:
    .sbv -> .json
    .srt -> .json
    .json -> .sbv
    .json -> .srt

JSON format:
{
    "start_time": "0:00:01.000",
    "end_time": "0:00:04.500",
    "text": ["line1", "line2"]
}
"""

import json
import re
import sys
import os
from pathlib import Path


# ---------------------------------------------------------------------------
# Time parsing / formatting helpers
# ---------------------------------------------------------------------------

def parse_sbv_time(time_str: str) -> float:
    """Parse SBV time format 'H:MM:SS.mmm' to seconds."""
    parts = time_str.strip().split(':')
    if len(parts) != 3:
        raise ValueError(f"Invalid SBV time format: {time_str}")
    h, m, s = parts
    return int(h) * 3600 + int(m) * 60 + float(s)


def parse_srt_time(time_str: str) -> float:
    """Parse SRT time format 'HH:MM:SS,mmm' to seconds."""
    parts = time_str.strip().split(':')
    if len(parts) != 3:
        raise ValueError(f"Invalid SRT time format: {time_str}")
    h, m, s = parts
    s = s.replace(',', '.')
    return int(h) * 3600 + int(m) * 60 + float(s)


def format_sbv_time(seconds: float) -> str:
    """Format seconds to SBV time 'H:MM:SS.mmm'."""
    ms = int(round((seconds - int(seconds)) * 1000))
    total_seconds = int(seconds)
    h = total_seconds // 3600
    m = (total_seconds % 3600) // 60
    s = total_seconds % 60
    return f"{h}:{m:02d}:{s:02d}.{ms:03d}"


def format_srt_time(seconds: float) -> str:
    """Format seconds to SRT time 'HH:MM:SS,mmm'."""
    ms = int(round((seconds - int(seconds)) * 1000))
    total_seconds = int(seconds)
    h = total_seconds // 3600
    m = (total_seconds % 3600) // 60
    s = total_seconds % 60
    return f"{h:02d}:{m:02d}:{s:02d},{ms:03d}"


# ---------------------------------------------------------------------------
# Parsers (subtitle -> json)
# ---------------------------------------------------------------------------

def parse_sbv(content: str) -> list:
    """Parse SBV subtitle content into a list of dicts."""
    entries = []
    lines = content.splitlines()
    i = 0
    while i < len(lines):
        line = lines[i].strip()
        if not line:
            i += 1
            continue

        # Time line: "0:00:01.000,0:00:04.500"
        if ',' in line and ':' in line:
            time_part, text_part = line.split(',', 1)
            try:
                start = parse_sbv_time(time_part)
                end = parse_sbv_time(text_part)
            except ValueError:
                i += 1
                continue

            # Collect text lines until blank line or next time line
            text_lines = []
            i += 1
            while i < len(lines):
                next_line = lines[i].strip()
                if not next_line:
                    break
                # Check if it's a new time line
                if ',' in next_line and ':' in next_line:
                    try:
                        t1, t2 = next_line.split(',', 1)
                        parse_sbv_time(t1)
                        parse_sbv_time(t2)
                        break  # Next entry starts
                    except ValueError:
                        pass
                text_lines.append(lines[i].rstrip())
                i += 1

            entries.append({
                "start_time": format_sbv_time(start),
                "end_time": format_sbv_time(end),
                "text": text_lines
            })
        else:
            i += 1

    return entries


def parse_srt(content: str) -> list:
    """Parse SRT subtitle content into a list of dicts."""
    entries = []
    lines = content.splitlines()
    i = 0
    while i < len(lines):
        line = lines[i].strip()
        if not line:
            i += 1
            continue

        # Skip index number (count is ignored)
        if line.isdigit():
            i += 1
            if i >= len(lines):
                break
            line = lines[i].strip()

        # Time line: "00:00:01,000 --> 00:00:04,500"
        if '-->' in line:
            time_part = line.split('-->')
            if len(time_part) != 2:
                i += 1
                continue
            try:
                start = parse_srt_time(time_part[0].strip())
                end = parse_srt_time(time_part[1].strip())
            except ValueError:
                i += 1
                continue

            # Collect text lines until blank line or next time line
            text_lines = []
            i += 1
            while i < len(lines):
                next_line = lines[i].strip()
                if not next_line:
                    break
                if '-->' in next_line:
                    break
                text_lines.append(lines[i].rstrip())
                i += 1

            entries.append({
                "start_time": format_srt_time(start),
                "end_time": format_srt_time(end),
                "text": text_lines
            })
        else:
            i += 1

    return entries


# ---------------------------------------------------------------------------
# Writers (json -> subtitle)
# ---------------------------------------------------------------------------

def write_sbv(entries: list) -> str:
    """Convert list of dicts to SBV format string."""
    blocks = []
    for entry in entries:
        start = entry["start_time"]
        end = entry["end_time"]
        text = entry["text"]
        if isinstance(text, list):
            text = "\n".join(text)
        blocks.append(f"{start},{end}\n{text}")
    return "\n\n".join(blocks) + "\n"


def write_srt(entries: list) -> str:
    """Convert list of dicts to SRT format string."""
    blocks = []
    for idx, entry in enumerate(entries, 1):
        start = entry["start_time"]
        end = entry["end_time"]
        text = entry["text"]
        if isinstance(text, list):
            text = "\n".join(text)
        blocks.append(f"{idx}\n{start} --> {end}\n{text}")
    return "\n\n".join(blocks) + "\n"


# ---------------------------------------------------------------------------
# JSON helpers
# ---------------------------------------------------------------------------

def load_json(filepath: str) -> list:
    """Load JSON file. Accepts either a list of entries or a dict with entries."""
    with open(filepath, 'r', encoding='utf-8') as f:
        data = json.load(f)

    if isinstance(data, list):
        return data
    elif isinstance(data, dict):
        # Try common keys
        for key in ('entries', 'subtitles', 'data'):
            if key in data and isinstance(data[key], list):
                return data[key]
        raise ValueError(f"Unrecognized JSON structure in {filepath}")
    else:
        raise ValueError(f"JSON must be a list or dict, got {type(data)}")


def save_json(entries: list, filepath: str) -> None:
    """Save entries to JSON file."""
    with open(filepath, 'w', encoding='utf-8') as f:
        json.dump(entries, f, ensure_ascii=False, indent=2)


# ---------------------------------------------------------------------------
# Main conversion logic
# ---------------------------------------------------------------------------

def convert(input_path: str, output_path: str = None) -> str:
    """
    Convert between subtitle formats.
    Returns the output file path.
    """
    input_path = Path(input_path)
    ext = input_path.suffix.lower()

    if output_path is None:
        # Auto-generate output path
        if ext == '.json':
            # Can't auto-determine target format, default to .sbv
            output_path = input_path.with_suffix('.sbv')
        else:
            output_path = input_path.with_suffix('.json')
    else:
        output_path = Path(output_path)

    out_ext = output_path.suffix.lower()

    # Read input
    with open(input_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # Parse to entries
    if ext == '.sbv':
        entries = parse_sbv(content)
    elif ext == '.srt':
        entries = parse_srt(content)
    elif ext == '.json':
        entries = load_json(str(input_path))
    else:
        raise ValueError(f"Unsupported input format: {ext}")

    # Write output
    if out_ext == '.json':
        save_json(entries, str(output_path))
    elif out_ext == '.sbv':
        result = write_sbv(entries)
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(result)
    elif out_ext == '.srt':
        result = write_srt(entries)
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(result)
    else:
        raise ValueError(f"Unsupported output format: {out_ext}")

    return str(output_path)


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = sys.argv[2] if len(sys.argv) > 2 else None

    try:
        result = convert(input_file, output_file)
        print(f"Converted: {input_file} -> {result}")
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()