#!/usr/bin/env python3
"""Check hand-written source size and C line-width rules."""

from __future__ import annotations

import argparse
import pathlib
import sys
import unicodedata

EXCLUDED_PARTS = {".git", "build", "third_party"}
CODE_SUFFIXES = {".c", ".h", ".cmake", ".py"}
C_SUFFIXES = {".c", ".h"}


def display_width(text: str) -> int:
    width = 0
    for character in text:
        if unicodedata.combining(character):
            continue
        width += 2 if unicodedata.east_asian_width(character) in "WFA" else 1
    return width


def source_files(root: pathlib.Path) -> list[pathlib.Path]:
    files: list[pathlib.Path] = []
    for path in root.rglob("*"):
        if not path.is_file() or path.suffix not in CODE_SUFFIXES:
            continue
        relative_parts = path.relative_to(root).parts
        if any(part in EXCLUDED_PARTS for part in relative_parts):
            continue
        files.append(path)
    cmake = root / "CMakeLists.txt"
    if cmake.exists():
        files.append(cmake)
    return sorted(set(files))


def check_file(root: pathlib.Path, path: pathlib.Path) -> tuple[list[str], list[str]]:
    errors: list[str] = []
    warnings: list[str] = []
    relative = path.relative_to(root).as_posix()
    lines = path.read_text(encoding="utf-8").splitlines()

    if len(lines) > 300:
        warnings.append(f"{relative}: {len(lines)} physical lines; review single responsibility")
    if len(lines) > 500:
        errors.append(f"{relative}: {len(lines)} physical lines exceeds 500")

    for number, line in enumerate(lines, start=1):
        if line.rstrip() != line:
            errors.append(f"{relative}:{number}: trailing whitespace")
        if path.suffix in C_SUFFIXES and "\t" in line:
            errors.append(f"{relative}:{number}: tab indentation is not allowed in C source")
        if path.suffix in C_SUFFIXES:
            width = display_width(line)
            if width > 100:
                errors.append(f"{relative}:{number}: {width} columns exceeds 100")
            elif width > 80:
                warnings.append(f"{relative}:{number}: {width} columns exceeds the 80-column target")
    return errors, warnings


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=pathlib.Path, default=pathlib.Path.cwd())
    args = parser.parse_args()
    root = args.root.resolve()
    errors: list[str] = []
    warnings: list[str] = []
    files = source_files(root)
    for path in files:
        file_errors, file_warnings = check_file(root, path)
        errors.extend(file_errors)
        warnings.extend(file_warnings)

    for warning in warnings:
        print(f"WARNING: {warning}")
    for error in errors:
        print(f"ERROR: {error}")
    print(f"Checked {len(files)} source files")
    if errors:
        return 1
    print("Code quality checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
