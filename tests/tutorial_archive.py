"""Exercise the real archive builder; never install the derived archive."""
import hashlib
import inspect
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import zipfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import patch_coj_weapon_consumers as patch
from coj_tutorial_hints import PRESENTATION_METHOD, TIP_PRESENTATION_METHOD
from inspect_java_bytecode import parse_class


class TutorialArchiveTests(unittest.TestCase):
    def test_builder_exposes_explicit_resource_input(self):
        self.assertIn('tutorial_data_en', inspect.signature(patch.patch_archive).parameters)

    def originals(self):
        game = Path(os.environ.get('COJ_GAME_DIRECTORY',
            'C:/Program Files (x86)/Steam/steamapps/common/Call of Juarez'))
        code = game / 'code.cojvr-backup.pak'
        if not code.exists():
            code = game / 'code.pak'
        resources = game / 'DataEn.pak'
        if not code.exists() or not resources.exists():
            self.skipTest('original archives unavailable')
        self.assertEqual(hashlib.sha256(code.read_bytes()).hexdigest(), patch.PAK_SHA256)
        return code, resources

    def test_actual_builder_changes_only_two_tutorial_members(self):
        code, resources = self.originals()
        source_hash = hashlib.sha256(resources.read_bytes()).hexdigest()
        with tempfile.TemporaryDirectory() as temp:
            base, result = Path(temp)/'base.pak', Path(temp)/'tutorial.pak'
            patch.patch_archive(code, base, manual_reload=True)
            patch.patch_archive(code, result, manual_reload=True, tutorial_data_en=resources)
            with zipfile.ZipFile(base) as a, zipfile.ZipFile(result) as b:
                self.assertEqual(a.namelist(), b.namelist())
                self.assertIsNone(b.testzip())
                changed = [name for name in a.namelist() if a.read(name) != b.read(name)]
                self.assertEqual(set(changed), {'HintBase.class', 'MenuHint.class'})
                methods = parse_class(b.read('HintBase.class'))[1]
                self.assertTrue({PRESENTATION_METHOD,TIP_PRESENTATION_METHOD} <= {m.name for m in methods})
        self.assertEqual(hashlib.sha256(resources.read_bytes()).hexdigest(), source_hash)

    def test_unknown_or_ambiguous_resources_do_not_create_output(self):
        code, _ = self.originals()
        with tempfile.TemporaryDirectory() as temp:
            resource, output = Path(temp)/'resources.pak', Path(temp)/'output.pak'
            for duplicate in (False, True):
                with zipfile.ZipFile(resource, 'w') as archive:
                    archive.writestr('Data/SaloonTexts.scr', b'foreign resource')
                    archive.writestr('Data/TextsGenerated.scr', b'foreign tips')
                    if duplicate:
                        import warnings
                        with warnings.catch_warnings():
                            warnings.simplefilter('ignore', UserWarning)
                            archive.writestr('Data/SaloonTexts.scr', b'ambiguous')
                with self.assertRaisesRegex(ValueError, 'SHA-256|Duplicate'):
                    patch.patch_archive(code, output, tutorial_data_en=resource)
                self.assertFalse(output.exists())

    def test_staging_cli_works_from_a_foreign_directory_without_installing(self):
        code, resources=self.originals()
        script=Path(__file__).resolve().parents[1]/'tools/patch_coj_weapon_consumers.py'
        with tempfile.TemporaryDirectory() as temp:
            output=Path(temp)/'derived.pak'
            result=subprocess.run([sys.executable,'-B',str(script),str(code),str(output),
                '--manual-reload','--tutorial-data-en',str(resources)],cwd=temp,
                capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            with zipfile.ZipFile(output) as archive:
                self.assertIsNone(archive.testzip())
                methods=parse_class(archive.read('HintBase.class'))[1]
                self.assertTrue({PRESENTATION_METHOD,TIP_PRESENTATION_METHOD} <= {m.name for m in methods})


if __name__ == '__main__':
    unittest.main()
