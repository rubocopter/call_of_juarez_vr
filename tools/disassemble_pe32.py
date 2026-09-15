#!/usr/bin/env python3
"""Disassemble a PE32 RVA with Capstone for exact-build reverse engineering."""

from __future__ import annotations

import argparse
from pathlib import Path

from capstone import CS_ARCH_X86, CS_MODE_32, Cs

from inspect_pe_refs import Pe32


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("binary", type=Path)
    parser.add_argument("rva", type=lambda value: int(value, 0))
    parser.add_argument("--size", type=lambda value: int(value, 0), default=0x100)
    args = parser.parse_args()

    data = args.binary.read_bytes()
    pe = Pe32(data)
    file_offset = pe.rva_to_file(args.rva)
    code = data[file_offset : file_offset + args.size]
    engine = Cs(CS_ARCH_X86, CS_MODE_32)
    engine.detail = False
    for instruction in engine.disasm(code, pe.image_base + args.rva):
        print(
            f"0x{instruction.address:08X}  {instruction.bytes.hex(' '):<30} "
            f"{instruction.mnemonic:<8} {instruction.op_str}"
        )
        if instruction.mnemonic in ("ret", "retf"):
            break
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
