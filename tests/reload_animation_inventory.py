"""Synthetic archive contracts; optional explicit original-asset integration."""
import contextlib
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
SPEC = importlib.util.find_spec('coj_reload_animation_inventory')
inventory = importlib.import_module('coj_reload_animation_inventory') if SPEC else None


def java_class(name, parent='java/lang/Object', fields=(), logic=None, extra=b''):
    """Tiny authored class fixture, with real constant pool and Code attributes."""
    pool = []
    def add(raw):
        pool.append(raw)
        return len(pool)
    def u2(n):
        return struct.pack('>H', n)
    def utf(s):
        raw = s.encode('ascii')
        return add(b'\x01' + u2(len(raw)) + raw)
    def cls(s):
        return add(b'\x07' + u2(utf(s)))
    this, sup = cls(name), cls(parent)
    code_name = utf('Code')
    ctor_name, void_desc = utf('<init>'), utf('()V')
    code = bytearray(extra)
    for field, desc, value in fields:
        nt = add(b'\x0c' + u2(utf(field)) + u2(utf(desc)))
        ref = add(b'\x09' + u2(this) + u2(nt))
        code.append(0x2a)
        if isinstance(value, str):
            const = add(b'\x08' + u2(utf(value)))
            code.extend(b'\x12' + bytes([const]))
        else:
            code.extend(b'\x10' + struct.pack('b', value))
        code.extend(b'\xb5' + u2(ref))
    code.append(0xb1)
    def method(n, d, body):
        attr = u2(4) + u2(1) + struct.pack('>I', len(body)) + body + u2(0) + u2(0)
        return u2(1) + u2(n) + u2(d) + u2(1) + u2(code_name) + struct.pack('>I', len(attr)) + attr
    methods = [method(ctor_name, void_desc, bytes(code))]
    if logic:
        n, d = utf('GetLogicParamsClass'), utf('()Ljava/lang/String;')
        const = add(b'\x08' + u2(utf(logic)))
        methods.append(method(n, d, b'\x12' + bytes([const]) + b'\xb0'))
    return (b'\xca\xfe\xba\xbe\x00\x00\x00\x30' + u2(len(pool)+1) + b''.join(pool)
            + u2(0x21) + u2(this) + u2(sup) + u2(0) + u2(0)
            + u2(len(methods)) + b''.join(methods) + u2(0))


