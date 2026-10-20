"""Shipped Java 1.4 verifier: load exact transformed class, initialize=false.

No native/game calls or archive staging. Artifacts stay under ignored work/.
"""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from patch_coj_weapon_consumers import Code, u2, u4
from coj_reload_wait_probe import ClassEdit
from coj_movie_subtitles import patch_movie_subtitles
from movie_subtitles_patch import GAME, original_class

EVIDENCE = ROOT / "work/offline-batch-20261020/subtitles"


def verifier_class():
    pool = []
    def add(raw):
        pool.append(raw); return len(pool)
    def utf(text):
        raw = text.encode("utf-8"); return add(b"\x01" + u2(len(raw)) + raw)
    def cls(name):
        return add(b"\x07" + u2(utf(name)))
    def method(owner, name, desc):
        nt = add(b"\x0c" + u2(utf(name)) + u2(utf(desc)))
        return add(b"\x0a" + u2(cls(owner)) + u2(nt))
    this, parent = cls("VerifyMovieSubtitles"), cls("java/lang/Object")
    main, desc, attr = utf("main"), utf("([Ljava/lang/String;)V"), utf("Code")
    name = add(b"\x08" + u2(utf("LawmanModule")))
    loader = method("java/lang/ClassLoader", "getSystemClassLoader", "()Ljava/lang/ClassLoader;")
    for_name = method("java/lang/Class", "forName", "(Ljava/lang/String;ZLjava/lang/ClassLoader;)Ljava/lang/Class;")
    methods = method("java/lang/Class", "getDeclaredMethods", "()[Ljava/lang/reflect/Method;")
    fields = method("java/lang/Class", "getDeclaredFields", "()[Ljava/lang/reflect/Field;")
    c = Code()
    c.ref(0x13, name); c.emit(0x03)  # initialize=false: never run module static initializers.
    c.ref(0xb8, loader); c.ref(0xb8, for_name); c.emit(0x59)
    c.ref(0xb6, methods); c.emit(0x57)
    c.ref(0xb6, fields); c.emit(0x57, 0xb1)
    code = c.finish()
    body = u2(3) + u2(1) + u4(len(code)) + code + u2(0) + u2(0)
    return (bytes.fromhex("cafebabe0000002e") + u2(len(pool)+1) + b"".join(pool) +
            u2(0x21) + u2(this) + u2(parent) + u2(0) + u2(0) + u2(1) +
            u2(9) + u2(main) + u2(desc) + u2(1) + u2(attr) + u4(len(body)) + body + u2(0))


class MovieSubtitleJvmTests(unittest.TestCase):
    def test_shipped_verifier_accepts_cache_and_rejects_invalid_target(self):
        java = GAME / "jre/bin/java.exe"
        if not java.is_file(): self.skipTest("Shipped JVM unavailable")
        original = original_class()  # A present but mismatched original is a failure.
        transformed = patch_movie_subtitles(original)
        EVIDENCE.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="jvm-", dir=EVIDENCE) as directory:
            root = Path(directory)
            good, bad = root / "good", root / "bad"
            good.mkdir(); bad.mkdir()
            (root / "VerifyMovieSubtitles.class").write_bytes(verifier_class())
            (good / "LawmanModule.class").write_bytes(transformed)
            # Invalid opcode in the actual transformed writer body proves that
            # loading/reflection verifies method bodies, not just declarations.
            edit = ClassEdit(transformed)
            edit.changes["OnShowSubtitle", "(Ljava/lang/String;I)V"] = \
                lambda code: code[:681] + b"\xfe" + code[682:]
            (bad / "LawmanModule.class").write_bytes(edit.finish())
            for label, folder in (("good", good), ("bad", bad)):
                result = subprocess.run([str(java), "-Xverify:all", "-cp",
                    str(folder) + ";" + str(root) + ";" + str(GAME / "code.pak"),
                    "VerifyMovieSubtitles"], capture_output=True, text=True, timeout=30)
                (EVIDENCE / ("jvm-" + label + ".log")).write_text(
                    "exit_code=" + str(result.returncode) + "\n" + result.stdout + result.stderr,
                    encoding="utf-8")
                if label == "good": self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                else:
                    self.assertNotEqual(result.returncode, 0, "Invalid writer bypassed JVM verification")
                    self.assertIn("VerifyError", result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)
