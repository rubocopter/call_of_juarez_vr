"""Exact Schofield opening and held ammunition return for explicit dispatch."""
import hashlib
import io
import zipfile

from coj_indexed_reload_session import _emit_retained_wait
from coj_reload_cycle import _retained_completion_veto
from coj_reload_wait_probe import ClassEdit
from patch_coj_weapon_consumers import Code, PAK_SHA256, insert_target_tag, patch_manual_reload_classes
from inspect_java_bytecode import Reader, parse_constant_pool


DUAL_FIELDS = {'Weapon': 'LWeapon;', 'Machine': 'LPlayerStateMashine;', 'Hand': 'I',
               'Partner': 'LWeapon;', 'PartnerMachine': 'LPlayerStateMashine;', 'Body': 'LBaseState;'}
AMMO_CLASS = 'WeaponFireAmmo'
SPENT_FIELD = 'cojvrSchofieldSpent'
AMMO_CLASS_SHA256 = 'dc6874a068ffcf0126ae351f9fed8292c70431c083409c76ee552428599f847e'


def patch_schofield_case_classes(fire: bytes, ammo: bytes) -> tuple[bytes, bytes]:
    """Track only confirmed player slot-zero debits at the original shot call."""
    from coj_clip_reload import CLIP_WEAPONS
    from coj_manual_reload import _exact_class_branch

    if hashlib.sha256(ammo).hexdigest() != AMMO_CLASS_SHA256:
        raise ValueError('Unknown WeaponFireAmmo.class SHA-256; no mutation')
    ammo_edit = ClassEdit(ammo)
    spent = ammo_edit.field(SPENT_FIELD, 'I')
    loaded = ammo_edit.ref(AMMO_CLASS, 'm_iAmmo0Count', 'I', 9)
    store = ammo_edit.ref(AMMO_CLASS, 'cojvrStoreAmmoZero', '(I)V')
    c = Code()
    c.emit(0x2a); c.ref(0xb4, loaded)
    c.emit(0x1b); c.branch(0x9f, 'store')
    # Stock reloads, inventory transfers and other writers retire the shadow.
    c.emit(0x2a, 0x03); c.ref(0xb5, spent)
    c.label('store')
    c.emit(0x2a, 0x1b); c.ref(0xb5, loaded); c.emit(0xb1)
    ammo_edit.helper('cojvrStoreAmmoZero', '(I)V', c.finish(), 2)

    def redirect(code, offset, expected_size, old, target):
        if len(code) != expected_size or code[offset:offset+3] != old:
            raise ValueError('Unknown Schofield case storage boundary')
        return code[:offset] + b'\xb6' + target.to_bytes(2, 'big') + code[offset+3:]

    ammo_edit.changes['SetAmmoCount', '(II)V'] = lambda code: redirect(
        code, 25, 35, b'\xb5\x00\x02', store)

    fire_edit = ClassEdit(fire)
    weapon = 'WeaponFire'
    setter = fire_edit.ref(weapon, 'SetAmmoCount', '(II)V')
    spent = fire_edit.ref(AMMO_CLASS, SPENT_FIELD, 'I', 9)
    loaded = fire_edit.ref(AMMO_CLASS, 'm_iAmmo0Count', 'I', 9)
    debit = fire_edit.ref(weapon, 'cojvrDebitShotAmmo', '(II)V')
    c = Code()
    c.emit(0x1b); c.branch(0x9a, 'stock')
    c.emit(0x1c); c.branch(0x9b, 'stock')
    c.emit(0x1c, 0x10, 6); c.branch(0xa2, 'stock')
    c.emit(0x2a); c.ref(0xb4, fire_edit.ref(weapon, 'cOwner', 'LPawnInventory;', 9))
    c.ref(0xc1, fire_edit.cls('ArmedPlayerBeing')); c.branch(0x99, 'stock')
    _exact_class_branch(fire_edit, c, lambda code: code.emit(0x2a), CLIP_WEAPONS, 'exact')
    c.branch(0xa7, 'stock')
    c.label('exact')
    c.emit(0x2a); c.ref(0xc0, fire_edit.cls(AMMO_CLASS)); c.emit(0x4e)
    c.emit(0x2d); c.ref(0xb4, loaded)
    c.emit(0x1c, 0x04, 0x60); c.branch(0xa0, 'stock')
    c.emit(0x2d); c.ref(0xb4, spent); c.emit(0x36, 4)
    c.emit(0x2a, 0x1b, 0x1c); c.ref(0xb6, setter)
    c.emit(0x2d); c.ref(0xb4, loaded); c.emit(0x1c); c.branch(0xa0, 'done')
    c.emit(0x2d, 0x10, 6, 0x1c, 0x64, 0x15, 4, 0x04, 0x60)
    c.ref(0xb8, fire_edit.ref('java/lang/Math', 'min', '(II)I'))
    c.ref(0xb5, spent)
    c.label('done'); c.emit(0xb1)
    c.label('stock')
    c.emit(0x2a, 0x1b, 0x1c); c.ref(0xb6, setter); c.emit(0xb1)
    fire_edit.helper('cojvrDebitShotAmmo', '(II)V', c.finish(), 5)
    fire_edit.changes['AttackInside', '(ZIZ)V'] = lambda code: redirect(
        code, 190, 392, b'\xb6\x00\x12', debit)
    return fire_edit.finish(), ammo_edit.finish()


