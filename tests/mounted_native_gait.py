"""Execute exact original gait bytecode; no game/JVM/process or archive writes.

Reuses the bounded opcode interpreter. Engine permission, liveness and the
no-added-speed scenario are leaves; controller Walk/Run, flag storage, native
eligibility and Horse.GetMoveForwardSpeed all execute original class bytes.
Array entries are distinct fixture sentinels, not claimed physical speeds.
"""
import unittest

from duel_input_native import NativeFixture, archive, parse_class


class MountedFixture(NativeFixture):
    def __init__(self):
        self.classes = {}
        with archive() as pak:
            for name in ('HorseController', 'CreatureController', 'BeingController', 'Horse', 'Creature'):
                pool, methods = parse_class(pak.read(name + '.class'))
                self.classes[name] = pool, {(m.name, m.descriptor): m for m in methods}
        self.locked = False
        self.statics = {}
        flags = dict(m_bCanMove=True, m_bWalk=False, m_bToggleWalk=False,
                     m_bRun=False, m_bToggleRun=False, m_bDuck=False, m_bToggleDuck=False)
        self.rider = dict(flags, kind='Creature')
        self.horse = dict(flags, kind='Horse', m_bNewPlayerControl=True,
                          m_cHorseman=self.rider, m_fFwdLocalSpeed=1.,
                          m_afWalkSpeed=[10., 0., 20., 30., 40., 50.])
        self.controller = dict(kind='HorseController', m_cControlledCreature=self.horse,
                               m_bWalk=False, m_bRun=False)

    def invoke(self, owner, name, desc, args, obj, special=False):
        if owner == 'BeingController' and name == 'ExecuteInput':
            return None  # unrelated base actions; movement/gait execute below
        if name == 'CanApplyState':
            return not self.locked
        if name == 'IsNotAlive':
            return False
        if name == 'GetAddSpeedState':
            return -1  # ordinary new-player control, no scripted speed override
        if name == 'CanExecuteInput' and owner == 'CreatureController':
            return True  # underlying engine permission, HorseController still executes
        kind = obj['kind'] if obj else owner
        if special:
            candidates = [owner]
        elif kind == 'HorseController':
            candidates = ['HorseController', 'CreatureController', 'BeingController']
        elif kind == 'Horse':
            candidates = ['Horse', 'Creature']
        else:
            candidates = ['Creature']
        for cls in candidates:
            if (name, desc) in self.classes[cls][1]:
                return self.run(cls, name, desc, args, obj)
        raise AssertionError(('unmodeled native seam', owner, name, desc))

    def action(self, action, held):
        args = [action, float(held), None]
        assert self.run('HorseController', 'CanExecuteInput', '(IFLjava/lang/Object;)Z', args, self.controller)
        assert ('ExecuteInput', '(IFLjava/lang/Object;)V') not in self.classes['HorseController'][1]
        self.run('CreatureController', 'ExecuteInput', '(IFLjava/lang/Object;)V', args, self.controller)

    def speed(self):
        return self.run('Horse', 'GetMoveForwardSpeed', '()F', [], self.horse)


class NativeMountedGaitTests(unittest.TestCase):
    def test_native_walk_action_sets_inherited_horse_flag_and_selects_walk_gait(self):
        vm = MountedFixture()
        self.assertEqual(vm.speed(), 40.)  # original ordinary gait array slot4
        vm.action(14, True)
        self.assertTrue(vm.controller['m_bWalk'])
        self.assertTrue(vm.horse['m_bWalk'])
        self.assertEqual(vm.speed(), 30.)  # original CanWalk branch -> slot3
        vm.action(14, False)
        self.assertFalse(vm.horse['m_bWalk'])
        self.assertEqual(vm.speed(), 40.)

    def test_locked_walk_defers_native_flag_until_apply_and_run_remains_separate(self):
        vm = MountedFixture()
        vm.locked = True
        vm.action(14, True)
        self.assertTrue(vm.controller['m_bWalk'])
        self.assertFalse(vm.horse['m_bWalk'])
        vm.locked = False
        vm.run('CreatureController', 'ApplyWalk', '()V', [], vm.controller)
        self.assertEqual(vm.speed(), 30.)
        # Native run permission also depends on the rider's own run flags.
        # The real action target list forwards gait to rider and horse.
        vm.action(14, False)
        vm.rider['m_bRun'] = True
        vm.action(18, True)
        self.assertTrue(vm.horse['m_bRun'])
        self.assertEqual(vm.speed(), 50.)
        vm.action(14, True)
        self.assertEqual(vm.speed(), 30.)  # native Walk vetoes CanRun even held
        vm.action(18, False)
        self.assertEqual(vm.speed(), 30.)
        vm.horse['m_fFwdLocalSpeed'] = -1.
        self.assertEqual(vm.speed(), 10.)  # reverse retains original slot0
        vm.horse['m_fFwdLocalSpeed'] = 0.
        self.assertEqual(vm.speed(), 0.)  # stop retains original slot1


if __name__ == '__main__':
    unittest.main()
