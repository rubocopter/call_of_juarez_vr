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
from patch_coj_weapon_consumers import patch_archive, u2, u4, Code
from coj_reload_wait_probe import ClassEdit
from coj_native_settings import MENU_CLASS_NAME
from native_settings_patch import original_archives, GRAPHICS, GAME

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
    c = Code()
    c.ref(0x13, name)
    c.emit(0x03)  # Class.forName initialize=false: no game static initializer/native call.
    c.ref(0xb8, loader)
    c.ref(0xb8, for_name)
    c.emit(0x4c, 0x2b)  # retain verified Class for exact bridge reflection
    c.ref(0xb6, methods)
    c.emit(0x57)
    for method_name, count in (("cojvrSyncSettings", 4), ("cojvrSyncGraphics", 11),
                               ("cojvrHolsterGuidesValue", 0), *(
                                   ("cojvr" + row[1] + "Value", 0) for row in GRAPHICS)):
        c.emit(0x2b); c.ref(0x13, add(b'\x08' + u2(utf(method_name))))
        c.emit(0x10, count); c.ref(0xbd, class_type)
        for index in range(count):
            c.emit(0x59, 0x10, index); c.ref(0xb2, integer_type); c.emit(0x53)
        c.ref(0xb6, get_method); c.emit(0x57)
    c.emit(0x2b); c.ref(0x13, add(b'\x08' + u2(utf('cojvrNeuralStatus'))))
    c.emit(0x04); c.ref(0xbd, class_type)
    c.emit(0x59, 0x03); c.ref(0x13, add(b'\x08' + u2(utf('java.lang.String'))))
    c.emit(0x03); c.ref(0xb8, loader); c.ref(0xb8, for_name); c.emit(0x53)
    c.ref(0xb6, get_method); c.emit(0x57)
    get_field = method('java/lang/Class', 'getField', '(Ljava/lang/String;)Ljava/lang/reflect/Field;')
    c.emit(0x2b); c.ref(0x13, add(b'\x08' + u2(utf('cojvrGraphicsReady'))))
    c.ref(0xb6, get_field); c.emit(0x57)
    # Verify pause/save consumers too, without invoking game initialization.
    for target in ('LawmanModuleSingle','MenuBackgroundModule','MenuSaveLoad','GameUILoading','FXEmitter','FireEmitterCore'):
        c.ref(0x13, add(b'\x08' + u2(utf(target)))); c.emit(0x03); c.ref(0xb8, loader)
        c.ref(0xb8, for_name); c.ref(0xb6, methods); c.emit(0x57)
    c.emit(0xb1)
    code = c.finish()
    body = u2(8) + u2(2) + u4(len(code)) + code + u2(0) + u2(0)
    return (bytes.fromhex('cafebabe0000002e') + u2(len(pool)+1) + b''.join(pool) +
            u2(0x21) + u2(this) + u2(parent) + u2(0) + u2(0) + u2(1) +
            u2(9) + u2(main) + u2(desc) + u2(1) + u2(attr) + u4(len(body)) + body + u2(0))

class JvmTests(unittest.TestCase):
    def test_shipped_jvm_verifies_menu_and_rejects_invalid_helper(self):
        java = GAME / 'jre/bin/java.exe'
        if not java.is_file():
            self.skipTest('Shipped JVM unavailable')
        # Read only exact originals/cache/backups, never load deployed derivatives.
        original, _ = original_archives()
        with tempfile.TemporaryDirectory(prefix='cojvr-menu-jvm-') as directory:
            root = Path(directory)
            (root / 'VerifyNativeOptions.class').write_bytes(verifier_class())
            good = root / 'good.pak'
            patch_archive(original, good, manual_reload=True)
            def verify(archive):
                return subprocess.run([str(java), '-Xverify:all', '-cp',
                    str(root) + ';' + str(archive), 'VerifyNativeOptions'],
                    capture_output=True, text=True, timeout=30)
            result = verify(good)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            # Prove that the JVM verified old AND new helper bodies, not metadata.
            for target, name, desc in (
                    (MENU_CLASS_NAME,'cojvrSettingsRow', '(LUIOptionsControlSelect;' + 'Ljava/lang/String;'*4 + 'I)V'),
                    (MENU_CLASS_NAME,'cojvrNeuralStatus', '(Ljava/lang/String;)V'),
                    (MENU_CLASS_NAME,'cojvrSyncGraphics', '(IIIIIIIIIII)V'),
                    (MENU_CLASS_NAME,'cojvrSharpnessRow', '(LUIOptionsControlSelect;Ljava/lang/String;Ljava/lang/String;I)V'),
                    ('LawmanModuleSingle.class','TakeScreenshotsOnLevelBreak','()V'),
                    ('MenuBackgroundModule.class','ShowScreenInGame','()V'),
                    ('MenuSaveLoad.class','SaveScreenshot','(Ljava/lang/String;)V'),
                    ('GameUILoading.class','cojvrContinueLoading','()Z'),
                    ('FXEmitter.class','Start','()V'),('FXEmitter.class','DestroyFXDefs','()V'),
                    ('FireEmitterCore.class','Start','()V'),('FireEmitterCore.class','StartSteam','()V'),
                    ('FireEmitterCore.class','DestroyFXDefs','()V')):
                with self.subTest(invalid_helper=name):
                    bad = root / (target + '-' + name + '.pak')
                    with zipfile.ZipFile(good) as source, zipfile.ZipFile(bad, 'x') as destination:
                        edit = ClassEdit(source.read(target))
                        edit.changes[name, desc] = lambda code: b'\xfe' + code[1:]
                        invalid = edit.finish()
                        for info in source.infolist():
                            destination.writestr(info, invalid if info.filename == target else source.read(info))
                    result = verify(bad)
                    self.assertNotEqual(result.returncode, 0, 'Invalid helper bypassed JVM verification')
                    self.assertIn('VerifyError', result.stdout + result.stderr)

if __name__ == '__main__':
    unittest.main(verbosity=2)