def dual_field(e, name):
    return e.ref('ArmedPlayerBeing', 'cojvrDualSchofield' + name, DUAL_FIELDS[name], 9)


def _append_dry_fire(player):
    from coj_manual_reload import _exact_class_branch, runtime_reload_policy
    from coj_clip_reload import CLIP_WEAPONS
    e,c=ClassEdit(player),Code()
    owner='ArmedPlayerBeing'
    def call(name,desc):c.ref(0xb6,e.ref(owner,name,desc))
    c.emit(0x1b); c.branch(0x9b,'reject')
    c.emit(0x1b,0x04); c.branch(0xa3,'reject')
    c.ref(0xb8,e.ref(owner,'IsNetGame','()Z')); c.branch(0x9a,'reject')
    for name in ('IsNotAlive','IsCarrying','IsWeaponReloading','GetNetIsOwner'):
        c.emit(0x2a); call(name,'()Z'); c.branch(0x99 if name=='GetNetIsOwner' else 0x9a,'reject')
    c.emit(0x2a,0x1b); call('CanAttack','(I)Z'); c.branch(0x99,'reject')
    c.emit(0x2a,0x1b); call('GetActualWeapon','(I)LWeapon;'); c.emit(0x4d)
    c.emit(0x2c); c.branch(0xc6,'reject')
    _exact_class_branch(e,c,lambda code:code.emit(0x2c),CLIP_WEAPONS,'schofield')
    _exact_class_branch(e,c,lambda code:code.emit(0x2c),('WeaponHenry1863',),'henry')
    _exact_class_branch(e,c,lambda code:code.emit(0x2c),tuple(runtime_reload_policy()),'frontier')
    c.branch(0xa7,'reject')
    for label,model in (('schofield','Schofield'),('henry','Henry'),('frontier','Frontier')):
        c.label(label)
        sound=e.append(b'\x08'+e.utf8(f'Data/Sounds/weapon/{model}/{model}_Empty.wav').to_bytes(2,'big'))
        c.ref(0x13,sound); c.emit(0x4e); c.branch(0xa7,'check')
    c.label('check')
    for name in ('GetActualWeapon','GetActiveWeapon','GetDesiredWeapon'):
        c.emit(0x2a,0x1b); call(name,'(I)LWeapon;'); c.emit(0x2c); c.branch(0xa6,'reject')
    c.emit(0x2c); c.ref(0xb4,e.ref('Weapon','cOwner','LPawnInventory;',9)); c.emit(0x2a); c.branch(0xa6,'reject')
    c.emit(0x2a,0x1b); call('GetWeaponAmmoCount','(I)I'); c.branch(0x9a,'reject')
    c.emit(0x2a,0x2d); c.ref(0xb6,e.ref('GameObject','PlaySound2D','(Ljava/lang/String;)I'))
    c.branch(0x9b,'reject'); c.emit(0x04,0xac)
    c.label('reject'); c.emit(0x03,0xac)
    e.helper('cojvrPlayManualDryFire','(I)Z',c.finish(),4)
    return e.finish()