@unittest.skipUnless(inventory, 'inventory implementation not yet present')
class InventoryTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.game = self.root / 'originals'
        self.game.mkdir()
        self.code = self.game / 'code.pak'
        self.data = self.game / 'Data0.pak'

    def write_zip(self, path, members):
        with zipfile.ZipFile(path, 'w') as z:
            for name, data in members.items():
                z.writestr(name, data)

    def fixture(self, animation=None, state=None, extras=None):
        prefix = 'Data/Player/Equipment/'
        self.state_member = prefix + 'WeaponRifleWinchester.scr'
        self.anim_member = prefix + 'Winchester/Animations/Winchester_Ray.scr'
        self.write_zip(self.code, {
            'WeaponRifleWinchester.class': java_class('WeaponRifleWinchester', logic='WpnLogicRifleWinchester'),
            'WpnLogicRifleWinchester.class': java_class('WpnLogicRifleWinchester', fields=[
                ('m_bReloadWholeClipAtOnce', 'Z', 0), ('m_eWeaponAnimType', 'I', 3),
                ('m_sStateTimesScript', 'Ljava/lang/String;', self.state_member)]),
            'WpnAmmoRifleWinchester.class': java_class('WpnAmmoRifleWinchester', fields=[('m_iCapacity', 'I', 12)]),
        })
        members = {
            self.state_member: state if state is not None else '!include("WeaponOperate.def")\nWpnStateTime(RELOAD_BEG, .33)\nWpnStateTime(RELOAD_LOOP, .35)\nWpnStateTime(RELOAD_END, .33)\n',
            prefix + 'WeaponOperate.def': '!WpnStateTime(iState, fTime)\n$RELOAD_BEG(i,20)\n',
            prefix + 'Winchester/Winchester.scr': '!include("Animations/Winchester_Animations.scr")',
            prefix + 'Winchester/Animations/Winchester_Animations.scr': '!include("Winchester_Ray.scr")',
            self.anim_member: animation if animation is not None else (
                '// SeqTrack("Winchester_ReloadBeg", "fake.anm",0,99,30,1,.5)\n'
                'SeqTrack("Winchester_ReloadBeg", "fixture_reload.anm",0,10,30,1,.5)\n'
                'SeqTrack("Winchester_ReloadLoop", "fixture_reload.anm",10,18,30,1,.5)\n'
                'SeqTrack("Winchester_ReloadEnd", "fixture_reload.anm",18,28,30,1,.5)\n'
                'SeqTrackAlias("Winchester_A_ReloadLoop", "Winchester_ReloadLoop")\n'),
        }
        members.update(extras or {})
        self.write_zip(self.data, members)
        return patch.dict(inventory.PINNED_SHA256, {
            'code': hashlib.sha256(self.code.read_bytes()).hexdigest(),
            'data0': hashlib.sha256(self.data.read_bytes()).hexdigest(),
        })

    def family(self, report):
        return next(f for f in report['families'] if f['family'] == 'Winchester')

    def test_tool_exists(self):
        self.assertIsNotNone(inventory)

    def test_provenance_counts_logical_lines_after_comments_and_blank_lines(self):
        text = '// fixture\r\r\n\r\r\n/* comment */\r\r\n\r\r\n  SeqTrack("Winchester_ReloadLoop","a.anm",0,2,30,1,.5)'
        with self.fixture(animation=text):
            f = self.family(inventory.build_inventory(self.code, self.data))
        self.assertEqual(f['phases']['base']['body']['animation']['resolved']['source']['line'], 5)

    def test_conditional_and_duplicate_native_stores_stay_unknown(self):
        for fields, extra in (([('m_bReloadWholeClipAtOnce', 'Z', 0)] * 2, b''),
                              ([('m_bReloadWholeClipAtOnce', 'Z', 0)], b'\x03\x99\x00\x03')):
            with self.subTest(extra=extra), self.fixture():
                self.write_zip(self.code, {'WpnLogicRifleWinchester.class': java_class('WpnLogicRifleWinchester', fields=fields, extra=extra)})
                with patch.dict(inventory.PINNED_SHA256, {'code': hashlib.sha256(self.code.read_bytes()).hexdigest()}):
                    f = self.family(inventory.build_inventory(self.code, self.data))
                self.assertEqual(f['logic']['m_bReloadWholeClipAtOnce']['status'], 'unknown')

    def test_second_call_on_same_line_cannot_hide_a_duplicate(self):
        text = 'SeqTrack("Winchester_ReloadLoop","a.anm",0,2,30,1,.5) SeqTrack("Winchester_ReloadLoop","b.anm",0,3,30,1,.5)'
        with self.fixture(animation=text):
            f = self.family(inventory.build_inventory(self.code, self.data))
        self.assertEqual(f['phases']['base']['body']['animation']['status'], 'unknown')

    def test_inventory_retains_distinct_authored_times_ranges_hashes_and_aliases(self):
        with self.fixture():
            report = inventory.build_inventory(self.code, self.data)
        f = self.family(report)
        self.assertEqual(f['accounting']['ammo'][0]['capacity'], 12)
        self.assertFalse(f['logic']['m_bReloadWholeClipAtOnce']['value'])
        body = f['phases']['base']['body']
        self.assertEqual(body['state_time']['definitions'][0]['literal'], '.35')
        self.assertEqual(body['animation']['definitions'][0]['parameters'], ['10', '18', '30', '1', '.5'])
        self.assertNotIn('duration_seconds', json.dumps(report))
        self.assertEqual(f['phases']['alternate']['body']['animation']['resolved']['clip'], 'fixture_reload.anm')
        source = next(s for s in report['sources'] if s['member'] == self.anim_member)
        with zipfile.ZipFile(self.data) as z:
            self.assertEqual(source['sha256'], hashlib.sha256(z.read(self.anim_member)).hexdigest())
        self.assertEqual(body['animation']['definitions'][0]['source']['member'], self.anim_member)
        self.assertEqual(f['logic']['m_sStateTimesScript']['source']['method'], '<init>()V')

    def test_commented_missing_and_duplicate_definitions_stay_unknown(self):
        animation = ('/* SeqTrack("Winchester_ReloadBeg", "fake.anm",0,2,30,1,.5) */\n'
                     'SeqTrack("Winchester_ReloadLoop", "a.anm",0,2,30,1,.5)\n'
                     'SeqTrack("Winchester_ReloadLoop", "b.anm",0,3,30,1,.5)\n')
        with self.fixture(animation=animation, state='WpnStateTime(RELOAD_LOOP,.2)\nWpnStateTime(RELOAD_LOOP,.3)'):
            f = self.family(inventory.build_inventory(self.code, self.data))
        self.assertEqual(f['phases']['base']['begin']['animation']['status'], 'unknown')
        self.assertEqual(f['phases']['base']['body']['animation']['status'], 'unknown')
        self.assertEqual(f['phases']['base']['body']['state_time']['status'], 'unknown')
        self.assertEqual(len(f['phases']['base']['body']['animation']['definitions']), 2)

    def test_malformed_relevant_calls_are_not_silently_ignored(self):
        for text in ('SeqTrack("Winchester_ReloadLoop","a.anm",0,2,0,1,.5)',
                     'SeqTrack("Winchester_ReloadLoop","a.anm",0,NaN,30,1,.5)',
                     'SeqTrackAlias("Winchester_ReloadLoop",unquoted)',
                     'SeqTrack("Winchester_ReloadLoop","a.anm",0,2,30,1,.5'):
            with self.subTest(text=text), self.fixture(animation=text):
                f = self.family(inventory.build_inventory(self.code, self.data))
                self.assertEqual(f['phases']['base']['body']['animation']['status'], 'unknown')
                self.assertTrue(f['animation_issues'])

    def test_include_escape_cycle_missing_and_depth_are_bounded(self):
        for include in ('../../../../../../outside.scr', 'C:/outside.scr', '//host/outside.scr',
                        'Winchester_Ray.scr', 'missing.scr'):
            with self.subTest(include=include), self.fixture(animation=f'!include("{include}")'):
                f = self.family(inventory.build_inventory(self.code, self.data))
                self.assertTrue(f['animation_issues'])
                self.assertEqual(f['phases']['base']['body']['animation']['status'], 'unknown')
        extras = {f'Data/Player/Equipment/Winchester/Animations/n{i}.scr': f'!include("n{i+1}.scr")' for i in range(40)}
        with self.fixture(animation='!include("n0.scr")', extras=extras):
            f = self.family(inventory.build_inventory(self.code, self.data))
            self.assertTrue(any('limit' in str(x) for x in f['animation_issues']))

    def test_relative_parent_include_is_archive_local_and_alias_cycle_unknown(self):
        text = '!include("../shared.scr")\nSeqTrackAlias("Winchester_ReloadLoop","loop")\nSeqTrackAlias("loop","Winchester_ReloadLoop")'
        with self.fixture(animation=text, extras={'Data/Player/Equipment/Winchester/shared.scr': '// fixture only'}):
            f = self.family(inventory.build_inventory(self.code, self.data))
        self.assertFalse(f['animation_issues'])
        self.assertEqual(f['phases']['base']['body']['animation']['status'], 'unknown')

    def test_archive_hashes_fail_closed_before_output(self):
        with self.fixture():
            self.data.write_bytes(self.data.read_bytes() + b'changed')
            out = self.root / 'output.json'
            with contextlib.redirect_stderr(io.StringIO()):
                result = inventory.main(['--code-pak', str(self.code), '--data0-pak', str(self.data), '--output', str(out)])
            self.assertNotEqual(result, 0)
            self.assertFalse(out.exists())

    def test_duplicate_zip_members_and_oversized_scripts_reject_or_remain_unknown(self):
        with self.fixture():
            with zipfile.ZipFile(self.data, 'a') as z:
                import warnings
                with warnings.catch_warnings():
                    warnings.simplefilter('ignore', UserWarning)
                    z.writestr(self.anim_member.upper(), 'duplicate')
            with patch.dict(inventory.PINNED_SHA256, {'data0': hashlib.sha256(self.data.read_bytes()).hexdigest()}):
                with self.assertRaisesRegex(ValueError, 'ambiguous|duplicate'):
                    inventory.build_inventory(self.code, self.data)
        with self.fixture(animation='x' * (1024 * 1024 + 1)):
            f = self.family(inventory.build_inventory(self.code, self.data))
            self.assertTrue(f['animation_issues'])

    def test_output_is_explicit_exclusive_and_outside_both_archive_parents(self):
        with self.fixture():
            args = ['--code-pak', str(self.code), '--data0-pak', str(self.data)]
            with contextlib.redirect_stderr(io.StringIO()):
                self.assertNotEqual(inventory.main(args + ['--output', str(self.game / 'sub' / 'out.json')]), 0)
            out = self.root / 'report.json'
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(inventory.main(args + ['--output', str(out)]), 0)
            self.assertEqual(json.loads(out.read_text())['schema_version'], 1)
            before = out.read_bytes()
            with contextlib.redirect_stderr(io.StringIO()):
                self.assertNotEqual(inventory.main(args + ['--output', str(out)]), 0)
            self.assertEqual(out.read_bytes(), before)
            self.assertFalse((self.game / 'sub').exists())

    def test_output_protects_the_second_archive_parent_when_separate(self):
        with self.fixture():
            other = self.root / 'second-game'
            other.mkdir()
            moved = other / 'Data0.pak'
            moved.write_bytes(self.data.read_bytes())
            with contextlib.redirect_stderr(io.StringIO()):
                self.assertNotEqual(inventory.main(['--code-pak', str(self.code), '--data0-pak', str(moved),
                                                    '--output', str(other / 'report.json')]), 0)
            self.assertFalse((other / 'report.json').exists())


