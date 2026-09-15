#!/usr/bin/env python3
"""Find PE32 absolute references to a file offset or ASCII string.

Useful for locating old x86 registration tables where entries contain absolute
image pointers to method names/signatures/functions.
"""

from __future__ import annotations

import argparse
import struct
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class Section:
    name: str
    virtual_address: int
    virtual_size: int
    raw_offset: int
    raw_size: int


class Pe32:
    def __init__(self, data: bytes) -> None:
        self.data = data
        pe_offset = struct.unpack_from("<I", data, 0x3C)[0]
        if data[pe_offset : pe_offset + 4] != b"PE\0\0":
            raise ValueError("not a PE image")
        file_header = pe_offset + 4
        section_count = struct.unpack_from("<H", data, file_header + 2)[0]
        optional_size = struct.unpack_from("<H", data, file_header + 16)[0]
        optional = file_header + 20
        if struct.unpack_from("<H", data, optional)[0] != 0x10B:
            raise ValueError("only PE32 images are supported")
        self.image_base = struct.unpack_from("<I", data, optional + 28)[0]
        section_table = optional + optional_size
        sections: list[Section] = []
        for index in range(section_count):
            offset = section_table + index * 40
            name = data[offset : offset + 8].split(b"\0", 1)[0].decode("ascii", "replace")
            virtual_size, virtual_address, raw_size, raw_offset = struct.unpack_from(
                "<IIII", data, offset + 8
            )
            sections.append(Section(name, virtual_address, virtual_size, raw_offset, raw_size))
        self.sections = tuple(sections)

    def file_to_rva(self, file_offset: int) -> int:
        for section in self.sections:
            if section.raw_offset <= file_offset < section.raw_offset + section.raw_size:
                return section.virtual_address + (file_offset - section.raw_offset)
        raise ValueError(f"file offset 0x{file_offset:X} is not inside a section")

    def rva_to_file(self, rva: int) -> int:
        for section in self.sections:
            span = max(section.virtual_size, section.raw_size)
            if section.virtual_address <= rva < section.virtual_address + span:
                return section.raw_offset + (rva - section.virtual_address)
        raise ValueError(f"RVA 0x{rva:X} is not inside a section")

    def describe_offset(self, file_offset: int) -> str:
        rva = self.file_to_rva(file_offset)
        section = next(
            section
            for section in self.sections
            if section.raw_offset <= file_offset < section.raw_offset + section.raw_size
        )
        return f"file=0x{file_offset:08X} rva=0x{rva:08X} va=0x{self.image_base + rva:08X} section={section.name}"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("binary", type=Path)
    target = parser.add_mutually_exclusive_group(required=True)
    target.add_argument("--string")
    target.add_argument("--file-offset", type=lambda value: int(value, 0))
    parser.add_argument("--context-dwords", type=int, default=4)
    args = parser.parse_args()

    data = args.binary.read_bytes()
    pe = Pe32(data)
    if args.string is not None:
        needle = args.string.encode("ascii") + b"\0"
        target_file = data.find(needle)
        if target_file < 0:
            raise SystemExit(f"string not found: {args.string!r}")
    else:
        target_file = args.file_offset

    target_rva = pe.file_to_rva(target_file)
    target_va = pe.image_base + target_rva
    print(f"IMAGE_BASE 0x{pe.image_base:08X}")
    print(f"TARGET {pe.describe_offset(target_file)}")
    encoded = struct.pack("<I", target_va)
    start = 0
    found = 0
    while True:
        reference = data.find(encoded, start)
        if reference < 0:
            break
        start = reference + 1
        try:
            print(f"REF {pe.describe_offset(reference)}")
        except ValueError:
            continue
        first = max(0, reference - args.context_dwords * 4)
        last = min(len(data), reference + (args.context_dwords + 1) * 4)
        for offset in range(first - (first % 4), last, 4):
            value = struct.unpack_from("<I", data, offset)[0]
            marker = "=>" if offset == reference else "  "
            print(f"  {marker} file=0x{offset:08X} dword=0x{value:08X}")
        found += 1
    print(f"REFERENCES {found}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
