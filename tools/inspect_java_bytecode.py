#!/usr/bin/env python3
"""Disassemble Java bytecode from a .class file or a class inside code.pak.

This is intentionally dependency-free.  It is aimed at the old Java class
files embedded in Chrome Engine games and prints symbolic constant-pool
references so camera call sites can be correlated with native registration
records without executing game code.
"""

from __future__ import annotations

import argparse
import io
import re
import struct
import zipfile
from dataclasses import dataclass
from pathlib import Path


class ClassFormatError(ValueError):
    pass


class Reader:
    def __init__(self, data: bytes) -> None:
        self.stream = io.BytesIO(data)

    def u1(self) -> int:
        return struct.unpack(">B", self.read(1))[0]

    def u2(self) -> int:
        return struct.unpack(">H", self.read(2))[0]

    def u4(self) -> int:
        return struct.unpack(">I", self.read(4))[0]

    def read(self, count: int) -> bytes:
        data = self.stream.read(count)
        if len(data) != count:
            raise ClassFormatError("unexpected end of class file")
        return data


@dataclass(frozen=True)
class Method:
    access: int
    name: str
    descriptor: str
    code: bytes | None
    max_stack: int = 0
    max_locals: int = 0


class ConstantPool:
    def __init__(self, entries: list[object | None]) -> None:
        self.entries = entries

    def get(self, index: int) -> object:
        if index <= 0 or index >= len(self.entries):
            raise ClassFormatError(f"constant-pool index {index} out of range")
        entry = self.entries[index]
        if entry is None:
            raise ClassFormatError(f"constant-pool index {index} is empty")
        return entry

    def utf8(self, index: int) -> str:
        entry = self.get(index)
        if not (isinstance(entry, tuple) and entry[0] == "Utf8"):
            raise ClassFormatError(f"constant-pool index {index} is not UTF-8")
        return entry[1]

    def describe(self, index: int) -> str:
        entry = self.get(index)
        if not isinstance(entry, tuple):
            return repr(entry)
        tag = entry[0]
        if tag == "Utf8":
            return repr(entry[1])
        if tag == "Class":
            return self.utf8(entry[1])
        if tag == "String":
            return repr(self.utf8(entry[1]))
        if tag in ("Integer", "Float", "Long", "Double"):
            return repr(entry[1])
        if tag == "NameAndType":
            return f"{self.utf8(entry[1])}{self.utf8(entry[2])}"
        if tag in ("Fieldref", "Methodref", "InterfaceMethodref"):
            owner = self.describe(entry[1])
            name_type = self.describe(entry[2])
            return f"{owner}.{name_type}"
        if tag == "MethodType":
            return self.utf8(entry[1])
        if tag == "MethodHandle":
            return f"kind={entry[1]} {self.describe(entry[2])}"
        return repr(entry)


def parse_constant_pool(reader: Reader) -> ConstantPool:
    count = reader.u2()
    entries: list[object | None] = [None] * count
    index = 1
    while index < count:
        tag = reader.u1()
        if tag == 1:
            length = reader.u2()
            entries[index] = ("Utf8", reader.read(length).decode("utf-8", errors="replace"))
        elif tag == 3:
            entries[index] = ("Integer", struct.unpack(">i", reader.read(4))[0])
        elif tag == 4:
            entries[index] = ("Float", struct.unpack(">f", reader.read(4))[0])
        elif tag == 5:
            entries[index] = ("Long", struct.unpack(">q", reader.read(8))[0])
            index += 1
        elif tag == 6:
            entries[index] = ("Double", struct.unpack(">d", reader.read(8))[0])
            index += 1
        elif tag == 7:
            entries[index] = ("Class", reader.u2())
        elif tag == 8:
            entries[index] = ("String", reader.u2())
        elif tag == 9:
            entries[index] = ("Fieldref", reader.u2(), reader.u2())
        elif tag == 10:
            entries[index] = ("Methodref", reader.u2(), reader.u2())
        elif tag == 11:
            entries[index] = ("InterfaceMethodref", reader.u2(), reader.u2())
        elif tag == 12:
            entries[index] = ("NameAndType", reader.u2(), reader.u2())
        elif tag == 15:
            entries[index] = ("MethodHandle", reader.u1(), reader.u2())
        elif tag == 16:
            entries[index] = ("MethodType", reader.u2())
        elif tag in (17, 18):
            entries[index] = ("Dynamic" if tag == 17 else "InvokeDynamic", reader.u2(), reader.u2())
        elif tag in (19, 20):
            entries[index] = ("Module" if tag == 19 else "Package", reader.u2())
        else:
            raise ClassFormatError(f"unsupported constant-pool tag {tag} at index {index}")
        index += 1
    return ConstantPool(entries)


