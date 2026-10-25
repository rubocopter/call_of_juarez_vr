"""Execute isolated indexed helper + original storage/counter bytecode, never the game."""
import functools
import copy
import hashlib
import importlib.util
import io
import os
import re
import sys
import unittest
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
sys.path.insert(0, str(Path(__file__).parent))
from reload_wait_probe import JVMThrown
from inspect_java_bytecode import Reader, parse_constant_pool, parse_class, disassemble

GAME = Path(os.environ.get('COJ_GAME_DIRECTORY',
    'C:/Program Files (x86)/Steam/steamapps/common/Call of Juarez'))
ARCHIVE = GAME / 'code.cojvr-backup.pak'
if not ARCHIVE.exists():
    ARCHIVE = GAME / 'code.pak'
ARCHIVE_HASH = 'f9db47c166e03f23e37cbcdfd5344e4ad4c5c9134f35e8f6dcdf66db7e71ce12'
PROFILES = {
    'WeaponRifleRemington': ((1, 1), (1, 1)),
    'WeaponRifleRemingtonSawOff': ((1, 1), (1, 1)),
    'WeaponPistolDerringer': ((1, 1), (2, 2)),
    'WeaponPistolLemant': ((1, 8), (0, 2)),
}

@functools.lru_cache(None)
def original_archive():
    if not ARCHIVE.exists(): raise unittest.SkipTest('Pinned original archive unavailable')
    data = ARCHIVE.read_bytes()
    if hashlib.sha256(data).hexdigest() != ARCHIVE_HASH:
        raise AssertionError('Original fixture archive hash mismatch')
    return data


