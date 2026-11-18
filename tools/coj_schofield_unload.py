"""Exact base Schofield opening and held ammunition return for explicit dispatch."""
import hashlib
import io
import zipfile

from coj_indexed_reload_session import _emit_retained_wait
from coj_reload_cycle import _retained_completion_veto
from coj_reload_wait_probe import ClassEdit
from patch_coj_weapon_consumers import Code, PAK_SHA256, patch_manual_reload_classes


def build_offline_schofield_unload(original_archive: bytes) -> tuple[bytes, bytes]:
    """Load the canonical motion bytecode from a pinned original for offline tests."""
    if hashlib.sha256(original_archive).hexdigest() != PAK_SHA256:
        raise ValueError('Unknown original code.pak SHA-256; no Schofield unload derivative')
    with zipfile.ZipFile(io.BytesIO(original_archive)) as archive:
        return patch_manual_reload_classes(archive.read('ArmedPlayerBeing.class'),
                                           archive.read('StateMashine.class'))


def _append_open(player: bytes) -> bytes:
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
                c.emit(0x19, 7); field(hand_machine, name, 'LArmedPlayerBeing$HandState;')
                c.emit(0x3a, 9, 0x19, 9); c.branch(0xc6, failure)
                c.emit(0x19, 9); field(hand_state, 'm_bTwoHanded', 'Z')
                c.branch(0x9a, failure)
                c.emit(0x19, 9); field(hand_state, 'm_cWeapon', 'LWeapon;')
                if support: c.branch(0xc7, failure)
                else: c.emit(0x2c); c.branch(0xa6, failure)
                c.emit(0x19, 9); field(hand_state, 'm_iState', 'I')
                if opening and not support and name == 'm_cDesiredState':
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
    _exact_class_branch(e, c, lambda c: c.emit(0x2c), CLIP_WEAPONS[:1], 'exact')
    c.branch(0xa7, 'decline'); c.label('exact')
    c.emit(0x2a, 0x1b); call('GetHandStateMashine', '(I)LPlayerStateMashine;')
    c.emit(0x3a, 4, 0x19, 4); c.branch(0xc6, 'decline')
    safety('decline')
    for name, desc in (('Weapon', 'LWeapon;'), ('Machine', 'LPlayerStateMashine;')):
        probe(name, desc); c.branch(0xc7, 'decline')
    player_field('cojvrSingleRoundWeapon', 'LWeapon;'); c.branch(0xc7, 'decline')
    machine_owner(4); c.branch(0xc7, 'decline')
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
    e.helper('cojvrOpenSchofield', '(I)Z', c.finish(), 11,
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
    _emit_retained_wait(e, c, 1, 2, 3, 'decline')
    c.emit(0x2b); call('java/lang/Object', 'getClass', '()Ljava/lang/Class;')
    call('java/lang/Class', 'getName', '()Ljava/lang/String;')
    c.ref(0x13, e.append(b'\x08' + e.utf8('WeaponPistolSchofield').to_bytes(2, 'big')))
    call('java/lang/String', 'equals', '(Ljava/lang/Object;)Z')
    c.branch(0x99, 'decline')
    # A native callback can retire the mutable probe even before the first
    # heartbeat. The borrowed body's original transfer must stay vetoed.
    _emit_retained_wait(e, c, 1, 2, 3, 'decline')
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
        c.emit(0x2b); c.ref(0xc0, e.cls('WeaponFireAmmo'))
        field('WeaponFireAmmo', 'm_iAmmo0Count', 'I')

    def get_reserve():
        c.emit(0x2a); field('PawnInventory', 'nAmmoPistol', 'I')

    def get_max():
        c.emit(0x19, 4); field('InventoryAmmoCounter', 'nMaxAmount', 'I')

    find_pistol_counter()
    c.emit(0x3a, 4, 0x19, 4); c.branch(0xc6, 'decline')
    c.emit(0x19, 4); call('java/lang/Object', 'getClass', '()Ljava/lang/Class;')
    call('java/lang/Class', 'getName', '()Ljava/lang/String;')
    c.ref(0x13, e.append(b'\x08' + e.utf8('InventoryAmmoCounterPistol').to_bytes(2, 'big')))
    call('java/lang/String', 'equals', '(Ljava/lang/Object;)Z')
    c.branch(0x99, 'decline')
    get_loaded(); c.emit(0x36, 5)
    c.emit(0x15, 5); c.branch(0x9e, 'decline')
    c.emit(0x15, 5, 0x10, 6); c.branch(0xa3, 'decline')
    get_reserve(); c.emit(0x36, 6)
    c.emit(0x15, 6); c.branch(0x9b, 'decline')
    get_max(); c.emit(0x36, 7)
    c.emit(0x15, 7, 0x15, 6); c.branch(0xa1, 'decline')
    c.emit(0x15, 7, 0x15, 6, 0x64, 0x15, 5); c.branch(0xa1, 'decline')

    def check_storage(label, *, unloaded=False):
        find_pistol_counter()
        c.emit(0x19, 4); c.branch(0xa6, label)
        _emit_retained_wait(e, c, 1, 2, 3, label)
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
    c.emit(0x2a, 0x2b, 0x03)
    call('PawnInventory', 'AddAmmo', '(LWeapon;Z)Z')
    c.branch(0x99, 'fault')
    check_storage('fault', unloaded=True)
    c.label('try_end')
    set_flag('cojvrClipBusy', False)
    c.emit(0x04, 0xac)
    c.label('fault'); set_flag('cojvrClipFaulted', True)
    c.label('decline'); set_flag('cojvrClipBusy', False)
    c.emit(0x03, 0xac)
    c.label('catch'); c.emit(0x3a, 8)
    set_flag('cojvrClipFaulted', True)
    set_flag('cojvrClipBusy', False)
    c.emit(0x19, 8, 0xbf)
    e.helper('cojvrUnloadSchofield', '()Z', c.finish(), 9,
             exception_table=((c.labels['try'], c.labels['try_end'], c.labels['catch'], 0),))
    return e.finish()
