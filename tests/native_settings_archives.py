"""Exercise the real archive transformations; no game install is modified."""
import hashlib
from pathlib import Path
import sys
import tempfile
import unittest
import zipfile
sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from inspect_java_class import parse_class
import patch_coj_weapon_consumers as consumers
import split_coj_player_mesh as resources

GAME = Path('C:/Program Files (x86)/Steam/steamapps/common/Call of Juarez')

class ArchiveTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        code, data = GAME / 'code.pak', GAME / 'Data0.pak'
        if not code.is_file() or not data.is_file():
            raise unittest.SkipTest('Exact original game archives unavailable')
        if hashlib.sha256(code.read_bytes()).hexdigest() != consumers.PAK_SHA256:
            raise unittest.SkipTest('Installed code archive is not the original')
        cls.code = code
        with zipfile.ZipFile(data) as pak:
            cls.script = pak.read('Data/Menu/scr/OptionsGame.scr')

    def test_native_stereo_code_patch_also_installs_options_contract(self):
        with tempfile.TemporaryDirectory(prefix='cojvr-menu-code-') as directory:
            output = Path(directory) / 'code.pak'
            consumers.patch_archive(self.code, output)
            with zipfile.ZipFile(output) as pak:
                menu = parse_class(pak.read('MenuOptionsGame.class'))
                self.assertIn(('cojvrHolsterGuidesValue', '()I'), {(m.name,m.descriptor) for m in menu.methods})
                self.assertIn(('cojvrSyncSettings', '(IIII)V'), {(m.name,m.descriptor) for m in menu.methods})
                self.assertIn(('cojvrSettingsReady', 'Z'),
                              {(f.name, f.descriptor) for f in menu.fields})
                movie = parse_class(pak.read('LawmanModule.class'))
                self.assertTrue({('cojvrMovieSubtitleKey','Ljava/lang/String;'),
                                 ('cojvrMovieSubtitleId','I'),('cojvrMovieSubtitleMovie','LMovie;')}
                                <= {(f.name,f.descriptor) for f in movie.fields})

    def test_resource_only_mode_retains_unrelated_compressed_records(self):
        with tempfile.TemporaryDirectory(prefix='cojvr-menu-resources-') as directory:
            source, output = (Path(directory) / name for name in ('input.pak', 'output.pak'))
            with zipfile.ZipFile(source, 'x', compression=zipfile.ZIP_DEFLATED) as pak:
                pak.comment = b'original archive metadata'
                pak.writestr('untouched.bin', bytes(range(256)) * 4)
                pak.writestr('Data/Menu/scr/OptionsGame.scr', self.script)
            resources.patch_archive(source, output, independent_hands=False, native_settings=True)
            with zipfile.ZipFile(source) as original, zipfile.ZipFile(output) as patched:
                self.assertEqual(original.namelist(), patched.namelist())
                self.assertEqual(original.comment, patched.comment)
                a, b = original.getinfo('untouched.bin'), patched.getinfo('untouched.bin')
                self.assertEqual((a.CRC, a.header_offset, a.compress_size),
                                 (b.CRC, b.header_offset, b.compress_size))
                self.assertEqual(original.read('untouched.bin'), patched.read('untouched.bin'))
                self.assertIn(b'ID_VR_HOLSTER_GUIDES', patched.read('Data/Menu/scr/OptionsGame.scr'))
                self.assertNotIn('Data/Player/Ray/Ray.msh', patched.namelist())

    def test_unknown_resource_is_rejected_before_output(self):
        with tempfile.TemporaryDirectory(prefix='cojvr-menu-reject-') as directory:
            source, output = (Path(directory) / name for name in ('input.pak', 'output.pak'))
            with zipfile.ZipFile(source, 'x') as pak:
                pak.writestr('Data/Menu/scr/OptionsGame.scr', b'foreign script')
            with self.assertRaises(ValueError):
                resources.patch_archive(source, output, independent_hands=False, native_settings=True)
            self.assertFalse(output.exists())

if __name__ == '__main__':
    unittest.main(verbosity=2)
