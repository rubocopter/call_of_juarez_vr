"""Set only LAA on the one recognized original CoJ x86 executable.

Deployment owns backup, publication and restoration. This helper produces a
separate verified image and cannot overwrite its source or an existing output.
"""
import argparse
import hashlib
from pathlib import Path
import struct

ORIGINAL_SHA256 = "5EC9215E1BBDA4BE0662BEE4DF696DF35577196792CD76570DFF49F18BF109EE"
LAA_SHA256 = "C8B8BB82FCB3D6599C5F77B1BB9CB3444CBB3A360461DAD43AD808B49AD28DC9"


def patch_image(original: bytes) -> bytes:
    if hashlib.sha256(original).hexdigest().upper() != ORIGINAL_SHA256:
        raise ValueError("Unknown original CoJ.exe SHA-256; refusing executable mutation")
    pe = struct.unpack_from("<I", original, 0x3C)[0]
    if original[:2] != b"MZ" or pe != 0xF8 or original[pe:pe + 4] != b"PE\0\0":
        raise ValueError("Exact CoJ PE header mismatch")
    machine = struct.unpack_from("<H", original, pe + 4)[0]
    at = pe + 22
    characteristics = struct.unpack_from("<H", original, at)[0]
    if machine != 0x14C or at != 0x10E or characteristics != 0x10E:
        raise ValueError("Exact CoJ x86 characteristics mismatch")
    patched = bytearray(original)
    struct.pack_into("<H", patched, at, characteristics | 0x20)
    result = bytes(patched)
    if hashlib.sha256(result).hexdigest().upper() != LAA_SHA256:
        raise ValueError("Exact LAA output SHA-256 mismatch")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    result = patch_image(args.source.read_bytes())
    with args.output.open("xb") as output:
        output.write(result)
    print(f"Exact CoJ LAA image SHA-256: {LAA_SHA256}")


if __name__ == "__main__":
    main()
