#!/usr/bin/env python3
"""Inspect Java class declarations stored directly or inside a ZIP/PAK archive.

This intentionally implements only the class-file structures needed for reverse-
engineering API surfaces: constant-pool UTF-8 values, fields and method signatures.
It has no third-party dependencies and does not execute class bytecode.
"""

from __future__ import annotations

import argparse
import io
import struct
import zipfile
from dataclasses import dataclass
from pathlib import Path


ACC_STATIC = 0x0008
ACC_NATIVE = 0x0100
ACC_ABSTRACT = 0x0400


class ClassFormatError(ValueError):
    pass


class Reader:
    def __init__(self, data: bytes) -> None:
        self._stream = io.BytesIO(data)

    def u1(self) -> int:
        return struct.unpack(">B", self._read(1))[0]

    def u2(self) -> int:
        return struct.unpack(">H", self._read(2))[0]

    def u4(self) -> int:
        return struct.unpack(">I", self._read(4))[0]

    def skip(self, count: int) -> None:
        self._read(count)

    def _read(self, count: int) -> bytes:
        value = self._stream.read(count)
        if len(value) != count:
            raise ClassFormatError("unexpected end of class file")
        return value


@dataclass(frozen=True)
class Member:
    access: int
    name: str
    descriptor: str

    @property
    def flags(self) -> str:
        flags: list[str] = []
        if self.access & ACC_STATIC:
            flags.append("static")
        if self.access & ACC_NATIVE:
            flags.append("native")
        if self.access & ACC_ABSTRACT:
            flags.append("abstract")
        return ",".join(flags) or "-"


@dataclass(frozen=True)
class ClassInfo:
    major: int
    minor: int
    utf8: tuple[str, ...]
    fields: tuple[Member, ...]
    methods: tuple[Member, ...]


def _parse_constant_pool(reader: Reader) -> list[object | None]:
    count = reader.u2()
    pool: list[object | None] = [None] * count
    index = 1
    while index < count:
        tag = reader.u1()
        if tag == 1:  # CONSTANT_Utf8
            length = reader.u2()
            pool[index] = reader._read(length).decode("utf-8", errors="replace")
        elif tag in (3, 4):  # Integer, Float
            reader.skip(4)
        elif tag in (5, 6):  # Long, Double (two entries)
            reader.skip(8)
            index += 1
        elif tag in (7, 8, 16, 19, 20):
            reader.skip(2)
        elif tag in (9, 10, 11, 12, 17, 18):
            reader.skip(4)
        elif tag == 15:
            reader.skip(3)
        else:
            raise ClassFormatError(f"unsupported constant-pool tag {tag} at index {index}")
        index += 1
    return pool


def _utf8(pool: list[object | None], index: int) -> str:
    try:
        value = pool[index]
    except IndexError as exc:
        raise ClassFormatError(f"constant-pool index {index} out of range") from exc
    if not isinstance(value, str):
        raise ClassFormatError(f"constant-pool index {index} is not UTF-8")
    return value


def _skip_attributes(reader: Reader) -> None:
    for _ in range(reader.u2()):
        reader.skip(2)  # attribute_name_index
        reader.skip(reader.u4())


def _parse_members(reader: Reader, pool: list[object | None]) -> tuple[Member, ...]:
    members: list[Member] = []
    for _ in range(reader.u2()):
        access = reader.u2()
        name = _utf8(pool, reader.u2())
        descriptor = _utf8(pool, reader.u2())
        _skip_attributes(reader)
        members.append(Member(access, name, descriptor))
    return tuple(members)


def parse_class(data: bytes) -> ClassInfo:
    reader = Reader(data)
    if reader.u4() != 0xCAFEBABE:
        raise ClassFormatError("not a Java class file")
    minor = reader.u2()
    major = reader.u2()
    pool = _parse_constant_pool(reader)
    reader.skip(6)  # access_flags, this_class, super_class
    reader.skip(reader.u2() * 2)  # interfaces
    fields = _parse_members(reader, pool)
    methods = _parse_members(reader, pool)
    return ClassInfo(
        major=major,
        minor=minor,
        utf8=tuple(value for value in pool if isinstance(value, str)),
        fields=fields,
        methods=methods,
    )


def load_class(source: Path, class_name: str | None) -> tuple[str, bytes]:
    if source.suffix.lower() in (".pak", ".zip"):
        if not class_name:
            raise ValueError("--class is required for a PAK/ZIP source")
        member = class_name if class_name.endswith(".class") else f"{class_name}.class"
        with zipfile.ZipFile(source) as archive:
            return member, archive.read(member)
    return source.name, source.read_bytes()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("--class", dest="classes", action="append", default=[])
    parser.add_argument("--strings", action="store_true", help="also print UTF-8 constants")
    args = parser.parse_args()

    classes = args.classes or [None]
    for class_name in classes:
        label, data = load_class(args.source, class_name)
        info = parse_class(data)
        print(f"CLASS {label} version={info.major}.{info.minor}")
        for field in info.fields:
            print(f"FIELD 0x{field.access:04X} [{field.flags}] {field.name} {field.descriptor}")
        for method in info.methods:
            print(f"METHOD 0x{method.access:04X} [{method.flags}] {method.name} {method.descriptor}")
        if args.strings:
            for value in info.utf8:
                print(f"UTF8 {value}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