def _append_dual_ticket(player):
    """Retain the pair and the native body independently of the mutable probe."""
    e = ClassEdit(player)
    owner = 'ArmedPlayerBeing'
    hand_machine = owner + '$HandStateMashine'
    hand_state = owner + '$HandState'
    hand_rows = '[LArmedPlayerBeing$HandStateMashine;'
    machine_desc = DUAL_FIELDS['Machine']
    state_desc = 'LArmedPlayerBeing$HandState;'
    weapon_desc = DUAL_FIELDS['Weapon']
    base_state = DUAL_FIELDS['Body']
    for name, desc in DUAL_FIELDS.items():
        e.field('cojvrDualSchofield' + name, desc)

    def field(cls, name, desc):
        c.ref(0xb4, e.ref(cls, name, desc, 9))

    def read(name):
        c.emit(0x2a); c.ref(0xb4, dual_field(e, name))

    def call(name, desc):
        c.ref(0xb6, e.ref(owner, name, desc))

    def slot(hand):
        c.emit(0x2a); field(owner, 'm_aHandStateMashine', hand_rows)
        c.emit(*hand, 0x32)

    def selected_slot():
        c.emit(0x2a); field(owner, 'm_aHandStateMashine', hand_rows)
        read('Hand'); c.emit(0x32)

    def finish_bool(name, desc, locals_):
        c.emit(0x04, 0xac)
        c.label('reject'); c.emit(0x03, 0xac)
        e.helper(name, desc, c.finish(), locals_)

    hand_desc = '(LWeapon;ILPlayerStateMashine;Z)Z'
    c = Code()
    for local in (1, 3):
        c.emit(0x19, local); c.branch(0xc6, 'reject')
    c.emit(0x2b); field('Weapon', 'cOwner', 'LPawnInventory;')
    c.emit(0x2a); c.branch(0xa6, 'reject')
    slot((0x1c,)); c.emit(0x3a, 5, 0x19, 5); c.branch(0xc6, 'reject')
    c.emit(0x19, 5); field(hand_machine, 'm_bTwoHandedMashine', 'Z'); c.branch(0x9a, 'reject')
    c.emit(0x19, 5); field(hand_machine, 'm_cStateMashine', machine_desc)
    c.emit(0x2d); c.branch(0xa6, 'reject')
    for key in ('m_cActualState', 'm_cDesiredState'):
        c.emit(0x19, 5); field(hand_machine, key, state_desc)
        c.emit(0x3a, 6, 0x19, 6); c.branch(0xc6, 'reject')
        c.emit(0x19, 6); field(hand_state, 'm_cWeapon', weapon_desc)
        c.emit(0x2b); c.branch(0xa6, 'reject')
        c.emit(0x19, 6); field(hand_state, 'm_bTwoHanded', 'Z'); c.branch(0x9a, 'reject')
        c.emit(0x15, 4); c.branch(0x99, key + '_checked')
        c.emit(0x19, 6); field(hand_state, 'm_iState', 'I')
        c.emit(0x04); c.branch(0xa0, 'reject')
        c.label(key + '_checked')
    c.emit(0x2a); field(owner, 'ActiveWeapon', '[LWeapon;')
    c.emit(0x1c, 0x32, 0x2b); c.branch(0xa6, 'reject')
    c.emit(0x15, 4); c.branch(0x99, 'ready')
    for key in ('m_sCurrentState', 'm_sDestState'):
        c.emit(0x2d); field('StateMashine', key, base_state)
        c.emit(0x3a, 6, 0x19, 6); c.branch(0xc6, 'reject')
        c.emit(0x19, 6); c.ref(0xc0, e.cls('PlayerState')); field('PlayerState', 'm_iStateID', 'I')
        c.emit(0x04); c.branch(0xa0, 'reject')
    c.label('ready')
    finish_bool('cojvrDualSchofieldHandMatches', hand_desc, 7)

    c = Code()
    for partner in (False, True):
        c.emit(0x2a); read('Partner' if partner else 'Weapon')
        if partner: c.emit(0x04)
        read('Hand')
        if partner: c.emit(0x64)
        read('PartnerMachine' if partner else 'Machine'); c.emit(0x04 if partner else 0x03)
        call('cojvrDualSchofieldHandMatches', hand_desc); c.branch(0x99, 'reject')
    finish_bool('cojvrDualSchofieldPairMatches', '()Z', 1)

    c = Code()
    for name in ('Weapon', 'Hand', 'Machine'):
        c.emit(0x2a); field(owner, 'cojvrReloadProbe' + name, DUAL_FIELDS[name]); read(name)
        c.branch(0xa0 if name == 'Hand' else 0xa6, 'reject')
    c.emit(0x2a); call('cojvrDualSchofieldPairMatches', '()Z'); c.emit(0xac)
    c.label('reject'); c.emit(0x03, 0xac)
    e.helper('cojvrDualSchofieldProbeMatches', '()Z', c.finish(), 1)

    # Native classification supplies the desired, actual, animation and machine
    # two-handed flags. Override its 20/21/22 result for the retained selected
    # object only. Keep the exception through close if the partner disappears.
    c = Code()
    c.emit(0x1c, 0x10, 20); c.branch(0xa1, 'reject')
    c.emit(0x1c, 0x10, 22); c.branch(0xa3, 'reject')
    c.emit(0x2b); read('Weapon'); c.branch(0xa6, 'reject')
    c.emit(0x2a, 0x2b); read('Hand'); read('Machine'); c.emit(0x03)
    call('cojvrDualSchofieldHandMatches', hand_desc); c.emit(0xac)
    c.label('reject'); c.emit(0x03, 0xac)
    e.helper('cojvrDualSchofieldOneHanded', '(LWeapon;I)Z', c.finish(), 3)

    def classify(code):
        if len(code) != 88 or code[84:] != b'\x04\xac\x03\xac':
            raise ValueError('Unknown Schofield handedness boundary')
        c = Code(); c.emit(0x2a, 0x2b, 0x1c)
        c.ref(0xb6, e.ref(owner, 'cojvrDualSchofieldOneHanded', '(LWeapon;I)Z'))
        c.branch(0x99, 'native'); c.emit(0x03, 0xac); c.label('native')
        tag=c.finish()
        return tag+b'\0'*(-len(tag)%4)+code

    e.changes[('IsWeaponOperatedTwoHand', '(LWeapon;I)Z')] = classify

    # CheckDesiredWeaponState only writes the opposite hand. In scope the
    # selected hand already has the desired one-handed move/reload fields.
    c = Code(); read('Weapon'); c.branch(0xc6, 'reject')
    read('Hand'); c.emit(0x1b); c.branch(0xa0, 'reject')
    c.emit(0x2a); read('Weapon'); c.emit(0x1b); read('Machine'); c.emit(0x03)
    call('cojvrDualSchofieldHandMatches', hand_desc); c.branch(0x99, 'reject')
    slot((0x1b,)); field(hand_machine, 'm_cDesiredState', state_desc)
    field(hand_state, 'm_iState', 'I'); c.emit(0x3d)
    c.emit(0x1c, 0x04); c.branch(0x9f, 'own')
    c.emit(0x1c, 0x10, 21); c.branch(0xa0, 'reject')
    c.label('own')
    finish_bool('cojvrDualSchofieldOwnsDesiredState', '(I)Z', 3)

    def desired_state(code):
        c = Code(); c.emit(0x2a, 0x1b)
        c.ref(0xb6, e.ref(owner, 'cojvrDualSchofieldOwnsDesiredState', '(I)Z'))
        c.branch(0x99, 'native'); c.emit(0xb1); c.label('native')
        tag = c.finish()
        return tag + b'\0' * (-len(tag) % 4) + code

    e.changes[('CheckDesiredWeaponState', '(I)V')] = desired_state

    c = Code()
    read('Weapon'); c.branch(0xc6, 'reject')
    read('Hand'); c.emit(0x1b); c.branch(0xa0, 'reject')
    c.emit(0x1c, 0x10, 21); c.branch(0xa0, 'reject')
    selected_slot(); c.emit(0x4e, 0x2d); c.branch(0xc6, 'reject')
    c.emit(0x2d); field(hand_machine, 'm_cStateMashine', machine_desc)
    read('Machine'); c.branch(0xa6, 'reject')
    read('Machine'); field('StateMashine', 'm_sCurrentState', base_state)
    read('Body'); c.branch(0xa6, 'reject')
    finish_bool('cojvrDualSchofieldSuppress', '(II)Z', 4)

    def completion(code):
        native = b'\x2a\x19\x04\xb6' + e.ref(owner, 'WeaponReload', '(LWeapon;)Z').to_bytes(2, 'big') + b'\x36\x07'
        if code.count(native) != 1:
            raise ValueError('Unknown dual Schofield completion boundary')
        c = Code(); c.emit(0x2a, 0x1b, 0x1c)
        c.ref(0xb6, e.ref(owner, 'cojvrDualSchofieldSuppress', '(II)Z'))
        c.branch(0x99, 'native'); c.emit(0x03, 0x36, 7); c.branch(0xa7, 'after')
        c.label('native'); c.labels['after'] = len(c.data) + len(native)
        pool = parse_constant_pool(Reader((e.next).to_bytes(2, 'big') + e.data[10:e.cp_end] + e.extra))
        return insert_target_tag(code, code.index(native), c.finish(), pool, include_target=True)

    e.changes[('OnHandStateFinished_Reload', '(IIZ)V')] = completion

    def revoke_lost_pair(code):
        c=Code()
        c.emit(0x2a); c.ref(0xb4,dual_field(e,'Weapon')); c.branch(0xc6,'continue')
        c.emit(0x2a); c.ref(0xb4,dual_field(e,'Machine')); c.emit(0x2b); c.branch(0xa6,'continue')
        c.emit(0x2a); c.ref(0xb6,e.ref(owner,'cojvrDualSchofieldProbeMatches','()Z')); c.branch(0x9a,'continue')
        c.emit(0x2a,0x04); c.ref(0xb5,e.ref(owner,'cojvrReloadProbeCancelled','Z',9))
        c.label('continue')
        return c.finish()+code

    e.changes[('cojvrAllowReloadProbeTransition','(LStateMashine;)Z')]=revoke_lost_pair

    c = Code()
    for name in DUAL_FIELDS:
        if name != 'Hand':
            c.emit(0x2a, 0x01); c.ref(0xb5, dual_field(e, name))
    c.emit(0xb1)
    e.helper('cojvrClearDualSchofield', '()V', c.finish(), 1)

    c = Code()
    read('Weapon'); c.branch(0xc6, 'done')
    read('Hand'); c.emit(0x1b); c.branch(0xa0, 'done')
    selected_slot(); c.emit(0x4e, 0x2d); c.branch(0xc6, 'retire')
    c.emit(0x2d); field(hand_machine, 'm_cStateMashine', machine_desc)
    read('Machine'); c.branch(0xa6, 'retire')
    read('Machine'); field('StateMashine', 'm_sCurrentState', base_state)
    c.emit(0x3a, 4, 0x19, 4); c.branch(0xc6, 'retire')
    c.emit(0x19, 4); c.ref(0xc0, e.cls('PlayerState')); field('PlayerState', 'm_iStateID', 'I')
    c.emit(0x36, 5)
    c.emit(0x1c); c.branch(0x99, 'update')
    c.emit(0x15, 5, 0x10, 22); c.branch(0x9f, 'retire'); c.branch(0xa7, 'done')
    c.label('update')
    c.emit(0x15, 5, 0x10, 20); c.branch(0xa1, 'idle')
    c.emit(0x15, 5, 0x10, 22); c.branch(0xa4, 'done')
    c.label('idle')
    c.emit(0x2a); field(owner, 'cojvrReloadProbeWeapon', weapon_desc); c.branch(0xc7, 'done')
    c.emit(0x2d); field(hand_machine, 'm_cDesiredState', state_desc)
    field(hand_state, 'm_iState', 'I'); c.emit(0x10, 21); c.branch(0x9f, 'done')
    c.label('retire'); c.emit(0x2a); call('cojvrClearDualSchofield', '()V')
    c.label('done'); c.emit(0xb1)
    e.helper('cojvrRetireDualSchofield', '(IZ)V', c.finish(), 6)

    def retire(code, finished):
        c = Code(); c.emit(0x2a, 0x1b, 0x04 if finished else 0x03)
        c.ref(0xb6, e.ref(owner, 'cojvrRetireDualSchofield', '(IZ)V'))
        tag = c.finish()
        return tag + b'\0' * (-len(tag) % 4) + code

    e.changes[('UpdateHandState', '(IZ)V')] = lambda code: retire(code, False)
    e.changes[('OnHandStateFinished_ReloadEnd', '(IIZ)V')] = lambda code: retire(code, True)
    return e.finish()


