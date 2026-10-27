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
GOG_ORIGINAL_SHA256 = "8BAB3D109BD4C08BF356D30DE1B746CDC3A89DEBF4C953763B1305FE0C8285BD"
GOG_LAA_SHA256 = "E9D06B14EB70BBC57135C1C63000E5EC7E16C277B8115184CABF29F733729C19"

EXACT_BUILDS = {
    ORIGINAL_SHA256: (0xF8, LAA_SHA256),
    GOG_ORIGINAL_SHA256: (0x40, GOG_LAA_SHA256),
}


def patch_image(original: bytes) -> bytes:
    build = EXACT_BUILDS.get(hashlib.sha256(original).hexdigest().upper())
    if build is None:
        raise ValueError("Unknown original CoJ.exe SHA-256; refusing executable mutation")
    expected_pe, expected_derived_hash = build
    pe = struct.unpack_from("<I", original, 0x3C)[0]
    if original[:2] != b"MZ" or pe != expected_pe or original[pe:pe + 4] != b"PE\0\0":
        raise ValueError("Exact CoJ PE header mismatch")
    machine = struct.unpack_from("<H", original, pe + 4)[0]
    at = pe + 22
    characteristics = struct.unpack_from("<H", original, at)[0]
    if machine != 0x14C or at != expected_pe + 22 or characteristics != 0x10E:
        raise ValueError("Exact CoJ x86 characteristics mismatch")
    patched = bytearray(original)
    struct.pack_into("<H", patched, at, characteristics | 0x20)
    result = bytes(patched)
    if hashlib.sha256(result).hexdigest().upper() != expected_derived_hash:
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
    print(f"Exact CoJ LAA image SHA-256: {hashlib.sha256(result).hexdigest().upper()}")


if __name__ == "__main__":
    main()
