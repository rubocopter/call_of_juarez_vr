"""Offline Schofield whole-clip reload laboratory; never builds an installable PAK.

The original reload 20/21/22 states keep animation, sound and skin ownership.
One explicitly requested clip insertion calls the original inventory authority
once and verifies loaded/reserve deltas before publishing the event. Uncertain
effects quarantine the player instance without retry or compensating writes.
There is no runtime gesture, clip prop, free-hand or mechanical acceptance.
"""
from __future__ import annotations

import hashlib
import io
import zipfile

from coj_reload_wait_probe import ClassEdit
from patch_coj_weapon_consumers import Code, PAK_SHA256, _patch_class


CLIP_WEAPONS = ('WeaponPistolSchofield', 'WeaponPistolSchofield_A',
                'WeaponPistolSchofield_B')


def _patch_clip_player(player: bytes, weapon: str) -> bytes:
    if weapon not in CLIP_WEAPONS:
        raise ValueError('No exact offline clip profile')
    return _patch_class(player, (weapon,), _whole_clip_expected=True)


def patch_offline_clip_classes(player: bytes, machine: bytes, weapon: str) -> tuple[bytes, bytes]:
    if weapon not in CLIP_WEAPONS:
        raise ValueError('No exact offline clip profile')
    from coj_manual_reload import patch_manual_classes
    return patch_manual_classes(player, machine, _offline_clip=weapon)


def build_offline_clip_lab(original_archive: bytes, weapon: str) -> tuple[bytes, bytes]:
    """Produce only in-memory class derivatives from the pinned complete PAK."""
    if weapon not in CLIP_WEAPONS:
        raise ValueError('No exact offline clip profile')
    if hashlib.sha256(original_archive).hexdigest() != PAK_SHA256:
        raise ValueError('Unknown original code.pak SHA-256; no clip derivative')
    with zipfile.ZipFile(io.BytesIO(original_archive)) as archive:
        if len(archive.namelist()) != len(set(archive.namelist())):
            raise ValueError('Duplicate archive members; no clip derivative')
        return patch_offline_clip_classes(archive.read('ArmedPlayerBeing.class'),
                                          archive.read('StateMashine.class'), weapon)