def skip_attributes(reader: Reader) -> None:
    for _ in range(reader.u2()):
        reader.u2()
        reader.read(reader.u4())


def parse_methods(reader: Reader, pool: ConstantPool) -> list[Method]:
    methods: list[Method] = []
    for _ in range(reader.u2()):
        access = reader.u2()
        name = pool.utf8(reader.u2())
        descriptor = pool.utf8(reader.u2())
        code: bytes | None = None
        max_stack = 0
        max_locals = 0
        attribute_count = reader.u2()
        for _ in range(attribute_count):
            attribute_name = pool.utf8(reader.u2())
            attribute_length = reader.u4()
            attribute_data = reader.read(attribute_length)
            if attribute_name != "Code":
                continue
            code_reader = Reader(attribute_data)
            max_stack = code_reader.u2()
            max_locals = code_reader.u2()
            code = code_reader.read(code_reader.u4())
        methods.append(Method(access, name, descriptor, code, max_stack, max_locals))
    return methods


def parse_class(data: bytes) -> tuple[ConstantPool, list[Method]]:
    reader = Reader(data)
    if reader.u4() != 0xCAFEBABE:
        raise ClassFormatError("not a Java class file")
    reader.u2()  # minor
    reader.u2()  # major
    pool = parse_constant_pool(reader)
    reader.read(6)  # access, this, super
    reader.read(reader.u2() * 2)  # interfaces
    for _ in range(reader.u2()):
        reader.read(6)  # access, name, descriptor
        skip_attributes(reader)
    return pool, parse_methods(reader, pool)