class AvailabilityTests(unittest.TestCase):
    def test_implementation_is_available(self):
        self.assertIsNotNone(inventory, 'missing offline inventory implementation')


@unittest.skipUnless(inventory and os.environ.get('COJ_INVENTORY_CODE_PAK') and os.environ.get('COJ_INVENTORY_DATA0_PAK'),
                     'set COJ_INVENTORY_CODE_PAK and COJ_INVENTORY_DATA0_PAK to explicit original paths')
class OriginalIntegrationTests(unittest.TestCase):
    def test_exact_original_profiles(self):
        report = inventory.build_inventory(Path(os.environ['COJ_INVENTORY_CODE_PAK']), Path(os.environ['COJ_INVENTORY_DATA0_PAK']))
        fs = {f['family']: f for f in report['families']}
        w = fs['Winchester']['phases']['base']['body']
        self.assertEqual(w['state_time']['definitions'][0]['literal'], '0.35')
        self.assertEqual(w['animation']['definitions'][0]['parameters'][:3], ['10', '18', '30'])
        self.assertEqual(fs['Volcanic']['phases']['base']['begin']['animation']['definitions'][0]['parameters'][:2], ['0', '4'])
        self.assertEqual(fs['WinchesterScope']['phases']['base']['body']['animation']['resolved']['clip'], 'Ray_Winchester_Reload.anm')
        self.assertEqual(fs['Lemant']['accounting']['ammo'][1]['capacity'], 8)
        self.assertEqual(fs['Remington']['phases']['base']['begin']['animation']['status'], 'unknown')
        for f in fs.values():
            self.assertFalse(f['animation_issues'], f['family'])
            self.assertFalse(f['state_issues'], f['family'])
            self.assertEqual(f['logic']['m_bReloadWholeClipAtOnce']['status'], 'known', f['family'])


if __name__ == '__main__':
    unittest.main(verbosity=2)
