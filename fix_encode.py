#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
fix_encode.py - 텍스트 파일 인코딩 변환 도구 (EUC-KR <-> UTF-8)

특수문자(0x09 탭, 0x0D CR, 0x0A LF)는 그대로 유지하면서
EUC-KR로 저장된 텍스트 파일을 UTF-8로, 또는 그 반대로 변환합니다.

사용법:
    python fix_encode.py <입력파일> <출력파일> [--to-utf8|--to-euckr]
    python fix_encode.py <입력폴더> <출력폴더> --dir [--ext por] [--recursive]

옵션:
    --to-utf8    : EUC-KR -> UTF-8 변환 (기본값)
    --to-euckr   : UTF-8 -> EUC-KR 변환
    --overwrite  : 출력 파일이 이미 존재해도 덮어쓰기
    --dir        : 폴더 단위 일괄 변환 모드
    --ext EXT    : 변환할 확장자 (기본값: por, 콤마로 구분 가능 예: por,txt)
    --recursive  : 하위 폴더까지 재귀 탐색
    --no-verify  : 특수문자 보존 검증 생략
"""

import sys
import os
import argparse
from pathlib import Path

# 보존해야 할 특수문자 바이트 값들
PRESERVE_BYTES = {0x09, 0x0D, 0x0A}  # 탭, CR, LF


def convert_euckr_to_utf8(input_path, output_path):
    """
    EUC-KR 인코딩 파일을 UTF-8로 변환합니다.
    특수문자(0x09, 0x0D, 0x0A)는 그대로 유지됩니다.
    """
    try:
        with open(input_path, 'rb') as f:
            data = f.read()
    except FileNotFoundError:
        print(f"  오류: 입력 파일을 찾을 수 없습니다: {input_path}")
        return False
    except Exception as e:
        print(f"  오류: 입력 파일 읽기 실패: {e}")
        return False

    try:
        # EUC-KR로 디코딩 (cp949는 EUC-KR의 확장판)
        text = data.decode('euc-kr')
    except UnicodeDecodeError as e:
        print(f"  오류: EUC-KR 디코딩 실패 (파일이 EUC-KR이 아닐 수 있습니다): {e}")
        return False

    # UTF-8로 인코딩
    # 특수문자(0x09, 0x0D, 0x0A)는 유니코드에서도 동일한 코드포인트를 가지므로
    # 인코딩 과정에서 자동으로 보존됩니다.
    result = text.encode('utf-8')

    try:
        with open(output_path, 'wb') as f:
            f.write(result)
        return True
    except Exception as e:
        print(f"  오류: 출력 파일 쓰기 실패: {e}")
        return False


def convert_utf8_to_euckr(input_path, output_path):
    """
    UTF-8 인코딩 파일을 EUC-KR로 변환합니다.
    특수문자(0x09, 0x0D, 0x0A)는 그대로 유지됩니다.
    """
    try:
        with open(input_path, 'rb') as f:
            data = f.read()
    except FileNotFoundError:
        print(f"  오류: 입력 파일을 찾을 수 없습니다: {input_path}")
        return False
    except Exception as e:
        print(f"  오류: 입력 파일 읽기 실패: {e}")
        return False

    try:
        # UTF-8로 디코딩
        text = data.decode('utf-8')
    except UnicodeDecodeError as e:
        print(f"  오류: UTF-8 디코딩 실패 (파일이 UTF-8이 아닐 수 있습니다): {e}")
        return False

    try:
        # EUC-KR로 인코딩
        result = text.encode('euc-kr')
    except UnicodeEncodeError as e:
        print(f"  오류: EUC-KR 인코딩 실패 (EUC-KR로 표현할 수 없는 문자가 있습니다): {e}")
        return False

    try:
        with open(output_path, 'wb') as f:
            f.write(result)
        return True
    except Exception as e:
        print(f"  오류: 출력 파일 쓰기 실패: {e}")
        return False


def verify_special_chars(input_path, output_path):
    """
    변환 전후의 특수문자(0x09, 0x0D, 0x0A) 바이트가 동일한지 검증합니다.
    """
    try:
        with open(input_path, 'rb') as f:
            input_data = f.read()
        with open(output_path, 'rb') as f:
            output_data = f.read()
    except Exception as e:
        print(f"  검증 오류: {e}")
        return False

    # 특수문자 바이트만 추출하여 비교
    input_special = bytes(b for b in input_data if b in PRESERVE_BYTES)
    output_special = bytes(b for b in output_data if b in PRESERVE_BYTES)

    return input_special == output_special


def convert_file(input_path, output_path, to_utf8, verify=True):
    """
    단일 파일 변환을 수행하고 결과를 반환합니다.
    Returns: (success: bool, message: str)
    """
    # 출력 디렉토리 생성
    output_dir = os.path.dirname(output_path)
    if output_dir:
        os.makedirs(output_dir, exist_ok=True)

    # 변환 실행
    if to_utf8:
        success = convert_euckr_to_utf8(input_path, output_path)
    else:
        success = convert_utf8_to_euckr(input_path, output_path)

    if not success:
        return False, "변환 실패"

    # 특수문자 보존 검증
    if verify:
        if not verify_special_chars(input_path, output_path):
            return False, "특수문자 보존 검증 실패"

    return True, "성공"


def convert_directory(src_dir, dst_dir, extensions, to_utf8, recursive=False, overwrite=False, verify=True):
    """
    디렉토리 내 특정 확장자의 모든 파일을 일괄 변환합니다.
    """
    src_path = Path(src_dir)
    dst_path = Path(dst_dir)

    if not src_path.exists():
        print(f"오류: 입력 폴더가 존재하지 않습니다: {src_dir}")
        return False

    if not src_path.is_dir():
        print(f"오류: 입력 경로가 폴더가 아닙니다: {src_dir}")
        return False

    # 확장자 정규화 (소문자, 점 제거)
    ext_set = set()
    for ext in extensions:
        ext = ext.strip().lower()
        if ext.startswith('.'):
            ext = ext[1:]
        if ext:
            ext_set.add(ext)

    if not ext_set:
        print("오류: 유효한 확장자가 없습니다.")
        return False

    # 파일 수집
    files = []
    if recursive:
        for ext in ext_set:
            files.extend(src_path.rglob(f'*.{ext}'))
    else:
        for ext in ext_set:
            files.extend(src_path.glob(f'*.{ext}'))

    # 중복 제거 및 정렬
    files = sorted(set(files))

    if not files:
        print(f"경고: '{src_dir}'에서 확장자가 {sorted(ext_set)}인 파일을 찾지 못했습니다.")
        return False

    print(f"발견된 파일: {len(files)}개")
    print(f"  소스 폴더: {src_dir}")
    print(f"  대상 폴더: {dst_dir}")
    print(f"  확장자: {', '.join(sorted(ext_set))}")
    print(f"  변환 방향: {'EUC-KR -> UTF-8' if to_utf8 else 'UTF-8 -> EUC-KR'}")
    print()

    success_count = 0
    fail_count = 0
    skip_count = 0

    for file_path in files:
        rel_path = file_path.relative_to(src_path)
        out_path = dst_path / rel_path

        # 출력 파일 존재 확인
        if out_path.exists() and not overwrite:
            print(f"  [SKIP] {rel_path} (출력 파일이 이미 존재)")
            skip_count += 1
            continue

        # 변환 수행
        success, message = convert_file(str(file_path), str(out_path), to_utf8, verify)
        if success:
            print(f"  [ OK ] {rel_path}")
            success_count += 1
        else:
            print(f"  [FAIL] {rel_path} ({message})")
            fail_count += 1

    # 결과 요약
    print()
    print("=" * 50)
    print("변환 결과 요약")
    print("=" * 50)
    print(f"  성공: {success_count}")
    print(f"  실패: {fail_count}")
    print(f"  건너뜀: {skip_count}")
    print(f"  총계: {len(files)}")

    return fail_count == 0


def main():
    parser = argparse.ArgumentParser(
        description='텍스트 파일 인코딩 변환 도구 (EUC-KR <-> UTF-8)',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
예시:
  # 단일 파일 EUC-KR -> UTF-8 변환
  python fix_encode.py input.txt output.txt --to-utf8

  # 단일 파일 UTF-8 -> EUC-KR 변환
  python fix_encode.py input.txt output.txt --to-euckr

  # 폴더 내 모든 .por 파일 일괄 변환 (EUC-KR -> UTF-8)
  python fix_encode.py src/ dst/ --dir --ext por

  # 폴더 내 .por, .txt 파일 일괄 변환 (UTF-8 -> EUC-KR)
  python fix_encode.py src/ dst/ --dir --ext por,txt --to-euckr

  # 하위 폴더까지 재귀 탐색
  python fix_encode.py src/ dst/ --dir --ext por --recursive

  # 기존 파일 덮어쓰기 허용
  python fix_encode.py src/ dst/ --dir --ext por --overwrite
"""
    )
    parser.add_argument('input', help='입력 파일 또는 폴더 경로')
    parser.add_argument('output', help='출력 파일 또는 폴더 경로')
    parser.add_argument('--to-utf8', action='store_true', help='EUC-KR -> UTF-8 변환 (기본값)')
    parser.add_argument('--to-euckr', action='store_true', help='UTF-8 -> EUC-KR 변환')
    parser.add_argument('--overwrite', action='store_true', help='출력 파일이 이미 존재해도 덮어쓰기')
    parser.add_argument('--dir', action='store_true', help='폴더 단위 일괄 변환 모드')
    parser.add_argument('--ext', default='por', help='변환할 확장자 (기본: por, 콤마 구분 가능)')
    parser.add_argument('--recursive', action='store_true', help='하위 폴더까지 재귀 탐색')
    parser.add_argument('--no-verify', action='store_true', help='특수문자 보존 검증 생략')

    args = args = parser.parse_args()

    # 변환 방향 결정
    if args.to_utf8 and args.to_euckr:
        print("오류: --to-utf8과 --to-euckr을 동시에 사용할 수 없습니다.")
        sys.exit(1)

    to_utf8 = not args.to_euckr  # 기본값은 EUC-KR -> UTF-8

    # 폴더 단위 일괄 변환 모드
    if args.dir:
        if not os.path.exists(args.input):
            print(f"오류: 입력 폴더를 찾을 수 없습니다: {args.input}")
            sys.exit(1)
        if not os.path.isdir(args.input):
            print(f"오류: 입력 경로가 폴더가 아닙니다: {args.input}")
            sys.exit(1)

        # 확장자 파싱
        extensions = [e.strip() for e in args.ext.split(',') if e.strip()]
        if not extensions:
            print("오류: 유효한 확장자가 없습니다.")
            sys.exit(1)

        success = convert_directory(
            src_dir=args.input,
            dst_dir=args.output,
            extensions=extensions,
            to_utf8=to_utf8,
            recursive=args.recursive,
            overwrite=args.overwrite,
            verify=not args.no_verify
        )
        sys.exit(0 if success else 1)

    # 단일 파일 변환 모드
    else:
        # 입력 파일 존재 확인
        if not os.path.exists(args.input):
            print(f"오류: 입력 파일을 찾을 수 없습니다: {args.input}")
            sys.exit(1)

        if os.path.isdir(args.input):
            print(f"오류: 입력 경로가 폴더입니다. 폴더 단위 변환은 --dir 옵션을 사용하세요.")
            sys.exit(1)

        # 출력 파일 존재 확인
        if os.path.exists(args.output) and not args.overwrite:
            print(f"오류: 출력 파일이 이미 존재합니다: {args.output}")
            print("  --overwrite 옵션을 사용하면 덮어쓸 수 있습니다.")
            sys.exit(1)

        # 변환 실행
        success, message = convert_file(
            input_path=args.input,
            output_path=args.output,
            to_utf8=to_utf8,
            verify=not args.no_verify
        )

        if not success:
            print(f"오류: {message}")
            sys.exit(1)

        # 성공 메시지
        direction = "EUC-KR -> UTF-8" if to_utf8 else "UTF-8 -> EUC-KR"
        print(f"변환 완료: {args.input} ({direction}) -> {args.output}")
        if not args.no_verify:
            print("특수문자(0x09, 0x0D, 0x0A) 보존 검증 통과")


if __name__ == '__main__':
    main()