MNEMONICS = {
    0x00:"nop",0x01:"aconst_null",0x02:"iconst_m1",0x03:"iconst_0",0x04:"iconst_1",0x05:"iconst_2",0x06:"iconst_3",0x07:"iconst_4",0x08:"iconst_5",
    0x09:"lconst_0",0x0A:"lconst_1",0x0B:"fconst_0",0x0C:"fconst_1",0x0D:"fconst_2",0x0E:"dconst_0",0x0F:"dconst_1",
    0x10:"bipush",0x11:"sipush",0x12:"ldc",0x13:"ldc_w",0x14:"ldc2_w",
    0x15:"iload",0x16:"lload",0x17:"fload",0x18:"dload",0x19:"aload",
    0x1A:"iload_0",0x1B:"iload_1",0x1C:"iload_2",0x1D:"iload_3",0x1E:"lload_0",0x1F:"lload_1",0x20:"lload_2",0x21:"lload_3",
    0x22:"fload_0",0x23:"fload_1",0x24:"fload_2",0x25:"fload_3",0x26:"dload_0",0x27:"dload_1",0x28:"dload_2",0x29:"dload_3",
    0x2A:"aload_0",0x2B:"aload_1",0x2C:"aload_2",0x2D:"aload_3",0x2E:"iaload",0x2F:"laload",0x30:"faload",0x31:"daload",0x32:"aaload",0x33:"baload",0x34:"caload",0x35:"saload",
    0x36:"istore",0x37:"lstore",0x38:"fstore",0x39:"dstore",0x3A:"astore",
    0x3B:"istore_0",0x3C:"istore_1",0x3D:"istore_2",0x3E:"istore_3",0x3F:"lstore_0",0x40:"lstore_1",0x41:"lstore_2",0x42:"lstore_3",
    0x43:"fstore_0",0x44:"fstore_1",0x45:"fstore_2",0x46:"fstore_3",0x47:"dstore_0",0x48:"dstore_1",0x49:"dstore_2",0x4A:"dstore_3",
    0x4B:"astore_0",0x4C:"astore_1",0x4D:"astore_2",0x4E:"astore_3",0x4F:"iastore",0x50:"lastore",0x51:"fastore",0x52:"dastore",0x53:"aastore",0x54:"bastore",0x55:"castore",0x56:"sastore",
    0x57:"pop",0x58:"pop2",0x59:"dup",0x5A:"dup_x1",0x5B:"dup_x2",0x5C:"dup2",0x5D:"dup2_x1",0x5E:"dup2_x2",0x5F:"swap",
    0x60:"iadd",0x61:"ladd",0x62:"fadd",0x63:"dadd",0x64:"isub",0x65:"lsub",0x66:"fsub",0x67:"dsub",0x68:"imul",0x69:"lmul",0x6A:"fmul",0x6B:"dmul",0x6C:"idiv",0x6D:"ldiv",0x6E:"fdiv",0x6F:"ddiv",
    0x70:"irem",0x71:"lrem",0x72:"frem",0x73:"drem",0x74:"ineg",0x75:"lneg",0x76:"fneg",0x77:"dneg",0x78:"ishl",0x79:"lshl",0x7A:"ishr",0x7B:"lshr",0x7C:"iushr",0x7D:"lushr",0x7E:"iand",0x7F:"land",0x80:"ior",0x81:"lor",0x82:"ixor",0x83:"lxor",0x84:"iinc",
    0x85:"i2l",0x86:"i2f",0x87:"i2d",0x88:"l2i",0x89:"l2f",0x8A:"l2d",0x8B:"f2i",0x8C:"f2l",0x8D:"f2d",0x8E:"d2i",0x8F:"d2l",0x90:"d2f",0x91:"i2b",0x92:"i2c",0x93:"i2s",
    0x94:"lcmp",0x95:"fcmpl",0x96:"fcmpg",0x97:"dcmpl",0x98:"dcmpg",
    0x99:"ifeq",0x9A:"ifne",0x9B:"iflt",0x9C:"ifge",0x9D:"ifgt",0x9E:"ifle",0x9F:"if_icmpeq",0xA0:"if_icmpne",0xA1:"if_icmplt",0xA2:"if_icmpge",0xA3:"if_icmpgt",0xA4:"if_icmple",0xA5:"if_acmpeq",0xA6:"if_acmpne",0xA7:"goto",0xA8:"jsr",0xA9:"ret",0xAA:"tableswitch",0xAB:"lookupswitch",
    0xAC:"ireturn",0xAD:"lreturn",0xAE:"freturn",0xAF:"dreturn",0xB0:"areturn",0xB1:"return",
    0xB2:"getstatic",0xB3:"putstatic",0xB4:"getfield",0xB5:"putfield",0xB6:"invokevirtual",0xB7:"invokespecial",0xB8:"invokestatic",0xB9:"invokeinterface",0xBA:"invokedynamic",
    0xBB:"new",0xBC:"newarray",0xBD:"anewarray",0xBE:"arraylength",0xBF:"athrow",0xC0:"checkcast",0xC1:"instanceof",0xC2:"monitorenter",0xC3:"monitorexit",0xC4:"wide",0xC5:"multianewarray",0xC6:"ifnull",0xC7:"ifnonnull",0xC8:"goto_w",0xC9:"jsr_w",
}


LOCAL_U1 = set(range(0x15, 0x1A)) | set(range(0x36, 0x3B)) | {0xA9}
CP_U1 = {0x12}
CP_U2 = {0x13,0x14,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xBB,0xBD,0xC0,0xC1}
BRANCH_S2 = set(range(0x99, 0xA9)) | {0xC6, 0xC7}


def s2(data: bytes, offset: int) -> int:
    return struct.unpack_from(">h", data, offset)[0]


def s4(data: bytes, offset: int) -> int:
    return struct.unpack_from(">i", data, offset)[0]


