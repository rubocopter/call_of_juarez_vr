#!/usr/bin/env python3
"""Locate printable ASCII/UTF-16LE strings and their file offsets in a binary."""

from __future__ import annotations

import argparse
import re
from pathlib import Path


def ascii_strings(data: bytes, minimum: int):
    pattern = rb"[\x20-\x7e]{" + str(minimum).encode("ascii") + rb",}"
    for match in re.finditer(pattern, data):
        yield match.start(), match.group().decode("ascii")


def utf16le_strings(data: bytes, minimum: int):
    pattern = rb"(?:[\x20-\x7e]\x00){" + str(minimum).encode("ascii") + rb",}"
    for match in re.finditer(pattern, data):
        yield match.start(), match.group().decode("utf-16le")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("binary", type=Path)
    parser.add_argument("patterns", nargs="*")
    parser.add_argument("--minimum", type=int, default=4)
    args = parser.parse_args()

    data = args.binary.read_bytes()
    wanted = [re.compile(pattern, re.IGNORECASE) for pattern in args.patterns]
    results = list(ascii_strings(data, args.minimum)) + list(utf16le_strings(data, args.minimum))
    for offset, value in sorted(results):
        if wanted and not any(pattern.search(value) for pattern in wanted):
            continue
        print(f"0x{offset:08X}\t{value}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