class IndexedVM:
    """Reuse bounded opcode engine, interpreting original setter inheritance too.

    External stand-ins: native hand/state/network eligibility, immutable parameter
    reads, inventory table lookup and UpdateAmmoSkin's native presentation. Count
    storage, clamping and typed reserve storage execute ORIGINAL method bodies.
    """
    def __init__(self, emitter, kind='WeaponPistolLemant', hand=0, shared=False):
        self.classes, self.parents, self.frames = {}, {}, []
        with zipfile.ZipFile(io.BytesIO(original_archive())) as z:
            for member in z.namelist():
                if member.endswith('.class'):
                    name = member[:-6]
                    if name.startswith(('Weapon', 'InventoryAmmoCounter')) or name in ('ArmedPlayerBeing', 'PawnArmed'):
                        data = z.read(member)
                        if name == 'ArmedPlayerBeing': data = emitter(original_archive())
                        pool, methods = parse_class(data)
                        self.classes[name] = pool, {(m.name, m.descriptor): m for m in methods}
                        reader = Reader(data); reader.read(8); pool = parse_constant_pool(reader)
                        reader.u2(); reader.u2(); parent = reader.u2()
                        self.parents[name] = pool.describe(parent) if parent else None
        self.hand, self.shared = hand, shared
        self.player = {'class': 'ArmedPlayerBeing', 'm_bNetAttackForced': 0,
                       'nAmmoRifle': 10, 'nAmmoShotgun': 10, 'nAmmoPistol': 10}
        self.weapon = {'class': kind, 'cOwner': self.player, 'm_iAmmo0Count': 0, 'm_iAmmo1Count': 0}
        self.machine = {'class': 'PlayerStateMashine'}
        self.capacities, self.types = map(list, PROFILES[kind])
        self.counters = [{'class': 'InventoryAmmoCounter' + suffix,
                          'cOwner': self.player, 'nMaxAmount': 99}
                         for suffix in ('Rifle', 'Shotgun', 'Pistol')]
        self.alive = self.net_owner = True
        self.carrying = self.net_game = False
        self.current_state = self.desired_state = 21
        self.slots = [[None] * 3 for _ in range(2)]
        self.slots[hand] = [self.weapon] * 3
        if shared: self.slots[1-hand] = [self.weapon] * 3
        self.calls, self.writes, self.skin_calls = [], [], 0
        self.hook = None

    def run(self, cls, name, desc, owner, args=()):
        self.frames.append((cls, name))
        try: return self.execute(cls, name, desc, owner, args)
        finally: self.frames.pop()

    def execute(self, cls, name, desc, owner, args):
        pool, methods = self.classes[cls]
        method = methods[name, desc]
        code = method.code
        assert code is not None, f'Abstract/native body: {cls}.{name}{desc}'
        locals_ = [owner, *args] + [None] * method.max_locals
        stack, pc, steps = [], 0, 0
        while pc < len(code):
            steps += 1
            assert steps < 20000, 'Unbounded indexed bytecode'
            assert len(stack) <= method.max_stack, 'Emitted max_stack too small'
            at, op = pc, code[pc]; pc += 1
            try:
                if op == 0x00: pass
                elif op == 0x01: stack.append(None)
                elif 0x02 <= op <= 0x08: stack.append(op - 3)
                elif 0x0b <= op <= 0x0d: stack.append(float(op-0x0b))
                elif op in (0x10,0x11):
                    size=1 if op==0x10 else 2
                    stack.append(int.from_bytes(code[pc:pc+size], 'big', signed=True)); pc += size
                elif op == 0x13:
                    entry = pool.get(int.from_bytes(code[pc:pc+2], 'big')); pc += 2
                    stack.append(pool.utf8(entry[1]) if entry[0]=='String' else entry[1])
                elif op in (0x15, 0x17, 0x19):
                    slot = code[pc]; pc += 1
                    assert slot < method.max_locals
                    stack.append(locals_[slot])
                elif 0x1a <= op <= 0x1d: stack.append(locals_[op - 0x1a])
                elif 0x2a <= op <= 0x2d: stack.append(locals_[op - 0x2a])
                elif op in (0x36, 0x38, 0x3a):
                    slot = code[pc]; pc += 1
                    assert slot < method.max_locals
                    locals_[slot] = stack.pop()
                elif 0x3b <= op <= 0x3e: locals_[op - 0x3b] = stack.pop()
                elif 0x4b <= op <= 0x4e:locals_[op-0x4b]=stack.pop()
                elif op == 0x59: stack.append(stack[-1])
                elif op == 0x32:
                    index=stack.pop();array=stack.pop();stack.append(array[index])
                elif op == 0x57: stack.pop()
                elif op in (0x60, 0x64):
                    b, a = stack.pop(), stack.pop()
                    value = a + b if op == 0x60 else a - b
                    stack.append((value + (1 << 31)) % (1 << 32) - (1 << 31))
                elif op == 0x62:stack.append(stack.pop()+stack.pop())
                elif op == 0x66:
                    b,a=stack.pop(),stack.pop();stack.append(a-b)
                elif op in (0x95,0x96):
                    b,a=stack.pop(),stack.pop()
                    stack.append((-1 if op==0x95 else 1) if a!=a or b!=b else (a>b)-(a<b))
                elif op == 0xc0:
                    target = pool.describe(int.from_bytes(code[pc:pc+2], 'big')); pc += 2
                    if stack[-1] is not None:
                        current = stack[-1]['class']
                        while current and current != target: current = self.parents.get(current)
                        if current is None: raise JVMThrown({'class': 'ClassCastException'})
                elif op in (0xb2, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8):
                    entry = pool.get(int.from_bytes(code[pc:pc+2], 'big')); pc += 2
                    declared = pool.describe(entry[1]); nt = pool.get(entry[2])
                    key, signature = pool.utf8(nt[1]), pool.utf8(nt[2])
                    if op == 0xb2:stack.append({} if key=='cSingleton' else 1)
                    elif op == 0xb4:
                        target = stack.pop()
                        if target is None: raise JVMThrown({'class': 'NullPointerException'})
                        stack.append(target.get(key, None if signature.startswith('L') else 0))
                    elif op == 0xb5:
                        value = stack.pop(); target = stack.pop()
                        if target is None: raise JVMThrown({'class': 'NullPointerException'})
                        target[key] = value
                    else:
                        types = re.findall(r'L[^;]+;|[IZF]', signature[1:signature.index(')')])
                        argv = [stack.pop() for _ in types][::-1]
                        target = None if op == 0xb8 else stack.pop()
                        if op != 0xb8 and target is None: raise JVMThrown({'class': 'NullPointerException'})
                        # Distinguish virtual and invokespecial precisely. The
                        # latter must not redispatch a superclass setter to its
                        # double-ammunition override and recurse forever.
                        previous = getattr(self, 'special_class', None)
                        self.special_class = declared if op == 0xb7 else None
                        try:
                            if key.startswith('cojvr'):
                                result = self.run(declared, key, signature, target, argv)
                            else: result = self.native(target, key, argv)
                        finally: self.special_class = previous
                        if signature[-1] != 'V': stack.append(result)
                elif op in (0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e,
                            0x9f, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xc6, 0xc7):
                    delta = int.from_bytes(code[pc:pc+2], 'big', signed=True); pc += 2
                    if op == 0xa7: take = True
                    elif op in (0xa5, 0xa6):
                        b, a = stack.pop(), stack.pop(); take = a is b if op == 0xa5 else a is not b
                    elif 0x9f <= op <= 0xa4:
                        b, a = stack.pop(), stack.pop()
                        take = {0x9f: a == b, 0xa0: a != b, 0xa1: a < b,
                                0xa2: a >= b, 0xa3: a > b, 0xa4: a <= b}[op]
                    else:
                        value = stack.pop()
                        if op == 0xc6: take = value is None
                        elif op == 0xc7: take = value is not None
                        else: take = {0x99: value == 0, 0x9a: value != 0, 0x9b: value < 0,
                                      0x9c: value >= 0, 0x9d: value > 0, 0x9e: value <= 0}[op]
                    if take: pc = at + delta
                elif op == 0xbf: raise JVMThrown(stack.pop())
                elif op in (0xac, 0xb0):
                    result = stack.pop(); assert not stack, 'Bad return stack'; return result
                elif op == 0xb1:
                    assert not stack, 'Bad void return stack'; return None
                else: raise AssertionError(f'Unimplemented indexed opcode {op:02x} at {cls}.{name}:{at}')
            except JVMThrown as thrown:
                handler = next((h[2] for h in method.exception_table if h[0] <= at < h[1] and h[3] == 0), None)
                if handler is None: raise
                stack, pc = [thrown.value], handler
        raise AssertionError('Missing bytecode return')

    def resolve(self, cls, name, desc):
        while cls in self.classes:
            method = self.classes[cls][1].get((name, desc))
            if method is not None: return cls
            cls = self.parents[cls]
        raise AssertionError(f'Missing original boundary {cls}.{name}{desc}')

    def native(self, owner, name, args):
        self.calls.append((name, tuple(args)))
        if self.hook: self.hook('before', name, owner, args)
        signatures = {'SetAmmoCount': '(II)V', 'GetAmmoCount': '(I)I',
                      'GetAmount': '()I', 'GetMaxAmount': '()I',
                      'SetAmount': '(I)V', 'SetInternalAmount': '(I)V'}
        if name in signatures:
            cls = self.special_class or owner['class']
            cls = self.resolve(cls, name, signatures[name])
            result = self.run(cls, name, signatures[name], owner, args)
            if name in ('SetAmmoCount', 'SetAmount'): self.writes.append((name, tuple(args)))
        elif name in ('GetActualWeapon', 'GetActiveWeapon', 'GetDesiredWeapon'):
            result = self.slots[args[0]][('GetActualWeapon', 'GetActiveWeapon', 'GetDesiredWeapon').index(name)]
        elif name == 'GetHandStateMashine': result = self.machine
        elif name == 'GetHandStateMashineState': result = self.current_state
        elif name == 'GetDesiredWeaponState': result = self.desired_state
        elif name == 'GetNetIsOwner': result = self.net_owner
        elif name == 'IsNetGame': result = self.net_game
        elif name == 'IsNotAlive': result = not self.alive
        elif name == 'IsCarrying': result = self.carrying
        elif name == 'getClass': result = owner['class']
        elif name == 'getName': result = owner
        elif name == 'equals': result = owner == args[0]
        elif name == 'GetAmmoTypeCount': result = len(self.types)
        elif name == 'GetAmmoCapacity': result = self.capacities[args[0]]
        elif name == 'GetAmmoType': result = self.types[args[0]]
        elif name == 'IsReloadWholeClipAtOnce': result = self.weapon['class'] != 'WeaponPistolLemant'
        elif name == 'FindAmmoCounter': result = self.counters[args[0]]
        elif name == 'UpdateAmmoSkin': self.skin_calls += 1; result = None
        else: raise AssertionError('Unmodeled indexed lab boundary: ' + name)
        if self.hook: self.hook('after', name, owner, args)
        return result

    def insert(self, index=1):
        return self.run('ArmedPlayerBeing', 'cojvrLabInsertIndexedUnit', '(LWeapon;II)Z',
                        self.player, [self.weapon, self.hand, index])

    def vector(self):
        return ((self.weapon['m_iAmmo0Count'], self.weapon['m_iAmmo1Count']),
                tuple(self.player['nAmmo'+suffix] for suffix in ('Rifle', 'Shotgun', 'Pistol')))


