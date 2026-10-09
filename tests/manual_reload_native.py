"""Run emitted exact-game helpers; native scheduling/transfer remain stand-ins."""
import sys
import hashlib
import unittest
import tempfile
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from reload_wait_probe import VM, original, patch
from inspect_java_bytecode import parse_class, disassemble


class ManualVM(VM):
    def __init__(self):
        super().__init__()
        if hasattr(patch, 'patch_manual_reload_classes'):
            armed, machine = patch.patch_manual_reload_classes(original('ArmedPlayerBeing'), original('StateMashine'))
            for name, data in [('ArmedPlayerBeing', armed), ('StateMashine', machine)]:
                pool, methods = parse_class(data)
                self.classes[name] = pool, {(m.name, m.descriptor): m for m in methods}

    def native(self, owner, name, args):
        if name == 'GetAmmoTypeCount': return 1
        if name == 'IsCarrying': return self.player.get('carrying', False)
        return super().native(owner, name, args)

    def call(self, name):
        return self.run('ArmedPlayerBeing', name, '()Z', self.player)

    def wait(self):
        self.arm(); self.update(.31); self.update(.62)


class ManualReloadNativeTests(unittest.TestCase):
    def test_manual_helpers_exist_on_opt_in_derivative(self):
        vm = ManualVM()
        self.assertTrue(('cojvrKeepManualReloadAlive', '()Z') in vm.classes['ArmedPlayerBeing'][1])
        self.assertTrue(('cojvrInsertManualRound', '()Z') in vm.classes['ArmedPlayerBeing'][1])

    def test_heartbeat_keeps_body_and_without_it_watchdog_closes_zero(self):
        vm = ManualVM(); vm.wait()
        for t in (1., 2., 3., 4., 5.):
            vm.machine['m_fCurrentTime'] = t
            self.assertTrue(vm.call('cojvrKeepManualReloadAlive'))
            vm.update(t)
            self.assertEqual(vm.machine['m_sCurrentState']['id'], 21)
        self.assertEqual((vm.ammo, vm.reserve, vm.events), (2, 10, 0))
        vm.update(7.); vm.update(7.4)
        self.assertEqual(vm.machine['m_sCurrentState']['id'], 1)
        self.assertIsNone(vm.player['cojvrReloadProbeWeapon'])
        self.assertEqual((vm.ammo, vm.reserve, vm.events), (2, 10, 0))

    def test_insert_uses_one_native_transfer_per_call_and_full_closes(self):
        vm = ManualVM(); vm.wait()
        for count in range(3, 7):
            self.assertTrue(vm.call('cojvrInsertManualRound'))
            self.assertEqual((vm.ammo, vm.reserve, vm.events), (count, 12-count, count-2))
            self.assertEqual(vm.machine['m_sCurrentState']['id'], 21 if count<6 else 22)
        self.assertFalse(vm.call('cojvrInsertManualRound'))
        vm.update(1.); self.assertIsNone(vm.player['cojvrReloadProbeWeapon'])
        self.assertEqual((vm.ammo, vm.reserve), (6, 6))

    def test_last_reserve_round_closes_and_empty_or_full_adds_nothing(self):
        vm = ManualVM(); vm.wait(); vm.reserve=1
        self.assertTrue(vm.call('cojvrInsertManualRound'))
        self.assertEqual((vm.ammo, vm.reserve), (3, 0))
        self.assertEqual(vm.machine['m_sCurrentState']['id'], 22)
        for ammo, reserve in ((6, 5), (2, 0)):
            vm=ManualVM(); vm.wait(); vm.ammo,vm.reserve=ammo,reserve
            self.assertFalse(vm.call('cojvrInsertManualRound'))
            self.assertEqual((vm.ammo,vm.reserve,vm.events),(ammo,reserve,0))

    def test_insertion_rejects_opening_closing_and_changed_owners(self):
        for change in ('opening','closing','actual','desired','support','machine','dead','network','carrying','attack'):
            with self.subTest(change=change):
                vm=ManualVM(); vm.wait()
                if change=='opening': vm.player['cojvrReloadProbeStatus']=1
                if change=='closing': vm.run('ArmedPlayerBeing','cojvrCancelReloadProbe','()V',vm.player)
                if change in ('actual','desired'): vm.player[change]={'class':'replacement'}
                if change=='support': vm.other_weapon={'class':'occupied'}
                if change=='machine': vm.machine=dict(vm.other)
                if change=='dead': vm.player['dead']=True
                if change=='network': vm.net_game=True
                if change=='carrying': vm.player['carrying']=True
                if change=='attack': vm.player['desired_state']=31
                self.assertFalse(vm.call('cojvrInsertManualRound'))
                self.assertEqual((vm.ammo,vm.reserve,vm.events),(2,10,0))

    def test_cancel_after_insert_does_not_add_another_round(self):
        vm=ManualVM(); vm.wait(); self.assertTrue(vm.call('cojvrInsertManualRound'))
        vm.run('ArmedPlayerBeing','cojvrCancelReloadProbe','()V',vm.player)
        vm.update(1.)
        self.assertEqual((vm.ammo,vm.reserve,vm.events),(3,9,1))
        self.assertIsNone(vm.machine['cojvrReloadProbeOwner'])

    def test_invalid_clock_deadline_or_foreign_veto_cannot_insert_or_renew(self):
        for value in (float('nan'),float('inf'),float('-inf'),.1,3.):
            with self.subTest(clock=value):
                vm=ManualVM();vm.wait();vm.machine['m_fCurrentTime']=value
                self.assertFalse(vm.call('cojvrKeepManualReloadAlive'))
                self.assertFalse(vm.call('cojvrInsertManualRound'))
                self.assertEqual((vm.ammo,vm.reserve,vm.events),(2,10,0))
        vm=ManualVM();vm.wait();vm.machine['cojvrReloadProbeOwner']={'foreign':True}
        self.assertFalse(vm.call('cojvrInsertManualRound'))
        self.assertEqual((vm.ammo,vm.reserve,vm.events),(2,10,0))

    def test_native_transfer_is_not_duplicated_or_ammo_written_in_helpers(self):
        vm=ManualVM(); pool,methods=vm.classes['ArmedPlayerBeing']
        code='\n'.join(line for (name,_),m in methods.items() if 'Manual' in name and m.code
                       for line in disassemble(m.code,pool))
        self.assertEqual(code.count('WeaponReload(LWeapon;)Z'),1)
        self.assertNotIn('SetAmmoCount',code); self.assertNotIn('SetAmount',code)

    def test_archive_manual_variant_is_explicit_and_unrelated_payloads_unchanged(self):
        import inspect
        self.assertTrue('manual_reload' in inspect.signature(patch.patch_archive).parameters)
        from reload_wait_probe import ARCHIVE
        # Headset staging replaces code.pak; use only the verified original.
        source = next((p for p in (ARCHIVE, ARCHIVE.with_name('code.cojvr-backup.pak'))
                       if p.exists() and hashlib.sha256(p.read_bytes()).hexdigest()
                       == patch.PAK_SHA256), None)
        if source is None:
            raise unittest.SkipTest('Exact original game archive unavailable')
        with tempfile.TemporaryDirectory() as temp:
            output=Path(temp)/'manual.pak'
            patch.patch_archive(source,output,manual_reload=True)
            with zipfile.ZipFile(source) as before,zipfile.ZipFile(output) as after:
                self.assertEqual(before.namelist(),after.namelist())
                changed={n for n in before.namelist() if before.read(n)!=after.read(n)}
                self.assertEqual(changed,{'ArmedPlayerBeing.class','StateMashine.class','WeaponFire.class',
                    'BeingTriggered.class','LawmanModuleSingle.class','WeaponWhip.class'})
            with self.assertRaises(ValueError):
                patch.patch_archive(source,Path(temp)/'both.pak',reload_wait_probe=True,manual_reload=True)
            self.assertFalse((Path(temp)/'both.pak').exists())

    def test_all_manual_symbols_resolve_and_branches_retain_native_save_order(self):
        from reload_wait_probe import ReloadWaitProbeTests
        old=patch.patch_reload_probe_classes
        try:
            patch.patch_reload_probe_classes=patch.patch_manual_reload_classes
            ReloadWaitProbeTests.test_every_probe_symbol_resolves_against_shipped_hierarchy(self)
            ReloadWaitProbeTests.test_probe_branch_targets_and_native_save_order(self)
        finally: patch.patch_reload_probe_classes=old


if __name__=='__main__': unittest.main()
