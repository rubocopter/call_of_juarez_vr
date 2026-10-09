"""Opt-in, bounded CoJ reload wait/zero-transfer experiment.

This is NOT the manual reload feature. It exercises the exact pre-Clear seam
without granting ammunition credits or taking ownership of tracked presentation.
Only the canonical patcher may install it from hash-verified original classes.
"""
import hashlib
import struct
from inspect_java_bytecode import Reader, parse_constant_pool, disassemble
from patch_coj_weapon_consumers import Code, u2, u4, insert_target_tag, patch_class

STATE_SHA256 = 'd8bc41b303ee2b419c42dd3f93318c1f76e1c4ba1b51d2df824c4b846d2af8f8'


class ClassEdit:
    """Retain all untouched members and attributes of a Java 1.4 class."""
    def __init__(self, data):
        self.data = data
        r = self.r = Reader(data); r.read(8)
        self.pool = parse_constant_pool(r); self.cp_end = r.stream.tell()
        self.extra = bytearray(); self.next = len(self.pool.entries)
        self.refs = {self.pool.describe(i): i for i, e in enumerate(self.pool.entries) if e is not None and i}
        r.u2(); self.this = r.u2(); r.u2(); r.read(r.u2() * 2)
        self.fields_at = r.stream.tell(); self.fields_count = r.u2()
        for _ in range(self.fields_count): self.member()
        self.fields_end = r.stream.tell()
        self.fields = bytearray(); self.helpers = []; self.changes = {}

    def member(self):
        r = self.r; start = r.stream.tell(); r.read(6)
        for _ in range(r.u2()): r.u2(); r.read(r.u4())
        return self.data[start:r.stream.tell()]

    def append(self, raw):
        i = self.next; self.next += 1; self.extra.extend(raw); return i

    def utf8(self, text):
        key = repr(text)
        if key not in self.refs:
            raw = text.encode('utf-8'); self.refs[key] = self.append(b'\x01' + u2(len(raw)) + raw)
        return self.refs[key]

    def cls(self, owner):
        if owner not in self.refs: self.refs[owner] = self.append(b'\x07' + u2(self.utf8(owner)))
        return self.refs[owner]

    def ref(self, owner, name, desc, tag=10):
        key = owner + '.' + name + desc
        if key not in self.refs:
            nt = self.append(b'\x0c' + u2(self.utf8(name)) + u2(self.utf8(desc)))
            self.refs[key] = self.append(bytes([tag]) + u2(self.cls(owner)) + u2(nt))
        return self.refs[key]

    def field(self, name, desc):
        owner = self.pool.describe(self.this)
        if owner + '.' + name + desc in self.refs: raise ValueError('Probe field already exists')
        self.fields.extend(u2(0x0081) + u2(self.utf8(name)) + u2(self.utf8(desc)) + u2(0))
        return self.ref(owner, name, desc, 9)

    def helper(self, name, desc, code, locals_):
        attr = u2(8) + u2(locals_) + u4(len(code)) + code + u2(0) + u2(0)
        self.helpers.append(u2(1) + u2(self.utf8(name)) + u2(self.utf8(desc)) + u2(1) +
                            u2(self.utf8('Code')) + u4(len(attr)) + attr)

    def finish(self):
        r = self.r; count = r.u2(); methods = bytearray(u2(count + len(self.helpers))); found = set()
        for _ in range(count):
            header = r.read(6); methods.extend(header)
            target = (self.pool.utf8(int.from_bytes(header[2:4], 'big')),
                      self.pool.utf8(int.from_bytes(header[4:6], 'big')))
            n = r.u2(); methods.extend(u2(n))
            for _ in range(n):
                index = r.u2(); attr = r.read(r.u4())
                if self.pool.utf8(index) == 'Code' and target in self.changes:
                    cr = Reader(attr); stack, locals_ = cr.u2(), cr.u2(); code = cr.read(cr.u4())
                    if cr.u2(): raise ValueError('Unexpected probe target exception table')
                    code = self.changes[target](code)
                    attr = u2(max(stack, 8)) + u2(max(locals_, 5)) + u4(len(code)) + code + u2(0) + u2(0)
                    found.add(target)
                methods.extend(u2(index) + u4(len(attr)) + attr)
        if found != set(self.changes): raise ValueError('Incomplete reload probe patch')
        for helper in self.helpers: methods.extend(helper)
        return (self.data[:8] + u2(self.next) + self.data[10:self.cp_end] + self.extra +
                self.data[self.cp_end:self.fields_at] + u2(self.fields_count + len(self.fields) // 8) +
                self.data[self.fields_at+2:self.fields_end] + self.fields + methods + self.data[r.stream.tell():])


def patch_probe_classes(original_player, original_machine):
    # patch_class owns the original ArmedPlayerBeing hash check. Never augment
    # unknown or already modified input. The second class is checked before any
    # output is created by the archive patcher.
    if hashlib.sha256(original_machine).hexdigest() != STATE_SHA256:
        raise ValueError('Unknown StateMashine.class SHA-256; no mutation')
    e = ClassEdit(patch_class(original_player))
    f = {name: e.field('cojvrReloadProbe' + name, desc) for name, desc in (
        ('Weapon', 'LWeapon;'), ('Hand', 'I'), ('Machine', 'LPlayerStateMashine;'),
        ('Until', 'F'), ('Since', 'F'), ('Cancelled', 'Z'), ('Status', 'I'))}
    owner_field = e.ref('StateMashine', 'cojvrReloadProbeOwner', 'LArmedPlayerBeing;', 9)
    def method(name, desc): return e.ref('ArmedPlayerBeing', name, desc)
    get_machine = method('GetHandStateMashine', '(I)LPlayerStateMashine;')
    actual = method('GetActualWeapon', '(I)LWeapon;')
    desired = method('GetDesiredWeapon', '(I)LWeapon;')
    desired_state = method('GetDesiredWeaponState', '(I)I')
    get_move = method('GetWeaponMoveState', '(LWeapon;)I')
    set_desired = method('SetDesiredWeaponState', '(II)V')
    find_state = e.ref('PlayerStateMashine', 'FindStateByID', '(I)LPlayerState;')
    current = e.ref('PlayerStateMashine', 'GetCurrentStateID', '()I')
    set_current = e.ref('PlayerStateMashine', 'SetCurrentState', '(LBaseState;)V')
    clock = e.ref('PlayerStateMashine', 'm_fCurrentTime', 'F', 9)
    wait = e.append(b'\x04' + struct.pack('>f', 1.5))
    active = e.ref('ArmedPlayerBeing', 'cojvrSingleRoundWeapon', 'LWeapon;', 9)
    active_hand = e.ref('ArmedPlayerBeing', 'cojvrSingleRoundHand', 'I', 9)
    completed = e.ref('ArmedPlayerBeing', 'cojvrSingleRoundCompleted', 'Z', 9)

    def read(c, key): c.emit(0x2a); c.ref(0xb4, f[key])
    def status(c, n): c.emit(0x2a, 0x10, n); c.ref(0xb5, f['Status'])
    def set_move(c):
        # Only the still-owned weapon's reload destination may be changed.
        read(c, 'Weapon'); c.emit(0x2a); read(c, 'Hand'); c.ref(0xb6, actual); c.branch(0xa6, 'move_done')
        read(c, 'Weapon'); c.emit(0x2a); read(c, 'Hand'); c.ref(0xb6, desired); c.branch(0xa6, 'move_done')
        c.emit(0x2a); read(c, 'Hand'); c.ref(0xb6, desired_state); c.emit(0x10, 21); c.branch(0xa0, 'move_done')
        c.emit(0x2a); read(c, 'Hand'); c.emit(0x2a); read(c, 'Weapon'); c.ref(0xb6, get_move); c.ref(0xb6, set_desired)
        c.label('move_done')

    # Begin only after the existing admission has published a bounded ticket.
    # Cache/validate the exact machine and closing state before attaching a veto.
    c = Code(); c.emit(0x2a); c.ref(0xb4, active); c.branch(0xc6, 'ordinary')
    c.emit(0x2a); c.ref(0xb4, active_hand); c.emit(0x1b); c.branch(0xa0, 'ordinary')
    c.emit(0x2a); c.ref(0xb4, completed); c.branch(0x9a, 'done')
    c.emit(0x2a, 0x1b); c.ref(0xb6, get_machine); c.emit(0x4d, 0x2c); c.branch(0xc6, 'done')
    c.emit(0x2c, 0x10, 22); c.ref(0xb6, find_state); c.branch(0xc6, 'done')
    c.emit(0x2c); c.ref(0xb4, owner_field); c.branch(0xc7, 'done')
    read(c, 'Weapon'); c.branch(0xc7, 'done')
    c.emit(0x2a, 0x1b); c.ref(0xb5, f['Hand'])
    c.emit(0x2a, 0x2c); c.ref(0xb5, f['Machine'])
    c.emit(0x2a, 0x0b); c.ref(0xb5, f['Until'])
    c.emit(0x2a, 0x0b); c.ref(0xb5, f['Since'])
    c.emit(0x2a, 0x03); c.ref(0xb5, f['Cancelled'])
    c.emit(0x2a, 0x2a); c.ref(0xb4, active); c.ref(0xb5, f['Weapon'])
    status(c, 1); c.emit(0x2c, 0x2a); c.ref(0xb5, owner_field)
    c.label('ordinary'); c.emit(0x04, 0xac)
    c.label('done'); c.emit(0x03, 0xac)
    e.helper('cojvrBeginReloadProbe', '(I)Z', c.finish(), 3)

    # Pre-Clear veto is reached only after the game's CanChangeState approves.
    c = Code(); read(c, 'Weapon'); c.branch(0xc6, 'allow')
    read(c, 'Machine'); c.emit(0x2b); c.branch(0xa6, 'allow')
    read(c, 'Machine'); c.ref(0xb6, current); c.emit(0x10, 21); c.branch(0xa0, 'allow')
    read(c, 'Cancelled'); c.branch(0x9a, 'close')
    c.emit(0x2a); c.ref(0xb6, method('IsNotAlive', '()Z')); c.branch(0x9a, 'close')
    c.emit(0x2a); c.ref(0xb4, e.ref('ArmedPlayerBeing', 'm_bNetAttackForced', 'Z', 9)); c.branch(0x9a, 'close')
    c.emit(0x2a); c.ref(0xb6, method('GetNetIsOwner', '()Z')); c.branch(0x99, 'close')
    c.ref(0xb8, method('IsNetGame', '()Z')); c.branch(0x9a, 'close')
    for getter in (actual, desired):
        c.emit(0x2a); read(c, 'Hand'); c.ref(0xb6, getter); read(c, 'Weapon'); c.branch(0xa6, 'close')
    c.emit(0x2a); read(c, 'Hand'); c.ref(0xb6, desired_state); c.emit(0x10, 21); c.branch(0xa0, 'close')
    read(c, 'Until'); c.emit(0x0b, 0x95); c.branch(0x9a, 'deadline')
    c.emit(0x2a); read(c, 'Machine'); c.ref(0xb4, clock); c.ref(0xb5, f['Since'])
    c.emit(0x2a); read(c, 'Machine'); c.ref(0xb4, clock); c.ref(0x13, wait); c.emit(0x62); c.ref(0xb5, f['Until'])
    status(c, 2)
    c.label('deadline')
    read(c, 'Machine'); c.ref(0xb4, clock); read(c, 'Since'); c.emit(0x95); c.branch(0x9b, 'close')
    read(c, 'Machine'); c.ref(0xb4, clock); read(c, 'Until'); c.emit(0x96); c.branch(0x9c, 'close')
    c.emit(0x03, 0xac)
    c.label('close'); status(c, 3); c.emit(0x2a, 0x04); c.ref(0xb5, f['Cancelled']); set_move(c)
    c.label('allow'); c.emit(0x04, 0xac)
    e.helper('cojvrAllowReloadProbeTransition', '(LStateMashine;)Z', c.finish(), 2)

    # Explicit cancellation uses the audited native state setter and retains
    # Clear/Set callbacks. It never invokes Update/reload or writes ammo.
    c = Code(); read(c, 'Weapon'); c.branch(0xc6, 'done')
    c.emit(0x2a, 0x04); c.ref(0xb5, f['Cancelled']); status(c, 3); set_move(c)
    read(c, 'Machine'); c.emit(0x4c, 0x2b); c.branch(0xc6, 'done')
    c.emit(0x2b); c.ref(0xb6, current); c.emit(0x3d)
    c.emit(0x1c, 0x10, 20); c.branch(0x9f, 'end')
    c.emit(0x1c, 0x10, 21); c.branch(0xa0, 'done')
    c.label('end'); c.emit(0x2b, 0x10, 22); c.ref(0xb6, find_state); c.emit(0x4e, 0x2d); c.branch(0xc6, 'done')
    c.emit(0x2b, 0x2d); c.ref(0xb6, set_current)
    c.label('done'); c.emit(0xb1)
    e.helper('cojvrCancelReloadProbe', '()V', c.finish(), 4)

    c = Code(); read(c, 'Weapon'); c.branch(0xc6, 'done')
    read(c, 'Hand'); c.emit(0x1b); c.branch(0xa0, 'done')
    read(c, 'Machine'); c.emit(0x4d, 0x2c); c.branch(0xc6, 'clear')
    c.emit(0x2c); c.ref(0xb4, owner_field); c.emit(0x2a); c.branch(0xa6, 'clear')
    c.emit(0x2c, 0x01); c.ref(0xb5, owner_field)
    c.label('clear'); c.emit(0x2a, 0x01); c.ref(0xb5, f['Weapon'])
    c.emit(0x2a, 0x01); c.ref(0xb5, f['Machine']); c.emit(0x2a, 0x0b); c.ref(0xb5, f['Until'])
    status(c, 4); c.label('done'); c.emit(0xb1)
    e.helper('cojvrClearReloadProbe', '(I)V', c.finish(), 3)

    # Native replacement can discard a machine without a ReloadEnd callback.
    # Finish its old body under zero-transfer ownership before releasing it.
    c = Code(); read(c, 'Weapon'); c.branch(0xc6, 'done')
    read(c, 'Hand'); c.emit(0x1b); c.branch(0xa0, 'done')
    read(c, 'Machine'); c.emit(0x2a, 0x1b); c.ref(0xb6, get_machine); c.branch(0xa6, 'orphan')
    read(c, 'Machine'); c.ref(0xb6, current); c.emit(0x3d)
    for n in (20, 21, 22):
        c.emit(0x1c, 0x10, n); c.branch(0x9f, 'done')
    c.emit(0x2a, 0x1b); c.ref(0xb6, desired_state); c.emit(0x10, 21); c.branch(0x9f, 'done')
    c.label('orphan'); c.emit(0x2a); c.ref(0xb6, method('cojvrCancelReloadProbe', '()V'))
    c.emit(0x2a, 0x1b); c.ref(0xb6, method('cojvrClearReloadProbe', '(I)V'))
    c.label('done'); c.emit(0xb1)
    e.helper('cojvrCheckReloadProbeOwner', '(I)V', c.finish(), 3)

    def prepend_call(code, name, desc, hand=False):
        c = Code(); c.emit(0x2a)
        if hand: c.emit(0x1b)
        c.ref(0xb6, method(name, desc)); tag = c.finish()
        # Preserve any original switch alignment and relative branch offsets.
        return tag + b'\x00' * (-len(tag) % 4) + code

    def request(code):
        setter = method('SetDesiredWeaponState', '(II)V')
        boundaries = [int(s.split(':')[0]) for s in disassemble(code, e.pool)]
        calls = [at for at in boundaries if code[at:at+3] == b'\xb6' + u2(setter)]
        if len(calls) != 1: raise ValueError('Unknown reload probe admission seam')
        # Insert before the three setter arguments, after all existing admission
        # branches (branches reaching the original argument start include tag).
        c = Code(); c.emit(0x2a, 0x1b); c.ref(0xb6, method('cojvrBeginReloadProbe', '(I)Z'))
        c.branch(0x9a, 'native')
        # Failed probe admission must not silently fall through to a real round.
        c.emit(0x2a, 0x01); c.ref(0xb5, e.ref('ArmedPlayerBeing', 'cojvrManualReloadWeapon', 'LWeapon;', 9))
        c.emit(0x2a, 0x01); c.ref(0xb5, active)
        c.emit(0x2a, 0x03); c.ref(0xb5, completed); c.emit(0xb1)
        c.label('native')
        return insert_target_tag(code, calls[0] - 3, c.finish(), e.pool, include_target=True)

    def completion(code):
        native = b'\x2a\x19\x04\xb6' + u2(method('WeaponReload', '(LWeapon;)Z')) + b'\x36\x07'
        if code.count(native) != 1: raise ValueError('Unknown reload probe transfer seam')
        at = code.index(native)
        c = Code(); read(c, 'Weapon'); c.branch(0xc6, 'native')
        read(c, 'Hand'); c.emit(0x1b); c.branch(0xa0, 'native')
        c.emit(0x03, 0x36, 7); c.branch(0xa7, 'after')
        c.label('native'); c.labels['after'] = len(c.data) + len(native)
        return insert_target_tag(code, at, c.finish(), e.pool, include_target=True)

    e.changes[('ReloadWeapon', '(I)V')] = request
    e.changes[('OnHandStateFinished_Reload', '(IIZ)V')] = completion
    e.changes[('OnHandStateFinished_ReloadEnd', '(IIZ)V')] = lambda code: prepend_call(code, 'cojvrClearReloadProbe', '(I)V', True)
    e.changes[('UpdateHandState', '(IZ)V')] = lambda code: prepend_call(code, 'cojvrCheckReloadProbeOwner', '(I)V', True)
    e.changes[('SGSaveChunk', '(LFileChunk;)V')] = lambda code: prepend_call(code, 'cojvrCancelReloadProbe', '()V')
    def load(code):
        c = Code(); c.emit(0x2a); c.ref(0xb6, method('cojvrCancelReloadProbe', '()V'))
        c.emit(0x2a); read(c, 'Hand'); c.ref(0xb6, method('cojvrClearReloadProbe', '(I)V'))
        tag = c.finish(); return tag + b'\x00' * (-len(tag) % 4) + code
    e.changes[('SGLoadChunk', '(LFileChunk;)V')] = load
    player = e.finish()

    e = ClassEdit(original_machine)
    owner = e.field('cojvrReloadProbeOwner', 'LArmedPlayerBeing;')
    allow = e.ref('ArmedPlayerBeing', 'cojvrAllowReloadProbeTransition', '(LStateMashine;)Z')
    def update(code):
        if len(code) != 73 or code[60:66] != bytes.fromhex('2a 19 04 b6 00 06'):
            raise ValueError('Unknown StateMashine pre-Clear seam')
        c = Code(); c.emit(0x2a); c.ref(0xb4, owner); c.branch(0xc6, 'native')
        c.emit(0x2a); c.ref(0xb4, owner); c.emit(0x2a); c.ref(0xb6, allow); c.branch(0x9a, 'native')
        c.emit(0x1b, 0xac); c.label('native')
        return insert_target_tag(code, 60, c.finish(), e.pool, include_target=True)
    e.changes[('Update', '()Z')] = update
    return player, e.finish()