def build_offline_schofield_unload(original_archive: bytes) -> tuple[bytes, bytes]:
    """Load the canonical motion bytecode from a pinned original for offline tests."""
    if hashlib.sha256(original_archive).hexdigest() != PAK_SHA256:
        raise ValueError('Unknown original code.pak SHA-256; no Schofield unload derivative')
    with zipfile.ZipFile(io.BytesIO(original_archive)) as archive:
        return patch_manual_reload_classes(archive.read('ArmedPlayerBeing.class'),
                                           archive.read('StateMashine.class'))


def _append_open(player: bytes, *, dual=False) -> bytes:
    e, c = ClassEdit(player), Code()
    owner = 'ArmedPlayerBeing'
    hand_machine = owner + '$HandStateMashine'
    hand_state = owner + '$HandState'

    def field(cls, name, desc):
        c.ref(0xb4, e.ref(cls, name, desc, 9))

    def player_field(name, desc):
        c.emit(0x2a); field(owner, name, desc)

    def call(name, desc, opcode=0xb6):
        c.ref(opcode, e.ref(owner, name, desc))

    def flag(name, value):
        c.emit(0x2a, 0x04 if value else 0x03)
        c.ref(0xb5, e.ref(owner, name, 'Z', 9))

    def probe(name, desc):
        player_field('cojvrReloadProbe' + name, desc)

    def machine_owner(slot):
        c.emit(0x19, slot)
        field('StateMashine', 'cojvrReloadProbeOwner', 'LArmedPlayerBeing;')

    def opening_ticket():
        for name, desc, load, compare in (
            ('Weapon', 'LWeapon;', (0x2c,), 0xa6),
            ('Hand', 'I', (0x1b,), 0xa0),
            ('Machine', 'LPlayerStateMashine;', (0x19, 4), 0xa6),
            ('Status', 'I', (0x04,), 0xa0),
        ):
            probe(name, desc); c.emit(*load); c.branch(compare, 'fault')
        probe('Cancelled', 'Z'); c.branch(0x9a, 'fault')
        machine_owner(4); c.emit(0x2a); c.branch(0xa6, 'fault')

    def safety(failure):
        for name in ('IsNotAlive', 'IsCarrying', 'GetNetIsOwner'):
            c.emit(0x2a); call(name, '()Z')
            c.branch(0x99 if name == 'GetNetIsOwner' else 0x9a, failure)
        call('IsNetGame', '()Z', 0xb8); c.branch(0x9a, failure)
        player_field('m_bNetAttackForced', 'Z'); c.branch(0x9a, failure)

    def slots(failure, *, opening):
        # Read the same exact fields as the shipped hand getters. The final
        # proof makes no virtual calls that could replace a checked slot.
        c.emit(0x2c); field('Weapon', 'cOwner', 'LPawnInventory;')
        c.emit(0x2a); c.branch(0xa6, failure)
        for support in (False, True):
            player_field('m_aHandStateMashine', '[LArmedPlayerBeing$HandStateMashine;')
            c.emit(0x1b if not support else 0x1d, 0x32, 0x3a, 7, 0x19, 7)
            c.branch(0xc6, failure)
            c.emit(0x19, 7); field(hand_machine, 'm_bTwoHandedMashine', 'Z')
            c.branch(0x9a, failure)
            c.emit(0x19, 7); field(hand_machine, 'm_cStateMashine', 'LPlayerStateMashine;')
            c.emit(0x3a, 8, 0x19, 8); c.branch(0xc6, failure)
            if not support:
                c.emit(0x19, 8, 0x19, 4); c.branch(0xa6, failure)
            if support or not opening:
                for name in ('m_sCurrentState', 'm_sDestState'):
                    c.emit(0x19, 8); field('StateMashine', name, 'LBaseState;')
                    c.emit(0x3a, 9, 0x19, 9); c.branch(0xc6, failure)
                    c.emit(0x19, 9); c.ref(0xc0, e.cls('PlayerState'))
                    field('PlayerState', 'm_iStateID', 'I')
                    c.emit(0x03 if support else 0x04); c.branch(0xa0, failure)
            for name in ('m_cActualState', 'm_cDesiredState'):
                armed_reload_desired = opening and not support and name == 'm_cDesiredState'
                c.emit(0x19, 7); field(hand_machine, name, 'LArmedPlayerBeing$HandState;')
                c.emit(0x3a, 9, 0x19, 9); c.branch(0xc6, failure)
                c.emit(0x19, 9); field(hand_state, 'm_bTwoHanded', 'Z')
                # The original setter marks desired reload 21 as two-handed.
                # The idle state and the other hand must still be independent.
                c.branch(0x99 if armed_reload_desired else 0x9a, failure)
                c.emit(0x19, 9); field(hand_state, 'm_cWeapon', 'LWeapon;')
                if support: c.branch(0xc7, failure)
                else: c.emit(0x2c); c.branch(0xa6, failure)
                c.emit(0x19, 9); field(hand_state, 'm_iState', 'I')
                if armed_reload_desired:
                    c.emit(0x10, 21)
                else:
                    c.emit(0x03 if support else 0x04)
                c.branch(0xa0, failure)
            player_field('ActiveWeapon', '[LWeapon;')
            c.emit(0x1b if not support else 0x1d, 0x32)
            if support: c.branch(0xc7, failure)
            else: c.emit(0x2c); c.branch(0xa6, failure)

    def loaded():
        c.emit(0x2c); c.ref(0xc0, e.cls('WeaponFireAmmo'))
        field('WeaponFireAmmo', 'm_iAmmo0Count', 'I')

    c.emit(0x1b); c.branch(0x9b, 'reject')
    c.emit(0x1b, 0x04); c.branch(0xa3, 'reject')
    c.emit(0x04, 0x1b, 0x64, 0x3e)  # opposite hand, local 3
    flag('cojvrClipBusy', True)
    c.label('try')
    player_field('cojvrManualReloadWeapon', 'LWeapon;')
    c.emit(0x4d, 0x2c); c.branch(0xc6, 'decline')
    from coj_manual_reload import _exact_class_branch
    from coj_clip_reload import CLIP_WEAPONS
    _exact_class_branch(e, c, lambda c: c.emit(0x2c), CLIP_WEAPONS, 'exact')
    c.branch(0xa7, 'decline'); c.label('exact')
    c.emit(0x2a, 0x1b); call('GetHandStateMashine', '(I)LPlayerStateMashine;')
    c.emit(0x3a, 4, 0x19, 4); c.branch(0xc6, 'decline')
    safety('decline')
    for name, desc in (('Weapon', 'LWeapon;'), ('Machine', 'LPlayerStateMashine;')):
        probe(name, desc); c.branch(0xc7, 'decline')
    player_field('cojvrSingleRoundWeapon', 'LWeapon;'); c.branch(0xc7, 'decline')
    machine_owner(4); c.branch(0xc7, 'decline')
    if dual:
        player_field('cojvrDualSchofieldWeapon','LWeapon;'); c.branch(0xc7,'decline')
        c.emit(0x2a,0x1d); call('GetActualWeapon','(I)LWeapon;'); c.emit(0x3a,7)
        c.emit(0x19,7); c.branch(0xc6,'decline')
        c.emit(0x19,7,0x2c); c.branch(0xa5,'decline')
        _exact_class_branch(e,c,lambda code:code.emit(0x19,7),CLIP_WEAPONS,'exact_partner')
        c.branch(0xa7,'decline'); c.label('exact_partner')
        c.emit(0x2a,0x1d); call('GetHandStateMashine','(I)LPlayerStateMashine;'); c.emit(0x3a,8)
        c.emit(0x19,8); c.branch(0xc6,'decline')
        c.emit(0x19,8,0x19,4); c.branch(0xa5,'decline')
        c.emit(0x19,4,0x10,21)
        c.ref(0xb6,e.ref('PlayerStateMashine','FindStateByID','(I)LPlayerState;')); c.emit(0x3a,9)
        c.emit(0x19,9); c.branch(0xc6,'decline')
        for weapon,hand,machine in (((0x2c,),(0x1b,),(0x19,4)),((0x19,7),(0x1d,),(0x19,8))):
            c.emit(0x2a,*weapon,*hand,*machine,0x04)
            call('cojvrDualSchofieldHandMatches','(LWeapon;ILPlayerStateMashine;Z)Z'); c.branch(0x99,'decline')
        for name,load in (('Hand',(0x1b,)),('Machine',(0x19,4)),('Partner',(0x19,7)),
                          ('PartnerMachine',(0x19,8)),('Body',(0x19,9)),('Weapon',(0x2c,))):
            c.emit(0x2a,*load); c.ref(0xb5,dual_field(e,name))
    else:
        slots('decline', opening=False)
    player_field('cojvrManualReloadWeapon', 'LWeapon;')
    c.emit(0x2c); c.branch(0xa6, 'decline')
    loaded(); c.emit(0x36, 5)
    player_field('nAmmoPistol', 'I'); c.emit(0x36, 6)

    # This protected request schedules the native animation even when public
    # reload has no ammo to transfer. The patched request consumes pending
    # publication and installs the same native completion veto as ordinary wait.
    c.emit(0x2a, 0x1b); call('ReloadWeapon', '(I)V', 0xb7)
    opening_ticket()
    c.emit(0x2a); call('cojvrManualReloadValid', '()Z'); c.branch(0x99, 'fault')
    if dual:
        c.emit(0x2a); call('cojvrDualSchofieldProbeMatches','()Z'); c.branch(0x99,'fault')
    else:
        slots('fault', opening=True)
    opening_ticket()
    player_field('cojvrManualReloadWeapon', 'LWeapon;'); c.branch(0xc7, 'fault')
    loaded(); c.emit(0x15, 5); c.branch(0xa0, 'fault')
    player_field('nAmmoPistol', 'I'); c.emit(0x15, 6); c.branch(0xa0, 'fault')
    c.label('try_end')
    flag('cojvrClipBusy', False); c.emit(0x04, 0xac)
    c.label('fault'); flag('cojvrClipFaulted', True)
    c.label('decline'); flag('cojvrClipBusy', False)
    c.label('reject'); c.emit(0x03, 0xac)
    c.label('catch'); c.emit(0x3a, 10)
    flag('cojvrClipFaulted', True)
    flag('cojvrClipBusy', False)
    c.emit(0x19, 10, 0xbf)
    e.helper('cojvrOpenDualSchofield' if dual else 'cojvrOpenSchofield', '(I)Z', c.finish(), 11,
             exception_table=((c.labels['try'], c.labels['try_end'], c.labels['catch'], 0),))
    return e.finish()