def disassemble(code: bytes, pool: ConstantPool) -> list[str]:
    lines: list[str] = []
    pc = 0
    while pc < len(code):
        start = pc
        opcode = code[pc]
        pc += 1
        mnemonic = MNEMONICS.get(opcode, f"op_{opcode:02x}")
        operand = ""
        if opcode == 0x10:
            operand = str(struct.unpack_from(">b", code, pc)[0]); pc += 1
        elif opcode == 0x11:
            operand = str(s2(code, pc)); pc += 2
        elif opcode in CP_U1:
            index = code[pc]; pc += 1
            operand = f"#{index} // {pool.describe(index)}"
        elif opcode in CP_U2:
            index = struct.unpack_from(">H", code, pc)[0]; pc += 2
            operand = f"#{index} // {pool.describe(index)}"
        elif opcode in LOCAL_U1:
            operand = str(code[pc]); pc += 1
        elif opcode == 0x84:
            index = code[pc]; const = struct.unpack_from(">b", code, pc + 1)[0]; pc += 2
            operand = f"{index}, {const}"
        elif opcode in BRANCH_S2:
            delta = s2(code, pc); pc += 2
            operand = str(start + delta)
        elif opcode == 0xB9:
            index = struct.unpack_from(">H", code, pc)[0]; count = code[pc + 2]; pc += 4
            operand = f"#{index}, {count} // {pool.describe(index)}"
        elif opcode == 0xBA:
            index = struct.unpack_from(">H", code, pc)[0]; pc += 4
            operand = f"#{index} // {pool.describe(index)}"
        elif opcode == 0xBC:
            operand = str(code[pc]); pc += 1
        elif opcode == 0xC5:
            index = struct.unpack_from(">H", code, pc)[0]; dims = code[pc + 2]; pc += 3
            operand = f"#{index}, {dims} // {pool.describe(index)}"
        elif opcode in (0xC8, 0xC9):
            delta = s4(code, pc); pc += 4
            operand = str(start + delta)
        elif opcode == 0xC4:
            wide_opcode = code[pc]; pc += 1
            wide_name = MNEMONICS.get(wide_opcode, f"op_{wide_opcode:02x}")
            index = struct.unpack_from(">H", code, pc)[0]; pc += 2
            if wide_opcode == 0x84:
                const = s2(code, pc); pc += 2
                operand = f"{wide_name} {index}, {const}"
            else:
                operand = f"{wide_name} {index}"
        elif opcode in (0xAA, 0xAB):
            while pc % 4:
                pc += 1
            default = s4(code, pc); pc += 4
            parts = [f"default:{start + default}"]
            if opcode == 0xAA:
                low = s4(code, pc); high = s4(code, pc + 4); pc += 8
                for key in range(low, high + 1):
                    delta = s4(code, pc); pc += 4
                    parts.append(f"{key}:{start + delta}")
            else:
                pairs = s4(code, pc); pc += 4
                for _ in range(pairs):
                    key = s4(code, pc); delta = s4(code, pc + 4); pc += 8
                    parts.append(f"{key}:{start + delta}")
            operand = " ".join(parts)
        raw = code[start:pc].hex(" ")
        lines.append(f"{start:04d}: {raw:<28} {mnemonic:<18} {operand}".rstrip())
    return lines


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
    parser.add_argument("--class", dest="class_name")
    parser.add_argument("--method", help="regex matched against method name or name+descriptor")
    args = parser.parse_args()

    label, data = load_class(args.source, args.class_name)
    pool, methods = parse_class(data)
    wanted = re.compile(args.method) if args.method else None
    for method in methods:
        signature = method.name + method.descriptor
        if wanted and not (wanted.search(method.name) or wanted.search(signature)):
            continue
        print(f"METHOD {label} {signature} access=0x{method.access:04X}")
        if method.code is None:
            print("  <native/abstract/no code>")
            continue
        print(f"  max_stack={method.max_stack} max_locals={method.max_locals} code_length={len(method.code)}")
        for line in disassemble(method.code, pool):
            print("  " + line)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
