"""Exercise the original held-Schofield return transaction in memory only."""
import hashlib
import io
import zipfile

from coj_indexed_reload_session import _emit_retained_wait
from coj_manual_families import RUNTIME_GUARD_FIELDS
from coj_reload_cycle import _retained_completion_veto
from coj_reload_wait_probe import ClassEdit
from patch_coj_weapon_consumers import Code, PAK_SHA256, patch_manual_reload_classes


def build_offline_schofield_unload(original_archive: bytes) -> tuple[bytes, bytes]:
    if hashlib.sha256(original_archive).hexdigest() != PAK_SHA256:
        raise ValueError('Unknown original code.pak SHA-256; no Schofield unload derivative')
    with zipfile.ZipFile(io.BytesIO(original_archive)) as archive:
        player, machine = patch_manual_reload_classes(archive.read('ArmedPlayerBeing.class'),
                                                      archive.read('StateMashine.class'))
    return _append_unload(player), machine


def _append_unload(player: bytes) -> bytes:
    e, c = ClassEdit(player), Code()
    owner = 'ArmedPlayerBeing'
    retain_completion, _ = _retained_completion_veto(e, 'cojvrLabUnloadDebt')

    def field(cls, name, desc):
        c.ref(0xb4, e.ref(cls, name, desc, 9))

    def call(cls, name, desc):
        c.ref(0xb6, e.ref(cls, name, desc))

    def set_flag(name, enabled):
        c.emit(0x2a, 0x04 if enabled else 0x03)
        c.ref(0xb5, e.ref(owner, name, 'Z', 9))

    for flag in RUNTIME_GUARD_FIELDS:
        c.emit(0x2a); field(owner, flag, 'Z')
        c.branch(0x9a, 'reject')
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
    c.label('reject'); c.emit(0x03, 0xac)
    c.label('catch'); c.emit(0x3a, 8)
    set_flag('cojvrClipFaulted', True)
    set_flag('cojvrClipBusy', False)
    c.emit(0x19, 8, 0xbf)
    e.helper('cojvrLabUnloadSchofield', '()Z', c.finish(), 9,
             exception_table=((c.labels['try'], c.labels['try_end'], c.labels['catch'], 0),))
    return e.finish()
