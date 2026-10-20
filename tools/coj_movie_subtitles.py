"""Exact CoJ 2006 movie-subtitle writer cache; pure class transformation.

Only hash-verified original LawmanModule bytes are accepted. Archive identity,
deployment and game-thread reader admission belong to the caller. No original
assets are embedded and no native methods, timing, sprites or item state change.

Show and accepted Hide extend the existing final return. Lifecycle clears use a
16-byte entry prefix, preserving all original relative branch bytes and switch
alignment; their LineNumberTable start_pc values shift by 16. This clears the old
cache before callbacks, preserving a first line shown reentrantly during start.
OnHideSubtitle's wrong-ID return stays unchanged. A null key is invalid
even though a freshly constructed instance has the JVM's default integer zero.
"""
from __future__ import annotations

import hashlib

from coj_reload_wait_probe import ClassEdit
from inspect_java_bytecode import Reader
from patch_coj_weapon_consumers import Code, u2, u4

MODULE_CLASS_NAME = "LawmanModule.class"
MODULE_CLASS_SHA256 = "80bdbbe228fa06eb8a3424f1dfc72ee42c42e9eefecdae0beaf41acd235330aa"
OWNER = "LawmanModule"
BRIDGE_FIELDS = (("cojvrMovieSubtitleKey", "Ljava/lang/String;"),
                 ("cojvrMovieSubtitleId", "I"),
                 ("cojvrMovieSubtitleMovie", "LMovie;"))
# final return offset, original max_stack, original max_locals, access
_TARGETS = {("OnShowSubtitle", "(Ljava/lang/String;I)V"): (681, 8, 6, 1),
            ("OnHideSubtitle", "(I)V"): (41, 2, 3, 1),
            ("MovieDestroySubtitles", "()V"): (38, 2, 1, 4),
            ("OnStartMovie", "(LMovie;)V"): (45, 1, 3, 1),
            ("OnEndMovie", "(LMovie;)V"): (54, 2, 3, 1)}
_LIFECYCLE = {"MovieDestroySubtitles", "OnStartMovie", "OnEndMovie"}


def patch_movie_subtitles(data: bytes) -> bytes:
    """Append three public transient fields; augment exact native writer bodies.

    Refuse modified/already-patched input. No file or archive is written here.
    """
    if hashlib.sha256(data).hexdigest() != MODULE_CLASS_SHA256:
        raise ValueError("Unknown or already patched LawmanModule.class SHA-256; no mutation")
    edit = ClassEdit(data)
    if data[:8] != bytes.fromhex("cafebabe0000002e") or edit.pool.describe(edit.this) != OWNER:
        raise ValueError("Unexpected LawmanModule class shape")
    fields = {name: edit.field(name, desc) for name, desc in BRIDGE_FIELDS}
    current_movie = edit.ref(OWNER, "m_cMoviePlaying", "LMovie;", 9)
    show = Code()
    show.emit(0x2a, 0x2b); show.ref(0xb5, fields["cojvrMovieSubtitleKey"])
    show.emit(0x2a, 0x1c); show.ref(0xb5, fields["cojvrMovieSubtitleId"])
    show.emit(0x2a, 0x2a); show.ref(0xb4, current_movie)
    show.ref(0xb5, fields["cojvrMovieSubtitleMovie"]); show.emit(0xb1)
    clear = Code()
    for name, opcode in (("cojvrMovieSubtitleKey", 0x01),
                         ("cojvrMovieSubtitleId", 0x02),
                         ("cojvrMovieSubtitleMovie", 0x01)):
        clear.emit(0x2a, opcode); clear.ref(0xb5, fields[name])
    clear.emit(0xb1)
    show_suffix, clear_suffix = show.finish(), clear.finish()
    lifecycle_prefix = clear_suffix[:-1] + b"\x00"  # 15 bytes + NOP; 4-byte alignment
    if len(lifecycle_prefix) != 16:
        raise ValueError("Unexpected lifecycle prefix size")

    # ClassEdit safely appends the constant-pool entries/field declarations.
    # Its normal finish rebuilds target Code metadata, so assemble members here
    # instead: preserve Code attributes, shifting only lifecycle line start PCs.
    r = edit.r
    count = r.u2(); methods = bytearray(u2(count)); found = set()
    for _ in range(count):
        header = r.read(6); methods.extend(header)
        name = edit.pool.utf8(int.from_bytes(header[2:4], "big"))
        desc = edit.pool.utf8(int.from_bytes(header[4:6], "big"))
        target = (name, desc)
        attribute_count = r.u2(); methods.extend(u2(attribute_count))
        for _ in range(attribute_count):
            index = r.u2(); payload = r.read(r.u4())
            if target in _TARGETS and edit.pool.utf8(index) == "Code":
                if target in found:
                    raise ValueError("Duplicate movie subtitle target Code")
                final, expected_stack, expected_locals, access = _TARGETS[target]
                cr = Reader(payload)
                stack, locals_ = cr.u2(), cr.u2(); code = cr.read(cr.u4())
                tail_start = cr.stream.tell()
                if (len(code) != final + 1 or code[-1:] != b"\xb1" or
                        (stack, locals_, int.from_bytes(header[:2], "big")) !=
                        (expected_stack, expected_locals, access)):
                    raise ValueError("Unexpected movie subtitle target shape: " + name)
                if cr.u2():
                    raise ValueError("Unexpected movie subtitle exception table")
                nested_count = cr.u2()
                relocated_tail = bytearray(u2(0) + u2(nested_count))
                for _ in range(nested_count):
                    nested_index = cr.u2()
                    nested_name = edit.pool.utf8(nested_index); nested = cr.read(cr.u4())
                    if nested_name != "LineNumberTable" or len(nested) < 2 or \
                            len(nested) != 2 + int.from_bytes(nested[:2], "big") * 4:
                        raise ValueError("Unexpected movie subtitle Code metadata")
                    if name in _LIFECYCLE:
                        shifted = bytearray(nested[:2])
                        for offset in range(2, len(nested), 4):
                            pc = int.from_bytes(nested[offset:offset+2], "big")
                            if pc >= len(code):
                                raise ValueError("Invalid movie lifecycle line offset")
                            shifted.extend(u2(pc + len(lifecycle_prefix)) + nested[offset+2:offset+4])
                        nested = bytes(shifted)
                    relocated_tail.extend(u2(nested_index) + u4(len(nested)) + nested)
                if cr.stream.tell() != len(payload):
                    raise ValueError("Trailing movie subtitle Code bytes")
                if name in _LIFECYCLE:
                    rewritten = lifecycle_prefix + code
                    tail = bytes(relocated_tail)
                else:
                    suffix = show_suffix if name == "OnShowSubtitle" else clear_suffix
                    rewritten = code[:-1] + suffix
                    tail = payload[tail_start:]
                payload = (u2(max(stack, 2)) + u2(locals_) + u4(len(rewritten)) +
                           rewritten + tail)
                found.add(target)
            methods.extend(u2(index) + u4(len(payload)) + payload)
    if found != set(_TARGETS):
        raise ValueError("Incomplete movie subtitle target set")
    if edit.next > 65535:
        raise ValueError("Movie subtitle constant pool overflow")
    return (data[:8] + u2(edit.next) + data[10:edit.cp_end] + edit.extra +
            data[edit.cp_end:edit.fields_at] + u2(edit.fields_count + len(BRIDGE_FIELDS)) +
            data[edit.fields_at+2:edit.fields_end] + edit.fields + methods + data[r.stream.tell():])
