"""Lab transaction executes original transfer/callback bytecode; no game claims."""
import inspect
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).parent))
from manual_reload_native import ManualVM
from reload_wait_probe import original, JVMThrown
from inspect_java_bytecode import parse_class
from coj_offline_manual_reload import patch_offline_classes


class CycleVM(ManualVM):
    def __init__(self, weapon='WeaponRifleWinchester'):
        super().__init__()
        player, machine = patch_offline_classes(original('ArmedPlayerBeing'), original('StateMashine'),
                                                 weapon, cycle_animation=True)
        for name, data in (('ArmedPlayerBeing', player), ('StateMashine', machine), ('PawnArmed', original('PawnArmed'))):
            pool, methods = parse_class(data)
            self.classes[name] = pool, {(m.name, m.descriptor): m for m in methods}
        self.weapon['class'] = weapon
        self.weapon['cOwner'] = self.player
        self.capacity = 12 if weapon == 'WeaponRifleWinchester' else 8
        self.counter = {'amount': 10}
        self.fault_at = None
        self.on_event = None
        self.on_body = None
        self.on_capacity = None
        self.started = self.zero_reloads = self.important_states = 0

    def fault(self, point):
        if self.fault_at == point:
            raise JVMThrown({'class': 'Throwable', 'point': point})

    def native(self, owner, name, args):
        if name == 'WeaponReload':
            return self.run('PawnArmed', 'WeaponReload', '(LWeapon;)Z', owner, args)
        if name == 'GetAmmoCount': return self.ammo
        if name == 'GetAmmoCapacity':
            if self.on_capacity: self.on_capacity()
            return self.capacity
        # Original PawnInventory indices: rifle=0, shotgun=1, pistol=2.
        if name == 'GetAmmoType': return 0 if self.weapon['class'] == 'WeaponRifleWinchester' else 2
        if name == 'FindAmmoCounter': return self.counter
        if name == 'GetAmount': return owner['amount']
        if name == 'SetAmmoCount':
            self.ammo = args[1]; self.fault('loaded'); return None
        if name == 'SetAmount':
            owner['amount'] = self.reserve = args[0]; self.fault('reserve'); return None
        if name == 'GetWeaponMaxAmmoToReload': return min(self.capacity-self.ammo, self.counter['amount'])
        if name == 'OnGameEvent':
            self.events += 1
            if self.on_event: self.on_event()
            self.fault('event'); return None
        if name == 'IncHandImportatntState': self.important_states += 1; return None
        if name == 'Reload':
            assert args == [0, 0], 'Body start is not an inventory transfer'
            self.zero_reloads += 1
            if self.on_body: self.on_body()
            self.fault('body'); return 0
        if name == 'Set':
            result = super().native(owner, name, args)
            if owner['id'] == 21:
                self.started += 1
                self.run('ArmedPlayerBeing', 'OnHandStateStarted_Reload', '(IIZ)V', self.player, [0,21,False])
            return result
        return super().native(owner, name, args)