def _append_unload(player: bytes) -> bytes:
    e, c = ClassEdit(player), Code()
    owner = 'ArmedPlayerBeing'
    retain_completion, _ = _retained_completion_veto(e, 'cojvrSchofieldUnloadDebt')

    def field(cls, name, desc):
        c.ref(0xb4, e.ref(cls, name, desc, 9))

    def call(cls, name, desc):
        c.ref(0xb6, e.ref(cls, name, desc))

    def set_flag(name, enabled):
        c.emit(0x2a, 0x04 if enabled else 0x03)
        c.ref(0xb5, e.ref(owner, name, 'Z', 9))

    set_flag('cojvrClipBusy', True)
    c.label('try')
    # Locals 1-3 retain the weapon, hand and machine across native calls.
    for name, desc, store in (('Weapon', 'LWeapon;', 0x4c), ('Hand', 'I', 0x3d),
                              ('Machine', 'LPlayerStateMashine;', 0x4e)):
        c.emit(0x2a); field(owner, 'cojvrReloadProbe'+name, desc)
        c.emit(store)
    for load in (0x2b, 0x2d):
        c.emit(load); c.branch(0xc6, 'decline')
    c.emit(0x1c); c.branch(0x9b, 'decline')
    c.emit(0x1c, 0x04); c.branch(0xa3, 'decline')
    _emit_retained_wait(e, c, 1, 2, 3, 'decline', paired=True)
    from coj_manual_reload import _exact_class_branch
    from coj_clip_reload import CLIP_WEAPONS
    _exact_class_branch(e, c, lambda c: c.emit(0x2b), CLIP_WEAPONS, 'exact')
    c.branch(0xa7, 'decline'); c.label('exact')
    # A native callback can retire the mutable probe even before the first
    # heartbeat. The borrowed body's original transfer must stay vetoed.
    _emit_retained_wait(e, c, 1, 2, 3, 'decline', paired=True)
    retain_completion(c, 1, 2, 3)
    c.emit(0x2a); call(owner, 'cojvrManualReloadValid', '()Z')
    c.branch(0x99, 'decline')
    c.emit(0x2b, 0x03); call('Weapon', 'GetAmmoType', '(I)I')
    c.emit(0x05); c.branch(0xa0, 'decline')
    c.emit(0x2b, 0x03); call('Weapon', 'GetAmmoCapacity', '(I)I')
    c.emit(0x10, 6); c.branch(0xa0, 'decline')

    # Retain the exact pistol counter and read storage, not cached XR values.
    def find_pistol_counter():
        c.emit(0x2a, 0x05)
        call('PawnInventory', 'FindAmmoCounter', '(I)LInventoryAmmoCounter;')

    def get_loaded():
        c.emit(0x2b); c.ref(0xc0, e.cls(AMMO_CLASS))
        field(AMMO_CLASS, 'm_iAmmo0Count', 'I')

    def clear_spent():
        c.emit(0x2b); c.ref(0xc0, e.cls(AMMO_CLASS)); c.emit(0x03)
        c.ref(0xb5, e.ref(AMMO_CLASS, SPENT_FIELD, 'I', 9))

    def get_reserve():
        c.emit(0x2a); field('PawnInventory', 'nAmmoPistol', 'I')

    def get_max():
        c.emit(0x19, 4); field('InventoryAmmoCounter', 'nMaxAmount', 'I')

    get_loaded(); c.emit(0x36, 5)
    c.emit(0x15, 5); c.branch(0x9b, 'decline')
    c.emit(0x15, 5, 0x10, 6); c.branch(0xa3, 'decline')
    c.emit(0x2b); c.ref(0xc0, e.cls(AMMO_CLASS)); field(AMMO_CLASS, SPENT_FIELD, 'I')
    c.emit(0x36, 9)
    c.emit(0x15, 9); c.branch(0x9b, 'decline')
    c.emit(0x15, 9, 0x15, 5, 0x60, 0x10, 6); c.branch(0xa3, 'decline')
    c.emit(0x15, 5); c.branch(0x9a, 'live_rounds')
    c.emit(0x15, 9); c.branch(0x99, 'decline')
    _emit_retained_wait(e, c, 1, 2, 3, 'decline', paired=True)
    c.branch(0xa7, 'cleared')

    c.label('live_rounds')
    find_pistol_counter()
    c.emit(0x3a, 4, 0x19, 4); c.branch(0xc6, 'decline')
    c.emit(0x19, 4); call('java/lang/Object', 'getClass', '()Ljava/lang/Class;')
    call('java/lang/Class', 'getName', '()Ljava/lang/String;')
    c.ref(0x13, e.append(b'\x08' + e.utf8('InventoryAmmoCounterPistol').to_bytes(2, 'big')))
    call('java/lang/String', 'equals', '(Ljava/lang/Object;)Z')
    c.branch(0x99, 'decline')
    get_reserve(); c.emit(0x36, 6)
    c.emit(0x15, 6); c.branch(0x9b, 'decline')
    get_max(); c.emit(0x36, 7)
    c.emit(0x15, 7, 0x15, 6); c.branch(0xa1, 'decline')
    def check_storage(label, *, unloaded=False):
        find_pistol_counter()
        c.emit(0x19, 4); c.branch(0xa6, label)
        _emit_retained_wait(e, c, 1, 2, 3, label, paired=True)
        c.emit(0x19, 4); field('InventoryAmmoCounter', 'cOwner', 'LPawnInventory;')
        c.emit(0x2a); c.branch(0xa6, label)
        get_max(); c.emit(0x15, 7); c.branch(0xa0, label)
        get_loaded()
        if unloaded: c.branch(0x9a, label)
        else:
            c.emit(0x15, 5); c.branch(0xa0, label)
        get_reserve(); c.emit(0x15, 6)
        if unloaded: c.emit(0x15, 5, 0x60)
        c.branch(0xa0, label)

    check_storage('decline')
    c.emit(0x15, 7, 0x15, 6, 0x64, 0x15, 5); c.branch(0xa1, 'reserve_full')
    c.emit(0x2a, 0x2b, 0x03)
    call('PawnInventory', 'AddAmmo', '(LWeapon;Z)Z')
    c.branch(0x99, 'fault')
    check_storage('fault', unloaded=True)
    c.label('cleared')
    clear_spent()
    c.label('try_end')
    set_flag('cojvrClipBusy', False)
    c.emit(0x04, 0xac)
    c.label('reserve_full')
    clear_spent()
    set_flag('cojvrClipBusy', False)
    c.emit(0x05, 0xac)
    c.label('fault'); set_flag('cojvrClipFaulted', True); set_flag('cojvrClipBusy', False)
    c.emit(0x02, 0xac)
    c.label('decline'); set_flag('cojvrClipBusy', False)
    c.emit(0x03, 0xac)
    c.label('catch'); c.emit(0x3a, 8)
    set_flag('cojvrClipFaulted', True)
    set_flag('cojvrClipBusy', False)
    c.emit(0x19, 8, 0xbf)
    e.helper('cojvrUnloadSchofield', '()I', c.finish(), 10,
             exception_table=((c.labels['try'], c.labels['try_end'], c.labels['catch'], 0),))
    return e.finish()
