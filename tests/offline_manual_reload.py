"""Execute lab bytecode; scheduling/skins remain explicit native stand-ins."""
import hashlib
import importlib
import inspect
import json
from pathlib import Path
import sys
import subprocess
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).parent))
from manual_reload_native import ManualVM
from reload_wait_probe import ARCHIVE, original, VM
from inspect_java_bytecode import parse_class
import patch_coj_weapon_consumers as production

try:
    lab = importlib.import_module('coj_offline_manual_reload')
except ModuleNotFoundError:
    lab = None


class OfflineManualReloadTests(unittest.TestCase):
    def factory(self, weapon):
        self.assertIsNotNone(lab, 'Missing isolated offline manual reload factory')
        return lab.patch_offline_classes(original('ArmedPlayerBeing'), original('StateMashine'), weapon)

    def vm(self, weapon):
        vm = ManualVM()
        for name, data in zip(('ArmedPlayerBeing', 'StateMashine'), self.factory(weapon)):
            pool, methods = parse_class(data)
            vm.classes[name] = pool, {(m.name, m.descriptor): m for m in methods}
        vm.weapon['class'] = weapon
        # Capacity belongs to the modeled native transaction, not the patch.
        capacity = {'WeaponRifleWinchester': 12, 'WeaponRifleWinchesterScope': 12,
                    'WeaponPistolVolcanic': 8}[weapon]
        base_native = vm.native
        def native(owner, name, args):
            if name == 'WeaponReload':
                if vm.ammo >= capacity or vm.reserve <= 0: return False
                vm.ammo += 1; vm.reserve -= 1; return True
            if name == 'GetWeaponMaxAmmoToReload': return min(capacity - vm.ammo, vm.reserve)
            return base_native(owner, name, args)
        vm.native = native
        return vm

    def test_waiting_never_auto_loads_then_explicit_insert_transfers_one(self):
        for weapon in ('WeaponRifleWinchester', 'WeaponRifleWinchesterScope', 'WeaponPistolVolcanic'):
            with self.subTest(weapon=weapon):
                vm = self.vm(weapon); vm.wait()
                self.assertEqual(vm.player['cojvrReloadProbeStatus'], 2)
                for t in (1., 2., 3.):
                    vm.machine['m_fCurrentTime'] = t
                    self.assertTrue(vm.call('cojvrKeepManualReloadAlive')); vm.update(t)
                self.assertEqual((vm.ammo, vm.reserve, vm.events), (2, 10, 0))
                self.assertTrue(vm.call('cojvrInsertManualRound'))
                self.assertEqual((vm.ammo, vm.reserve, vm.events), (3, 9, 1))
                vm.run('ArmedPlayerBeing', 'cojvrCancelReloadProbe', '()V', vm.player)
                vm.update(3.4)
                self.assertEqual((vm.ammo, vm.reserve, vm.events), (3, 9, 1))
                self.assertIsNone(vm.machine['cojvrReloadProbeOwner'])

    def test_native_capacity_and_last_reserve_close_without_extra_round(self):
        for weapon, capacity in (('WeaponRifleWinchester', 12), ('WeaponRifleWinchesterScope', 12), ('WeaponPistolVolcanic', 8)):
            for ammo, reserve in ((capacity-1, 10), (2, 1), (2, 0)):
                with self.subTest(weapon=weapon, ammo=ammo, reserve=reserve):
                    vm = self.vm(weapon); vm.wait(); vm.ammo, vm.reserve = ammo, reserve
                    accepted = vm.call('cojvrInsertManualRound')
                    count = int(reserve > 0)
                    self.assertEqual(bool(accepted), bool(count))
                    self.assertEqual((vm.ammo, vm.reserve, vm.events), (ammo+count, reserve-count, count))
                    self.assertEqual(vm.machine['m_sCurrentState']['id'], 22)
                    self.assertFalse(vm.call('cojvrInsertManualRound'))
                    vm.update(1.)
                    self.assertEqual((vm.ammo, vm.reserve, vm.events), (ammo+count, reserve-count, count))

    def test_successive_rounds_fill_exact_native_capacity_then_cleanup_once(self):
        for weapon, capacity in (('WeaponRifleWinchester', 12), ('WeaponRifleWinchesterScope', 12), ('WeaponPistolVolcanic', 8)):
            vm = self.vm(weapon); vm.wait(); vm.reserve = 20
            for count in range(3, capacity+1):
                self.assertTrue(vm.call('cojvrInsertManualRound'))
                self.assertEqual((vm.ammo, vm.reserve, vm.events), (count, 22-count, count-2))
            self.assertEqual(vm.transitions.count(22), 1)
            vm.run('ArmedPlayerBeing', 'cojvrCancelReloadProbe', '()V', vm.player)
            self.assertEqual(vm.transitions.count(22), 1)
            vm.update(1.)
            self.assertIsNone(vm.player['cojvrReloadProbeWeapon'])
            self.assertEqual((vm.ammo, vm.reserve, vm.events), (capacity, 22-capacity, capacity-2))

    def test_watchdog_and_opening_cancel_keep_native_end_and_zero_transfer(self):
        for weapon in ('WeaponRifleWinchester', 'WeaponRifleWinchesterScope', 'WeaponPistolVolcanic'):
            vm = self.vm(weapon); vm.arm()
            vm.run('ArmedPlayerBeing', 'cojvrCancelReloadProbe', '()V', vm.player)
            self.assertEqual(vm.transitions.count(22), 1)
            vm.update(.4)
            self.assertEqual((vm.ammo, vm.reserve, vm.events), (2, 10, 0))
            vm = self.vm(weapon); vm.wait(); vm.update(3.); vm.update(3.4)
            self.assertIsNone(vm.machine['cojvrReloadProbeOwner'])
            self.assertEqual((vm.ammo, vm.reserve, vm.events), (2, 10, 0))

    def test_body_reentry_is_not_an_animation_only_operation(self):
        for weapon in ('WeaponRifleWinchester', 'WeaponRifleWinchesterScope', 'WeaponPistolVolcanic'):
            vm = self.vm(weapon); vm.wait()
            self.assertTrue(vm.call('cojvrInsertManualRound'))
            # Execute the shipped setter and patched completion. A naive replay
            # runs Clear/Set and invalidates desired21 even with zero transfer.
            vm.run('StateMashine', 'SetCurrentState', '(LBaseState;)V', vm.machine, [vm.states[21]])
            self.assertEqual(vm.player['desired_state'], 1)
            self.assertFalse(vm.call('cojvrInsertManualRound'))
            self.assertEqual((vm.ammo, vm.reserve, vm.events), (3, 9, 1))

    def test_native_owner_support_and_ammo_shape_changes_cannot_insert(self):
        for weapon in ('WeaponRifleWinchester', 'WeaponRifleWinchesterScope', 'WeaponPistolVolcanic'):
            for change in ('support', 'actual', 'desired', 'machine', 'dead', 'network', 'whole_clip', 'multi_ammo'):
                with self.subTest(weapon=weapon, change=change):
                    vm = self.vm(weapon); vm.wait()
                    if change == 'support': vm.other_weapon = {'class': 'foreign'}
                    if change in ('actual', 'desired'): vm.player[change] = {'class': 'replacement'}
                    if change == 'machine': vm.machine = dict(vm.other)
                    if change == 'dead': vm.player['dead'] = True
                    if change == 'network': vm.net_game = True
                    if change == 'whole_clip': vm.weapon['whole_clip'] = True
                    if change == 'multi_ammo': vm.weapon['ammo_types'] = 2
                    self.assertFalse(vm.call('cojvrInsertManualRound'))
                    self.assertEqual((vm.ammo, vm.reserve, vm.events), (2, 10, 0))

    def test_lab_exact_class_does_not_admit_other_families_or_subclasses(self):
        for selected in ('WeaponRifleWinchester', 'WeaponRifleWinchesterScope', 'WeaponPistolVolcanic'):
            for actual in ('WeaponPistolPeacemaker', 'WeaponRifleWinchester', 'WeaponRifleWinchesterScope',
                           'WeaponRifleRemington', 'WeaponRifleRemingtonSawOff',
                           'WeaponPistolDerringer', 'WeaponPistolLemant',
                           'WeaponPistolSchofield', 'DerivedWinchester'):
                if selected == actual:
                    continue
                vm = self.vm(selected); vm.weapon['class'] = actual; vm.wait()
                self.assertFalse(vm.call('cojvrInsertManualRound'))
                self.assertEqual((vm.ammo, vm.reserve, vm.events), (2, 10, 0))

    def test_lab_does_not_change_probe_only_admission(self):
        # Production manual now intentionally integrates these native cycles;
        # the separate wait-probe experiment must retain its earlier whitelist.
        player, machine = production.patch_manual_reload_classes(original('ArmedPlayerBeing'), original('StateMashine'))
        pool, methods = parse_class(player)
        self.assertIn(('cojvrInsertManualNativeCycleRound','()Z'),{(m.name,m.descriptor) for m in methods})
        self.assertEqual(hashlib.sha256(machine).hexdigest(), 'd9103cc2b9b10e7e81eeafb03b864d621be0358b5541184ecec0ade59d9c53d4')
        player,machine=production.patch_reload_probe_classes(original('ArmedPlayerBeing'),original('StateMashine'))
        for weapon in ('WeaponRifleWinchester', 'WeaponRifleWinchesterScope', 'WeaponPistolVolcanic'):
            vm = VM(); vm.weapon['class'] = weapon
            vm.arm();vm.update(.31);vm.update(.62)
            self.assertIsNone(vm.player.get('cojvrReloadProbeWeapon'))
            self.assertEqual((vm.ammo, vm.reserve, vm.events), (2, 10, 0))

    def test_unknown_profile_or_modified_native_classes_reject(self):
        self.assertIsNotNone(lab)
        p, m = original('ArmedPlayerBeing'), original('StateMashine')
        for weapon in ('WeaponRifleRemington', 'WeaponPistolDerringer', 'unknown'):
            with self.assertRaises(ValueError): lab.patch_offline_classes(p, m, weapon)
        for p1, m1 in ((p+b'x', m), (p, m+b'x')):
            with self.assertRaises(ValueError): lab.patch_offline_classes(p1, m1, 'WeaponPistolVolcanic')

    def test_lab_export_is_local_new_directory_and_never_installs_archive(self):
        self.assertIsNotNone(lab)
        source = next((p for p in (ARCHIVE, ARCHIVE.with_name('code.cojvr-backup.pak'))
                       if p.exists() and hashlib.sha256(p.read_bytes()).hexdigest() == production.PAK_SHA256), None)
        if source is None: self.skipTest('Exact original game archive unavailable')
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp) / 'lab'
            result = lab.export_lab(source, output, 'WeaponRifleWinchester')
            self.assertFalse(result['runtime_admitted'])
            self.assertFalse(result['animation_playback_validated'])
            self.assertFalse(result['support_hand_validated'])
            self.assertEqual({p.name for p in output.iterdir()},
                             {'ArmedPlayerBeing.class.bin', 'StateMashine.class.bin', 'manifest.json'})
            self.assertEqual(json.loads((output/'manifest.json').read_text()), result)
            with self.assertRaises(ValueError): lab.export_lab(source, output, 'WeaponRifleWinchester')
            with self.assertRaises(ValueError): lab.export_lab(source, source.parent/'lab-forbidden', 'WeaponRifleWinchester')
            bad = Path(temp)/'originals'/'bad.pak'; bad.parent.mkdir(); bad.write_bytes(b'unknown')
            with self.assertRaisesRegex(ValueError, 'SHA-256'):
                lab.export_lab(bad, Path(temp)/'rejected', 'WeaponPistolVolcanic')
            self.assertFalse((Path(temp)/'rejected').exists())

    def test_indexed_cycle_export_requires_explicit_optin_and_preserves_originals(self):
        self.assertIn('indexed_body_cycle',inspect.signature(lab.export_lab).parameters,
                      'Missing explicit non-installable indexed cycle export')
        source=ARCHIVE.with_name('code.cojvr-backup.pak')
        if not source.exists():source=ARCHIVE
        before=hashlib.sha256(source.read_bytes()).hexdigest()
        self.assertEqual(before,production.PAK_SHA256)
        with tempfile.TemporaryDirectory() as temp:
            for weapon in ('WeaponPistolLemant','WeaponPistolDerringer',
                           'WeaponRifleRemington','WeaponRifleRemingtonSawOff'):
                output=Path(temp)/weapon
                result=lab.export_lab(source,output,weapon,indexed_body_cycle=True)
                self.assertEqual({p.name for p in output.iterdir()},
                    {'ArmedPlayerBeing.class.bin','StateMashine.class.bin','manifest.json'})
                self.assertFalse(result['runtime_admitted'])
                self.assertFalse(result['animation_playback_validated'])
                self.assertTrue(result['indexed_body_cycle_implemented'])
                self.assertEqual(result['reload_mode'],'indexed')
                signatures={(m.name,m.descriptor) for m in parse_class((output/'ArmedPlayerBeing.class.bin').read_bytes())[1]}
                self.assertIn(('cojvrIndexedCycleSuppressCompletion','(II)Z'),signatures)
                self.assertNotIn(('cojvrInsertManualRound','()Z'),signatures)
                self.assertEqual(json.loads((output/'manifest.json').read_text()),result)
            for weapon,flags in [('WeaponPistolLemant',{}),
                                 ('WeaponRifleWinchester',dict(indexed_body_cycle=True)),
                                 ('WeaponPistolLemant',dict(indexed_body_cycle=True,cycle_animation=True))]:
                output=Path(temp)/'rejected'
                with self.assertRaises(ValueError):lab.export_lab(source,output,weapon,**flags)
                self.assertFalse(output.exists())
        self.assertEqual(hashlib.sha256(source.read_bytes()).hexdigest(),before)

    def test_cli_indexed_cycle_optin_exports_only_class_bins_and_manifest(self):
        source=ARCHIVE.with_name('code.cojvr-backup.pak')
        if not source.exists():source=ARCHIVE
        with tempfile.TemporaryDirectory() as temp:
            output=Path(temp)/'lab'
            base=[sys.executable,'-B',str(Path(__file__).resolve().parents[1]/'tools/coj_offline_manual_reload.py'),
                  str(source),str(output),'--weapon','WeaponPistolLemant']
            missing=subprocess.run(base,capture_output=True,text=True,timeout=30)
            self.assertNotEqual(missing.returncode,0);self.assertFalse(output.exists())
            result=subprocess.run(base+['--indexed-body-cycle'],capture_output=True,text=True,timeout=30)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            report=json.loads(result.stdout)
            self.assertTrue(report['indexed_body_cycle_implemented'])
            self.assertFalse(report['runtime_admitted'])
            self.assertEqual(len(list(output.iterdir())),3)


if __name__ == '__main__': unittest.main()
