"""Verify transformed menu on the shipped JVM without initializing game classes."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import zipfile
sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from patch_coj_weapon_consumers import patch_archive, u2, u4, Code, PAK_SHA256
from coj_reload_wait_probe import ClassEdit
from coj_native_settings import MENU_CLASS_NAME
import hashlib

GAME = Path('C:/Program Files (x86)/Steam/steamapps/common/Call of Juarez')

def verifier_class():
    pool = []
    def add(value):
        pool.append(value)
        return len(pool)
    def utf(text):
        raw = text.encode()
        return add(b'\x01' + u2(len(raw)) + raw)
    def cls(name):
        return add(b'\x07' + u2(utf(name)))
    def method(owner, name, desc):
        nt = add(b'\x0c' + u2(utf(name)) + u2(utf(desc)))
        return add(b'\x0a' + u2(cls(owner)) + u2(nt))
    this, parent = cls('VerifyNativeOptions'), cls('java/lang/Object')
    main, desc, attr = utf('main'), utf('([Ljava/lang/String;)V'), utf('Code')
    name = add(b'\x08' + u2(utf('MenuOptionsGame')))
    loader = method('java/lang/ClassLoader', 'getSystemClassLoader', '()Ljava/lang/ClassLoader;')
    for_name = method('java/lang/Class', 'forName', '(Ljava/lang/String;ZLjava/lang/ClassLoader;)Ljava/lang/Class;')
    methods = method('java/lang/Class', 'getDeclaredMethods', '()[Ljava/lang/reflect/Method;')
    get_method = method('java/lang/Class', 'getMethod', '(Ljava/lang/String;[Ljava/lang/Class;)Ljava/lang/reflect/Method;')
    class_type = cls('java/lang/Class')
    nt = add(b'\x0c' + u2(utf('TYPE')) + u2(utf('Ljava/lang/Class;')))
    integer_type = add(b'\x09' + u2(cls('java/lang/Integer')) + u2(nt))
    sync_name = add(b'\x08' + u2(utf('cojvrSyncSettings')))
    guides_name = add(b'\x08' + u2(utf('cojvrHolsterGuidesValue')))
    c = Code()
    c.ref(0x13, name)
    c.emit(0x03)  # Class.forName initialize=false: no game static initializer/native call.
    c.ref(0xb8, loader)
    c.ref(0xb8, for_name)
    c.emit(0x4c, 0x2b)  # retain verified Class for exact bridge reflection
    c.ref(0xb6, methods)
    c.emit(0x57, 0x2b); c.ref(0x13, sync_name)
    c.emit(0x07); c.ref(0xbd, class_type)  # four int parameters
    for index in range(4):
        c.emit(0x59, 0x03 + index); c.ref(0xb2, integer_type); c.emit(0x53)
    c.ref(0xb6, get_method); c.emit(0x57, 0x2b); c.ref(0x13, guides_name)
    c.emit(0x03); c.ref(0xbd, class_type)
    c.ref(0xb6, get_method); c.emit(0x57, 0xb1)
    code = c.finish()
    body = u2(6) + u2(2) + u4(len(code)) + code + u2(0) + u2(0)
    return (bytes.fromhex('cafebabe0000002e') + u2(len(pool)+1) + b''.join(pool) +
            u2(0x21) + u2(this) + u2(parent) + u2(0) + u2(0) + u2(1) +
            u2(9) + u2(main) + u2(desc) + u2(1) + u2(attr) + u4(len(body)) + body + u2(0))

class JvmTests(unittest.TestCase):
    def test_shipped_jvm_verifies_menu_and_rejects_invalid_helper(self):
        java, original = GAME / 'jre/bin/java.exe', GAME / 'code.pak'
        if not java.is_file() or not original.is_file():
            self.skipTest('Shipped JVM/original code archive unavailable')
        if hashlib.sha256(original.read_bytes()).hexdigest() != PAK_SHA256:
            self.skipTest('Installed code archive is not the exact original')
        with tempfile.TemporaryDirectory(prefix='cojvr-menu-jvm-') as directory:
            root = Path(directory)
            (root / 'VerifyNativeOptions.class').write_bytes(verifier_class())
            good, bad = root / 'good.pak', root / 'bad.pak'
            patch_archive(original, good, manual_reload=True)
            def verify(archive):
                return subprocess.run([str(java), '-Xverify:all', '-cp',
                    str(root) + ';' + str(archive), 'VerifyNativeOptions'],
                    capture_output=True, text=True, timeout=30)
            result = verify(good)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            # Prove that the JVM verified helper bodies, not just class metadata.
            with zipfile.ZipFile(good) as source, zipfile.ZipFile(bad, 'x') as destination:
                edit = ClassEdit(source.read(MENU_CLASS_NAME))
                edit.changes['cojvrSettingsRow', '(LUIOptionsControlSelect;' + 'Ljava/lang/String;'*4 + 'I)V'] = \
                    lambda code: b'\xfe' + code[1:]
                invalid = edit.finish()
                for info in source.infolist():
                    destination.writestr(info, invalid if info.filename == MENU_CLASS_NAME else source.read(info))
            result = verify(bad)
            self.assertNotEqual(result.returncode, 0, 'Invalid helper bypassed JVM verification')
            self.assertIn('VerifyError', result.stdout + result.stderr)

if __name__ == '__main__':
    unittest.main(verbosity=2)