class IndexedManualReloadTests(unittest.TestCase):
    def emitter(self):
        self.assertIsNotNone(importlib.util.find_spec('coj_indexed_reload'),
                             'Missing isolated selected-index bytecode transaction')
        import coj_indexed_reload
        return coj_indexed_reload.build_indexed_reload_lab

    def make(self, kind='WeaponPistolLemant', **kwargs):
        return IndexedVM(self.emitter(), kind, **kwargs)

    def test_selected_second_slot_does_not_load_eligible_first_slot(self):
        for kind in PROFILES:
            for hand in (0, 1):
                for shared in (False, True) if kind.startswith('WeaponRifle') else (False,):
                    with self.subTest(kind=kind, hand=hand, shared=shared):
                        vm = self.make(kind, hand=hand, shared=shared)
                        self.assertTrue(vm.insert(1))
                        loaded, reserve = vm.vector()
                        self.assertEqual(loaded, (0, 1))
                        self.assertEqual(reserve, (10, 9, 10) if 'Remington' in kind else (10, 10, 9))
                        self.assertEqual(vm.skin_calls, 1)
                        self.assertFalse(vm.player['cojvrIndexedBusy'])
                        self.assertNotIn('WeaponReload', [name for name, _ in vm.calls])

    def test_first_slot_and_last_reserve_have_exact_unit_accounting(self):
        for kind in PROFILES:
            vm = self.make(kind)
            pool = PROFILES[kind][1][0]
            field = ('nAmmoRifle', 'nAmmoShotgun', 'nAmmoPistol')[pool]
            vm.player[field] = 1
            self.assertTrue(vm.insert(0))
            self.assertEqual(vm.vector()[0], (1, 0))
            self.assertEqual(vm.player[field], 0)
            before = vm.vector()
            self.assertFalse(vm.insert(0))
            self.assertEqual(vm.vector(), before)

    def test_bad_eligibility_does_not_mutate_or_quarantine(self):
        for change in ('network', 'nonowner', 'forced', 'dead', 'carrying', 'state',
                       'desired_state', 'actual', 'desired', 'active', 'owner', 'support', 'unknown_class',
                       'capacity', 'type', 'negative_loaded', 'overfull', 'negative_reserve', 'counter_owner', 'counter_class', 'overmax'):
            with self.subTest(change=change):
                vm = self.make()
                if change == 'network': vm.net_game = True
                if change == 'nonowner': vm.net_owner = False
                if change == 'forced': vm.player['m_bNetAttackForced'] = 1
                if change == 'dead': vm.alive = False
                if change == 'carrying': vm.carrying = True
                if change == 'state': vm.current_state = 20
                if change == 'desired_state': vm.desired_state = 1
                if change in ('actual', 'active', 'desired'):
                    vm.slots[0][('actual', 'active', 'desired').index(change)] = None
                if change == 'owner': vm.weapon['cOwner'] = {}
                if change == 'support': vm.slots[1] = [vm.weapon] * 3
                if change == 'unknown_class': vm.weapon['class'] = 'DerivedLemant'
                if change == 'capacity': vm.capacities[1] = 9
                if change == 'type': vm.types[0] = 2
                if change == 'negative_loaded': vm.weapon['m_iAmmo0Count'] = -1
                if change == 'overfull': vm.weapon['m_iAmmo1Count'] = 9
                if change == 'negative_reserve': vm.player['nAmmoShotgun'] = -1
                if change == 'counter_owner': vm.counters[0]['cOwner'] = {}
                if change == 'counter_class': vm.counters[0]['class'] = 'InventoryAmmoCounterPistol'
                if change == 'overmax': vm.player['nAmmoRifle'] = 100
                before = vm.vector()
                self.assertFalse(vm.insert())
                self.assertEqual(vm.vector(), before)
                self.assertEqual(vm.skin_calls, 0)
                self.assertFalse(vm.player['cojvrIndexedBusy'])
                self.assertFalse(vm.player.get('cojvrIndexedFaulted', False))

    def test_reentry_during_first_virtual_read_and_setter_is_blocked(self):
        for boundary in ('getClass', 'GetAmmoCapacity', 'GetAmount', 'UpdateAmmoSkin', 'SetInternalAmount'):
            with self.subTest(boundary=boundary):
                vm = self.make(); results = []
                def hook(when, name, owner, args):
                    if when == 'before' and name == boundary:
                        vm.hook = None
                        results.append(vm.insert())
                vm.hook = hook
                self.assertTrue(vm.insert())
                self.assertEqual(results, [False])
                self.assertEqual(vm.vector(), ((0, 1), (10, 10, 9)))

    def test_faults_clear_busy_and_quarantine_without_refund_or_retry(self):
        for boundary in ('getClass', 'GetAmmoCapacity', 'GetAmount', 'UpdateAmmoSkin', 'SetInternalAmount', 'SetAmount'):
            with self.subTest(boundary=boundary):
                vm = self.make()
                def hook(when, name, owner, args):
                    if when == 'after' and name == boundary:
                        raise JVMThrown({'class': 'Throwable', 'point': boundary})
                vm.hook = hook
                with self.assertRaises(JVMThrown): vm.insert()
                self.assertFalse(vm.player['cojvrIndexedBusy'])
                self.assertTrue(vm.player['cojvrIndexedFaulted'])
                vm.hook = None; before = vm.vector(); count = len(vm.calls)
                self.assertFalse(vm.insert())
                self.assertEqual(vm.vector(), before)
                self.assertEqual(len(vm.calls), count, 'Quarantined call must not invoke virtual reads')
                if boundary == 'UpdateAmmoSkin': self.assertEqual(before, ((0, 1), (10, 10, 10)))
                if boundary in ('SetInternalAmount', 'SetAmount'):
                    self.assertEqual(before, ((0, 1), (10, 10, 9)))

    def test_setter_owner_or_vector_changes_quarantine_before_reserve_debit(self):
        for change in ('owner', 'machine', 'counter', 'other_slot', 'other_pool', 'capacity', 'type'):
            with self.subTest(change=change):
                vm = self.make()
                def hook(when, name, owner, args):
                    if when == 'after' and name == 'UpdateAmmoSkin':
                        if change == 'owner': vm.slots[0][0] = {}
                        if change == 'machine': vm.machine = {'class': 'PlayerStateMashine'}
                        if change == 'counter': vm.counters[2] = dict(vm.counters[2])
                        if change == 'other_slot': vm.weapon['m_iAmmo0Count'] = 1
                        if change == 'other_pool': vm.player['nAmmoShotgun'] = 9
                        if change == 'capacity': vm.capacities[1] = 7
                        if change == 'type': vm.types[1] = 1
                vm.hook = hook
                self.assertFalse(vm.insert())
                self.assertTrue(vm.player['cojvrIndexedFaulted'])
                self.assertFalse(vm.player['cojvrIndexedBusy'])
                self.assertEqual(vm.player['nAmmoPistol'], 10)

    def test_early_readback_changes_are_not_committed(self):
        vm = self.make(); seen = [0]
        def hook(when, name, owner, args):
            if when == 'after' and name == 'GetAmount':
                seen[0] += 1
                if seen[0] == 3: vm.player['nAmmoRifle'] -= 1
        vm.hook = hook
        self.assertFalse(vm.insert())
        self.assertEqual(vm.vector()[0], (0, 0))
        self.assertEqual(vm.skin_calls, 0)
        self.assertTrue(vm.player['cojvrIndexedFaulted'])

    def test_transient_selected_type_cannot_choose_a_different_retained_pool(self):
        vm = self.make(); base = vm.native; observed = [0]
        def native(owner, name, args):
            result = base(owner, name, args)
            if name == 'GetAmmoType' and args == [1]:
                observed[0] += 1
                # Initial profile read is correct; capture alone returns rifle.
                if observed[0] == 2: return 0
            return result
        vm.native = native
        self.assertFalse(vm.insert())
        self.assertTrue(vm.player['cojvrIndexedFaulted'])
        self.assertEqual(vm.vector(), ((0, 0), (10, 10, 10)))

    def test_transient_capacity_does_not_admit_overfull_other_or_full_selected(self):
        for index, amount in ((0, 2), (1, 8)):
            with self.subTest(index=index):
                vm=self.make()
                vm.weapon['m_iAmmo'+str(index)+'Count']=amount
                reads=[0]
                def hook(when,name,owner,args):
                    if when=='after' and name=='GetAmmoCapacity' and args==[index]:
                        reads[0]+=1
                        if reads[0]==1:vm.capacities[index]+=1
                        if reads[0]==(2 if index==0 else 3):vm.capacities[index]-=1
                vm.hook=hook
                before=vm.vector()
                self.assertFalse(vm.insert(1))
                self.assertEqual(vm.vector(),before)
                self.assertEqual(vm.skin_calls,0,'A rejected snapshot must not reach the loaded setter')
                self.assertFalse(vm.player['cojvrIndexedBusy'])

    def test_zero_reserve_full_invalid_or_null_inputs_never_write(self):
        for kind in PROFILES:
            for index in (0, 1):
                for full in (False, True):
                    vm = self.make(kind)
                    if full: vm.weapon['m_iAmmo'+str(index)+'Count'] = PROFILES[kind][0][index]
                    else: vm.player['nAmmo'+('Rifle','Shotgun','Pistol')[PROFILES[kind][1][index]]] = 0
                    before = vm.vector()
                    self.assertFalse(vm.insert(index))
                    self.assertEqual(vm.vector(), before)
                    self.assertEqual(vm.skin_calls, 0)
        vm = self.make()
        for weapon, hand, index in ((None, 0, 0), (vm.weapon,-1,0), (vm.weapon,2,0),
                                    (vm.weapon,0,-1), (vm.weapon,0,2)):
            count = len(vm.calls)
            self.assertFalse(vm.run('ArmedPlayerBeing','cojvrLabInsertIndexedUnit','(LWeapon;II)Z',
                                    vm.player,[weapon,hand,index]))
            self.assertEqual(len(vm.calls),count)

    def test_other_slot_preserved_at_capacity_and_pistol_slot_can_fill_sequentially(self):
        for kind in PROFILES:
            vm = self.make(kind); vm.weapon['m_iAmmo0Count'] = 1
            for amount in range(1, PROFILES[kind][0][1]+1):
                self.assertTrue(vm.insert(1))
                self.assertEqual(vm.vector()[0], (1, amount))
                self.assertEqual(vm.skin_calls, amount)
            before = vm.vector()
            self.assertFalse(vm.insert(1))
            self.assertEqual(vm.vector(), before)

    def test_post_reserve_side_effect_is_quarantined_without_compensating_writes(self):
        for change in ('other_slot', 'other_pool', 'counter_max', 'counter_owner', 'owner'):
            with self.subTest(change=change):
                vm = self.make()
                def hook(when, name, owner, args):
                    if when == 'after' and name == 'SetInternalAmount':
                        if change == 'other_slot': vm.weapon['m_iAmmo0Count'] = 1
                        if change == 'other_pool': vm.player['nAmmoRifle'] -= 1
                        if change == 'counter_max': vm.counters[2]['nMaxAmount'] -= 1
                        if change == 'counter_owner': vm.counters[2]['cOwner'] = {}
                        if change == 'owner': vm.weapon['cOwner'] = {}
                vm.hook = hook
                self.assertFalse(vm.insert())
                self.assertTrue(vm.player['cojvrIndexedFaulted'])
                self.assertFalse(vm.player['cojvrIndexedBusy'])
                self.assertEqual(vm.player['nAmmoPistol'], 9)
                count = len(vm.calls); before = vm.vector()
                self.assertFalse(vm.insert())
                self.assertEqual(len(vm.calls),count)
                self.assertEqual(vm.vector(),before)

    def test_unknown_archive_and_repeated_derivative_reject(self):
        emit = self.emitter()
        with self.assertRaises(ValueError): emit(original_archive() + b'changed')
        with self.assertRaises(ValueError): emit(emit(original_archive()))
        with zipfile.ZipFile(ARCHIVE) as z:
            _, originals = parse_class(z.read('ArmedPlayerBeing.class'))
        _, patched = parse_class(emit(original_archive()))
        methods = {(m.name, m.descriptor): m for m in patched}
        for method in originals:
            self.assertEqual(method, methods[method.name, method.descriptor], 'Original methods changed')

    def test_emission_has_no_policy_parameter_animation_or_event_mutator(self):
        pool, methods = parse_class(self.emitter()(original_archive()))
        helpers = [m for m in methods if m.name.startswith('cojvrIndexed') or m.name == 'cojvrLabInsertIndexedUnit']
        self.assertEqual(len(helpers), 3)
        for helper in helpers:
            for line in disassemble(helper.code, pool):
                if 'putfield' in line:
                    self.assertTrue('ArmedPlayerBeing.cojvrIndexedBusyZ' in line or
                                    'ArmedPlayerBeing.cojvrIndexedFaultedZ' in line, line)
                if 'invoke' in line:
                    for forbidden in ('WeaponReload(', 'ReloadInside(', 'ReloadForce(', 'ReloadRotate(',
                                      'SetCurrentState(', 'SetDesiredWeaponState(', 'OnGameEvent(', 'ExecSound('):
                        self.assertNotIn(forbidden, line)

    def test_silent_setter_failure_and_reserve_clamping_cannot_publish_success(self):
        for fault in ('loaded_noop', 'reserve_noop', 'reserve_clamp'):
            vm = self.make(); base = vm.native
            def native(owner, name, args):
                if fault == 'loaded_noop' and name == 'SetAmmoCount': return None
                if fault == 'reserve_noop' and name == 'SetAmount': return None
                if fault == 'reserve_clamp' and name == 'SetAmount': owner['nMaxAmount'] = 5
                return base(owner, name, args)
            vm.native = native
            self.assertFalse(vm.insert())
            self.assertTrue(vm.player['cojvrIndexedFaulted'])
            self.assertFalse(vm.player['cojvrIndexedBusy'])
            self.assertEqual(vm.vector()[0], (0, 0) if fault == 'loaded_noop' else (0, 1))
            self.assertEqual(vm.player['nAmmoPistol'], 5 if fault == 'reserve_clamp' else 10)

    def test_every_observed_virtual_boundary_rethrows_and_retires_busy(self):
        template = self.make(); observed = []
        baseline = self.make()
        baseline.hook = lambda when, name, owner, args: observed.append((when, name))
        self.assertTrue(baseline.insert())
        self.assertGreater(len(observed), 100, 'Fault sweep lost the full transaction')
        for boundary, label in enumerate(observed):
            with self.subTest(boundary=boundary, label=label):
                vm = copy.copy(template)
                # Copy mutable modeled native state together to preserve object
                # identity/owner aliases. Bytecode/class inventories are read-only.
                state = copy.deepcopy({key: getattr(template, key) for key in (
                    'player', 'weapon', 'machine', 'slots', 'counters', 'types', 'capacities')})
                for key, value in state.items(): setattr(vm, key, value)
                vm.calls, vm.writes, vm.frames = [], [], []
                exception = {'class': 'Throwable', 'at': boundary}; seen = [0]
                def hook(when, name, owner, args):
                    current = seen[0]; seen[0] += 1
                    if current == boundary: raise JVMThrown(exception)
                vm.hook = hook
                with self.assertRaises(JVMThrown) as caught: vm.insert()
                self.assertIs(caught.exception.value, exception)
                self.assertTrue(vm.player['cojvrIndexedFaulted'])
                self.assertFalse(vm.player['cojvrIndexedBusy'])
                before = vm.vector(); count = len(vm.calls)
                self.assertFalse(vm.insert())
                self.assertEqual(vm.vector(), before)
                self.assertEqual(len(vm.calls), count)

    def test_parameter_discovery_cancellation_rechecks_full_authority_before_writer(self):
        for change in ('body', 'desired', 'actual', 'network', 'netowner', 'carrying', 'weapon_owner'):
            with self.subTest(change=change):
                vm = self.make(); reads = [0]
                def hook(when, name, owner, args):
                    if when == 'after' and name == 'GetAmmoCapacity':
                        reads[0] += 1
                        # Two reads in initial exact profile. Cancel during
                        # subsequent snapshot discovery, after initial owner proof.
                        if reads[0] == 3:
                            if change == 'body': vm.current_state = 22
                            if change == 'desired': vm.desired_state = 1
                            if change == 'actual': vm.slots[vm.hand][0] = None
                            if change == 'network': vm.net_game = True
                            if change == 'netowner': vm.net_owner = False
                            if change == 'carrying': vm.carrying = True
                            if change == 'weapon_owner': vm.weapon['cOwner'] = {}
                vm.hook = hook
                self.assertFalse(vm.insert())
                self.assertEqual(vm.vector(), ((0,0),(10,10,10)))
                self.assertEqual(vm.skin_calls, 0)
                self.assertTrue(vm.player['cojvrIndexedFaulted'])
                self.assertFalse(vm.player['cojvrIndexedBusy'])

    def test_counter_replaced_by_final_owner_callback_is_rejected_before_next_writer(self):
        for replace_at in (6, 10):
            vm = self.make(); reads = [0]
            def hook(when, name, owner, args):
                if when == 'after' and name == 'GetDesiredWeapon':
                    reads[0] += 1
                    if reads[0] == replace_at: vm.counters[2] = dict(vm.counters[2])
            vm.hook = hook
            self.assertFalse(vm.insert())
            self.assertEqual(vm.vector()[0], (0,0) if replace_at == 6 else (0,1))
            self.assertEqual(vm.player['nAmmoPistol'], 10)
            self.assertTrue(vm.player['cojvrIndexedFaulted'])

    def test_emitted_field_access_respects_original_private_and_package_visibility(self):
        derivative = self.emitter()(original_archive())
        pool, methods = parse_class(derivative)
        fields, parents = {}, {}
        with zipfile.ZipFile(ARCHIVE) as archive:
            for member in archive.namelist():
                if not member.endswith('.class'): continue
                name = member[:-6]
                data = derivative if name == 'ArmedPlayerBeing' else archive.read(member)
                reader = Reader(data); reader.read(8); constants = parse_constant_pool(reader)
                reader.u2(); reader.u2(); parent = reader.u2()
                parents[name] = constants.describe(parent) if parent else None
                reader.read(reader.u2() * 2)
                for _ in range(reader.u2()):
                    access = reader.u2(); key = constants.utf8(reader.u2()); desc = constants.utf8(reader.u2())
                    fields[name, key, desc] = access
                    for _ in range(reader.u2()): reader.u2(); reader.read(reader.u4())
        for method in methods:
            if not (method.name.startswith('cojvrIndexed') or method.name == 'cojvrLabInsertIndexedUnit'): continue
            for line in disassemble(method.code, pool):
                if 'getfield' not in line and 'putfield' not in line: continue
                entry = pool.get(int(re.search(r'#(\d+)', line).group(1)))
                owner = pool.describe(entry[1]); nt = pool.get(entry[2])
                key, desc = pool.utf8(nt[1]), pool.utf8(nt[2])
                while (owner,key,desc) not in fields:
                    self.assertIn(owner, parents, line); owner = parents[owner]
                access = fields[owner,key,desc]
                self.assertFalse(access & 2 and owner != 'ArmedPlayerBeing',
                                 'Illegal private field reference from emitted helper: ' + line)
                if not access & 1:
                    self.assertEqual(owner.rpartition('/')[0], '', 'Non-public field is outside default package')


if __name__ == '__main__': unittest.main()