def _add_offline_clip(player: bytes, weapon_class: str) -> bytes:
    if weapon_class not in CLIP_WEAPONS:
        raise ValueError('No exact offline clip profile')
    e = ClassEdit(player)
    busy = e.field('cojvrClipBusy', 'Z')
    faulted = e.field('cojvrClipFaulted', 'Z')

    def method(name, descriptor):
        return e.ref('ArmedPlayerBeing', name, descriptor)

    def emit_field(c, field):
        c.emit(0x2a)
        c.ref(0xb4, field)

    def set_field(c, field, value):
        c.emit(0x2a, 0x04 if value else 0x03)
        c.ref(0xb5, field)

    def load(c, slot):
        c.emit(0x19, slot)

    def number(c, slot):
        c.emit(0x15, slot)

    ammo_count = e.ref('Weapon', 'GetAmmoCount', '(I)I')
    ammo_capacity = e.ref('Weapon', 'GetAmmoCapacity', '(I)I')
    ammo_type = e.ref('Weapon', 'GetAmmoType', '(I)I')
    weapon_owner = e.ref('Weapon', 'cOwner', 'LPawnInventory;', 9)
    amount = e.ref('InventoryAmmoCounter', 'GetAmount', '()I')
    find_counter = method('FindAmmoCounter', '(I)LInventoryAmmoCounter;')
    probe_weapon = e.ref('ArmedPlayerBeing', 'cojvrReloadProbeWeapon', 'LWeapon;', 9)
    probe_status = e.ref('ArmedPlayerBeing', 'cojvrReloadProbeStatus', 'I', 9)
    probe_cancelled = e.ref('ArmedPlayerBeing', 'cojvrReloadProbeCancelled', 'Z', 9)
    probe_machine = e.ref('ArmedPlayerBeing', 'cojvrReloadProbeMachine', 'LPlayerStateMashine;', 9)
    probe_hand = e.ref('ArmedPlayerBeing', 'cojvrReloadProbeHand', 'I', 9)
    exact_name = e.append(b'\x08' + e.utf8(weapon_class).to_bytes(2, 'big'))

    def check_class(c, label):
        load(c, 1)
        c.ref(0xb6, e.ref('java/lang/Object', 'getClass', '()Ljava/lang/Class;'))
        c.ref(0xb6, e.ref('java/lang/Class', 'getName', '()Ljava/lang/String;'))
        c.ref(0x13, exact_name)
        c.ref(0xb6, e.ref('java/lang/String', 'equals', '(Ljava/lang/Object;)Z'))
        c.branch(0x99, label)
        # The Java reload probe retains the weapon reference, but cOwner can
        # change independently across native inventory and event callbacks.
        load(c, 1)
        c.ref(0xb4, weapon_owner)
        c.emit(0x2a)
        c.branch(0xa6, label)

    c = Code()
    emit_field(c, busy); c.branch(0x9a, 'reject')
    emit_field(c, faulted); c.branch(0x9a, 'reject')
    set_field(c, busy, True)
    c.label('try')

    def check_wait(label):
        c.emit(0x2a); c.ref(0xb6, method('cojvrManualReloadValid', '()Z'))
        c.branch(0x99, label)
        emit_field(c, probe_status); c.emit(0x05); c.branch(0xa0, label)

    check_wait('not_ready')
    emit_field(c, probe_weapon); c.emit(0x4c)  # retained weapon, local 1
    load(c, 1); c.branch(0xc6, 'not_ready')
    check_class(c, 'not_ready')
    # Literal type/capacity are independently proven for each exact Schofield.
    load(c, 1); c.emit(0x03); c.ref(0xb6, ammo_type)
    c.emit(0x05); c.branch(0xa0, 'not_ready')  # original pistol reserve type 2
    load(c, 1); c.emit(0x03); c.ref(0xb6, ammo_capacity)
    c.emit(0x10, 6); c.branch(0xa0, 'not_ready')
    load(c, 1); c.emit(0x03); c.ref(0xb6, ammo_count); c.emit(0x3e)  # local 3
    number(c, 3); c.branch(0x9b, 'not_ready')
    number(c, 3); c.emit(0x10, 6); c.branch(0xa2, 'not_ready')
    c.emit(0x2a, 0x05); c.ref(0xb6, find_counter); c.emit(0x4d)  # local 2
    load(c, 2); c.branch(0xc6, 'not_ready')
    load(c, 2); c.ref(0xb6, amount); c.emit(0x36, 4)  # local 4
    number(c, 4); c.branch(0x9e, 'not_ready')

    c.emit(0x10, 6); number(c, 3); c.emit(0x64); c.emit(0x36, 5)
    number(c, 4); number(c, 5); c.branch(0xa2, 'expected_ready')
    number(c, 4); c.emit(0x36, 5)
    c.label('expected_ready')
    check_wait('not_ready')
    emit_field(c, probe_weapon); load(c, 1); c.branch(0xa6, 'not_ready')
    check_class(c, 'not_ready')
    # Hold the admitted machine/hand across native inventory and callbacks.
    # Closing changes the ticket status, so a post-close verification must use
    # these original references instead of the now-invalid waiting predicate.
    emit_field(c, probe_machine); c.emit(0x3a, 7)
    emit_field(c, probe_hand); c.emit(0x36, 8)

    c.emit(0x2a); load(c, 1)
    c.ref(0xb6, method('WeaponReload', '(LWeapon;)Z'))
    c.branch(0x99, 'fault')

    def readback(label, *, closed=False):
        if closed:
            emit_field(c, probe_status); c.emit(0x06); c.branch(0xa0, label)
            emit_field(c, probe_cancelled); c.branch(0x99, label)
            emit_field(c, probe_machine); load(c, 7); c.branch(0xa6, label)
            emit_field(c, probe_hand); number(c, 8); c.branch(0xa0, label)
            load(c, 7)
            c.ref(0xb6, e.ref('PlayerStateMashine', 'GetCurrentStateID', '()I'))
            c.emit(0x10, 22); c.branch(0xa0, label)
        else:
            check_wait(label)
        emit_field(c, probe_weapon); load(c, 1); c.branch(0xa6, label)
        check_class(c, label)
        load(c, 1); c.emit(0x03); c.ref(0xb6, ammo_count)
        number(c, 3); number(c, 5); c.emit(0x60); c.branch(0xa0, label)
        load(c, 1); c.emit(0x03); c.ref(0xb6, ammo_type)
        c.emit(0x05); c.branch(0xa0, label)
        load(c, 1); c.emit(0x03); c.ref(0xb6, ammo_capacity)
        c.emit(0x10, 6); c.branch(0xa0, label)
        c.emit(0x2a, 0x05); c.ref(0xb6, find_counter)
        load(c, 2); c.branch(0xa6, label)
        load(c, 2); c.ref(0xb6, amount)
        number(c, 4); number(c, 5); c.emit(0x64); c.branch(0xa0, label)

    readback('fault')
    singleton = e.ref('GameMode', 'cSingleton', 'LGameMode;', 9)
    c.ref(0xb2, singleton); c.branch(0xc6, 'after_event')
    c.ref(0xb2, singleton)
    c.ref(0xb2, e.ref('EnumGameEvent', '_WEAPON_RELOADED', 'I', 9))
    c.ref(0xb6, e.ref('GameMode', 'OnGameEvent', '(I)V'))
    c.label('after_event')
    readback('fault')
    # Always close after one whole-clip operation, even when reserve is short.
    c.emit(0x2a); c.ref(0xb6, method('cojvrCancelReloadProbe', '()V'))
    # End-state callbacks may still mutate ammo or ownership. Publish success
    # only when the same clip delta survives the original native close.
    readback('fault', closed=True)
    c.label('accepted'); c.label('try_end')
    set_field(c, busy, False); c.emit(0x04, 0xac)
    c.label('fault'); set_field(c, faulted, True)
    c.label('not_ready'); set_field(c, busy, False); c.emit(0x03, 0xac)
    c.label('catch'); c.emit(0x3a, 6)
    set_field(c, faulted, True); set_field(c, busy, False)
    load(c, 6); c.emit(0xbf)
    c.label('reject'); c.emit(0x03, 0xac)
    e.helper('cojvrInsertManualClip', '()Z', c.finish(), 9,
             exception_table=((c.labels['try'], c.labels['try_end'], c.labels['catch'], 0),))
    # Once native inventory/event callbacks leave an uncertain result, no
    # further heartbeat may prolong the borrowed reload body. A reentrant
    # heartbeat during the transaction is likewise forbidden.
    def heartbeat(original):
        guard = Code()
        for field in (busy, faulted):
            emit_field(guard, field)
            guard.branch(0x9a, 'reject')
        guard.branch(0xa7, 'native')
        guard.label('reject'); guard.emit(0x03, 0xac)
        guard.label('native')
        prefix = guard.finish()
        return prefix + b'\0' * (-len(prefix) % 4) + original

    e.changes[('cojvrKeepManualReloadAlive', '()Z')] = heartbeat
    return e.finish()