class ManualReloadCycleTests(unittest.TestCase):
    def make(self, weapon='WeaponRifleWinchester'):
        self.assertIn('cycle_animation', inspect.signature(patch_offline_classes).parameters,
                      'Missing safe per-cartridge animation transaction')
        vm = CycleVM(weapon); vm.wait()
        return vm

    def test_one_native_unit_restarts_body_without_terminating_desired_reload(self):
        for weapon in ('WeaponRifleWinchester', 'WeaponPistolVolcanic'):
            vm = self.make(weapon)
            self.assertTrue(vm.call('cojvrInsertManualRound'))
            self.assertEqual((vm.ammo, vm.counter['amount'], vm.events), (3,9,1))
            self.assertEqual(vm.player['desired_state'], 21)
            self.assertEqual(vm.player['cojvrReloadProbeStatus'], 1)
            self.assertEqual(vm.started, 2)
            self.assertEqual(vm.zero_reloads, 2)
            self.assertEqual(vm.important_states, 2)
            self.assertFalse(vm.player['cojvrManualCycleActive'])
            self.assertIsNone(vm.player['cojvrManualCycleWeapon'])
            self.assertFalse(vm.call('cojvrInsertManualRound'), 'Cannot insert before the next body completion')
            vm.update(.95)
            self.assertEqual(vm.player['cojvrReloadProbeStatus'], 2)
            self.assertTrue(vm.call('cojvrInsertManualRound'))
            self.assertEqual((vm.ammo, vm.counter['amount'], vm.events), (4,8,2))

    def test_final_round_closes_without_reopening_the_body(self):
        for full in (False, True):
            vm = self.make()
            if full: vm.ammo = vm.capacity-1
            else: vm.counter['amount'] = vm.reserve = 1
            started = vm.started
            self.assertTrue(vm.call('cojvrInsertManualRound'))
            self.assertEqual(vm.started, started)
            self.assertEqual(vm.player['cojvrReloadProbeStatus'], 3)
            self.assertEqual(vm.machine['m_sCurrentState']['id'], 22)
            self.assertFalse(vm.call('cojvrInsertManualRound'))
            self.assertEqual(vm.events, 1)

    def test_fault_finally_clears_scope_and_prevents_retry_or_renewal(self):
        for point in ('loaded','reserve','event','body'):
            with self.subTest(point=point):
                vm = self.make(); vm.fault_at = point
                with self.assertRaises(JVMThrown) as thrown: vm.call('cojvrInsertManualRound')
                self.assertEqual(thrown.exception.value['point'], point)
                self.assertFalse(vm.player['cojvrManualCycleActive'])
                self.assertFalse(vm.player['cojvrManualCycleSelfClear'])
                self.assertIsNone(vm.player['cojvrManualCycleWeapon'])
                self.assertIsNone(vm.player['cojvrManualCycleMachine'])
                self.assertTrue(vm.player['cojvrManualCycleFaulted'])
                counts = vm.ammo,vm.counter['amount'],vm.events
                self.assertFalse(vm.call('cojvrInsertManualRound'))
                self.assertFalse(vm.call('cojvrKeepManualReloadAlive'))
                self.assertEqual((vm.ammo,vm.counter['amount'],vm.events),counts)

    def test_reentrant_insert_from_event_or_body_is_consumed_without_another_unit(self):
        for point in ('on_event','on_body'):
            vm = self.make(); reentered=[]
            setattr(vm,point,lambda: reentered.append(vm.call('cojvrInsertManualRound')))
            self.assertTrue(vm.call('cojvrInsertManualRound'))
            self.assertEqual(reentered,[False])
            self.assertEqual((vm.ammo,vm.counter['amount'],vm.events),(3,9,1))

    def test_reentry_during_parameter_read_is_blocked_before_native_transfer(self):
        vm=self.make();reentered=[]
        def during_read():
            vm.on_capacity=None
            reentered.append(vm.call('cojvrInsertManualRound'))
        vm.on_capacity=during_read
        self.assertTrue(vm.call('cojvrInsertManualRound'))
        self.assertEqual(reentered,[False])
        self.assertEqual((vm.ammo,vm.counter['amount'],vm.events),(3,9,1))

    def test_counter_replacement_or_unexplained_count_delta_never_cycles(self):
        for change in ('counter','extra_count','extra_reserve','capacity','false_success'):
            vm=self.make();started=vm.started
            if change=='false_success':
                vm.native_before=vm.native
                vm.native=lambda owner,name,args: True if name=='WeaponReload' else vm.native_before(owner,name,args)
            else:
                def alter():
                    if change=='counter':vm.counter=dict(vm.counter)
                    if change=='extra_count':vm.ammo+=1
                    if change=='extra_reserve':vm.counter['amount']-=1
                    if change=='capacity':vm.capacity+=1
                vm.on_event=alter
            self.assertFalse(vm.call('cojvrInsertManualRound'))
            self.assertEqual(vm.started,started)
            self.assertTrue(vm.player['cojvrManualCycleFaulted'])
            if change=='false_success':
                self.assertEqual(vm.events,0,'Native Boolean without a unit delta must not publish an event')

    def test_parameter_callback_cancellation_or_desired_state_change_prevents_transfer(self):
        for change in ('cancel', 'desired_state'):
            with self.subTest(change=change):
                vm=self.make()
                def change_eligibility():
                    vm.on_capacity=None
                    if change=='cancel':
                        vm.run('ArmedPlayerBeing','cojvrCancelReloadProbe','()V',vm.player)
                    else:
                        vm.player['desired_state']=31
                vm.on_capacity=change_eligibility
                self.assertFalse(vm.call('cojvrInsertManualRound'))
                self.assertEqual((vm.ammo,vm.counter['amount'],vm.events),(2,10,0))
                self.assertEqual(vm.player['desired_state'],1 if change=='cancel' else 31)
                self.assertFalse(vm.player['cojvrManualCycleActive'])
                self.assertIsNone(vm.player['cojvrManualCycleWeapon'])

    def test_owner_change_or_cancellation_after_transfer_never_forces_desired21(self):
        for change in ('owner','actual','desired','machine','cancel'):
            vm = self.make()
            def alter():
                if change=='owner': vm.weapon['cOwner']={'foreign':True}
                if change in ('actual','desired'): vm.player[change]={'foreign':True}
                if change=='machine': vm.machine=dict(vm.other)
                if change=='cancel': vm.run('ArmedPlayerBeing','cojvrCancelReloadProbe','()V',vm.player)
            vm.on_event=alter
            self.assertFalse(vm.call('cojvrInsertManualRound'))
            self.assertEqual(vm.started,1)
            self.assertEqual((vm.ammo,vm.counter['amount'],vm.events),(3,9,1))
            self.assertTrue(vm.player['cojvrManualCycleFaulted'])

    def test_foreign_native_weapon_owner_never_transfers(self):
        vm=self.make();vm.weapon['cOwner']={'foreign':True}
        self.assertFalse(vm.call('cojvrInsertManualRound'))
        self.assertEqual((vm.ammo,vm.counter['amount'],vm.events),(2,10,0))

    def test_ordinary_close_after_a_cycle_preserves_original_zero_transfer_end(self):
        vm=self.make();self.assertTrue(vm.call('cojvrInsertManualRound'))
        vm.run('ArmedPlayerBeing','cojvrCancelReloadProbe','()V',vm.player)
        self.assertEqual(vm.player['desired_state'],1)
        vm.update(1.)
        self.assertIsNone(vm.player['cojvrReloadProbeWeapon'])
        self.assertEqual((vm.ammo,vm.counter['amount'],vm.events),(3,9,1))


if __name__ == '__main__': unittest.main()
