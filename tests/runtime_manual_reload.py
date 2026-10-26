"""Exercise the production multi-weapon derivative; no headset claims."""
import sys
from pathlib import Path
import unittest

sys.path.insert(0,str(Path(__file__).parent))
import manual_reload_cycle as cycle
from manual_reload_native import ManualVM
from reload_wait_probe import original
from inspect_java_bytecode import parse_class
from patch_coj_weapon_consumers import patch_manual_reload_classes


class RuntimeCycleVM(cycle.CycleVM):
    def __init__(self,weapon='WeaponRifleWinchester'):
        super().__init__(weapon)
        player,machine=patch_manual_reload_classes(original('ArmedPlayerBeing'),original('StateMashine'))
        for name,data in (('ArmedPlayerBeing',player),('StateMashine',machine)):
            pool,methods=parse_class(data)
            self.classes[name]=pool,{(m.name,m.descriptor):m for m in methods}


class RuntimeManualReloadTests(cycle.ManualReloadCycleTests):
    # All native-cycle callback, readback, exception, watchdog and duplicate
    # tests now exercise the combined derivative used by the installer.
    def make(self,weapon='WeaponRifleWinchester'):
        vm=RuntimeCycleVM(weapon);vm.wait();return vm

    def test_first_heartbeat_probe_loss_throw_never_loads_without_insertion(self):
        for kind in ('WeaponRifleWinchester','WeaponPistolPeacemaker'):
            with self.subTest(kind=kind):
                vm=RuntimeCycleVM();vm.weapon['class']=kind
                if kind=='WeaponPistolPeacemaker':vm.capacity=6
                vm.wait();exception={'class':'Throwable','unique':object()};native=vm.native
                def fail(owner,name,args):
                    result=native(owner,name,args)
                    if name=='GetAmmoTypeCount':
                        vm.native=native
                        vm.run('ArmedPlayerBeing','cojvrClearReloadProbe','(I)V',vm.player,[0])
                        raise cycle.JVMThrown(exception)
                    return result
                vm.native=fail
                with self.assertRaises(cycle.JVMThrown) as caught:vm.call('cojvrKeepManualReloadAlive')
                self.assertIs(caught.exception.value,exception)
                vm.update(1.)
                self.assertEqual((vm.ammo,vm.counter['amount'],vm.events),(2,10,0))
                self.assertEqual(vm.player['desired_state'],1)

    def test_combined_derivative_preserves_revolver_route(self):
        vm=ManualVM();vm.wait()
        self.assertTrue(vm.call('cojvrInsertManualRound'))
        self.assertEqual((vm.ammo,vm.reserve,vm.events),(3,9,1))

    def test_revolver_initial_validator_probe_loss_throw_keeps_late_completion_zero(self):
        vm=RuntimeCycleVM();vm.weapon['class']='WeaponPistolPeacemaker';vm.capacity=6;vm.wait()
        exception={'class':'Throwable','unique':object()};native=vm.native
        def fail(owner,name,args):
            result=native(owner,name,args)
            if name=='GetAmmoTypeCount':
                vm.native=native
                vm.run('ArmedPlayerBeing','cojvrClearReloadProbe','(I)V',vm.player,[0])
                raise cycle.JVMThrown(exception)
            return result
        vm.native=fail
        before=vm.ammo,vm.counter['amount'],vm.events
        with self.assertRaises(cycle.JVMThrown) as caught:vm.call('cojvrInsertManualRound')
        self.assertIs(caught.exception.value,exception)
        vm.update(1.)
        self.assertEqual((vm.ammo,vm.counter['amount'],vm.events),before)
        self.assertEqual(vm.player['desired_state'],1)
        vm.update(1.4)
        vm.player['cojvrManualReloadWeapon']=None
        vm.run('ArmedPlayerBeing','ReloadWeapon','(I)V',vm.player,[0])
        vm.machine.update(m_sCurrentState=vm.states[20],start=1.4)
        vm.update(1.8);vm.update(2.2)
        self.assertEqual((vm.ammo,vm.counter['amount'],vm.events),(3,9,1))

    def test_revolver_event_probe_loss_throw_never_duplicates_the_committed_unit(self):
        vm=RuntimeCycleVM();vm.weapon['class']='WeaponPistolPeacemaker';vm.capacity=6;vm.wait()
        exception={'class':'Throwable','unique':object()}
        def fail():
            vm.run('ArmedPlayerBeing','cojvrClearReloadProbe','(I)V',vm.player,[0])
            raise cycle.JVMThrown(exception)
        vm.on_event=fail
        with self.assertRaises(cycle.JVMThrown) as caught:vm.call('cojvrInsertManualRound')
        self.assertIs(caught.exception.value,exception)
        vm.on_event=None
        self.assertEqual((vm.ammo,vm.counter['amount'],vm.events),(3,9,1))
        vm.update(1.)
        self.assertEqual((vm.ammo,vm.counter['amount'],vm.events),(3,9,1))

    def test_revolver_retained_ticket_cannot_load_a_foreign_native_owner(self):
        vm=RuntimeCycleVM();vm.weapon['class']='WeaponPistolPeacemaker';vm.capacity=6;vm.wait()
        vm.weapon['cOwner']={'class':'foreign'}
        self.assertFalse(vm.call('cojvrInsertManualRound'))
        self.assertEqual((vm.ammo,vm.counter['amount'],vm.events),(2,10,0))

    def test_parameter_read_cannot_replace_native_revolver_owner_before_transfer(self):
        vm=RuntimeCycleVM();vm.weapon['class']='WeaponPistolPeacemaker';vm.capacity=6;vm.wait()
        native=vm.native
        def replaced(owner,name,args):
            result=native(owner,name,args)
            if name=='GetAmmoTypeCount':vm.weapon['cOwner']={'class':'foreign'}
            return result
        vm.native=replaced
        self.assertFalse(vm.call('cojvrInsertManualRound'))
        self.assertEqual((vm.ammo,vm.counter['amount'],vm.events),(2,10,0))

    def test_unknown_subclasses_or_missing_index_never_use_unit_entry(self):
        for name in ('UnknownWinchester','WeaponRifleWinchesterScopeDerived','WeaponRifleRemington'):
            vm=RuntimeCycleVM();vm.weapon['class']=name;vm.wait()
            self.assertFalse(vm.call('cojvrInsertManualRound'))
            self.assertEqual((vm.ammo,vm.counter['amount'],vm.events),(2,10,0))


if __name__=='__main__':unittest.main()
