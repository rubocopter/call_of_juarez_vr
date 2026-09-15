#!/usr/bin/env python3
"""Recover Chrome Engine 3 Java-native registration records from the exact x86 DLL.

The inspected Call of Juarez build emits registration call sites as four consecutive
`push imm32` instructions: Java signature, method name, class name, native function.
This tool turns that compiler-specific evidence into stable RVAs for research.
"""

from __future__ import annotations

import argparse
import struct
from pathlib import Path

from inspect_pe_refs import Pe32


def read_c_string(data: bytes, offset: int) -> str | None:
    if offset < 0 or offset >= len(data):
        return None
    end = data.find(b"\0", offset)
    if end < 0 or end - offset > 512:
        return None
    raw = data[offset:end]
    try:
        text = raw.decode("ascii")
    except UnicodeDecodeError:
        return None
    if not text or any(ord(char) < 0x20 or ord(char) > 0x7E for char in text):
        return None
    return text


def va_to_file(pe: Pe32, va: int) -> int | None:
    if va < pe.image_base:
        return None
    try:
        return pe.rva_to_file(va - pe.image_base)
    except ValueError:
        return None


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("binary", type=Path)
    parser.add_argument("--class", dest="class_name")
    args = parser.parse_args()

    data = args.binary.read_bytes()
    pe = Pe32(data)
    text = next(section for section in pe.sections if section.name == ".text")
    start = text.raw_offset
    end = text.raw_offset + text.raw_size - 25
    records: list[tuple[int, str, str, str, int]] = []

    for offset in range(start, end):
        if not (
            data[offset] == 0x68
            and data[offset + 5] == 0x68
            and data[offset + 10] == 0x68
            and data[offset + 15] == 0x68
        ):
            continue
        signature_va = struct.unpack_from("<I", data, offset + 1)[0]
        name_va = struct.unpack_from("<I", data, offset + 6)[0]
        class_va = struct.unpack_from("<I", data, offset + 11)[0]
        function_va = struct.unpack_from("<I", data, offset + 16)[0]
        signature_file = va_to_file(pe, signature_va)
        name_file = va_to_file(pe, name_va)
        class_file = va_to_file(pe, class_va)
        function_file = va_to_file(pe, function_va)
        if None in (signature_file, name_file, class_file, function_file):
            continue
        signature = read_c_string(data, signature_file)
        name = read_c_string(data, name_file)
        class_name = read_c_string(data, class_file)
        if not signature or not name or not class_name or not signature.startswith("("):
            continue
        if args.class_name and class_name != args.class_name:
            continue
        function_rva = function_va - pe.image_base
        callsite_rva = pe.file_to_rva(offset)
        records.append((callsite_rva, class_name, name, signature, function_rva))

    for callsite_rva, class_name, name, signature, function_rva in records:
        print(
            f"{class_name}.{name}{signature}\t"
            f"native_rva=0x{function_rva:08X}\tregister_rva=0x{callsite_rva:08X}"
        )
    print(f"RECORDS {len(records)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
