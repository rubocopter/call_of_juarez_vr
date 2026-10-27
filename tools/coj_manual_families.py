"""Append exact indexed/whole-clip transactions to one production reload probe.

No archive admission, install or gesture policy. Indexed replay uses the
existing native body lifecycle after a verified selected-unit transaction.
The canonical builder verifies originals and owns mixed-class admission/validity.
"""
from coj_indexed_reload import PROFILES as INDEXED_PROFILES
from coj_indexed_reload_session import _append_indexed_session
from coj_clip_reload import CLIP_WEAPONS, _add_offline_clip
from coj_reload_wait_probe import ClassEdit
from inspect_java_bytecode import Reader, parse_class
from patch_coj_weapon_consumers import Code, u2, u4

INDEXED_WEAPONS=tuple(profile[0] for profile in INDEXED_PROFILES)
RUNTIME_BUSY_FIELDS=('cojvrManualFamiliesActive','cojvrManualCycleActive',
                     'cojvrIndexedBusy','cojvrIndexedSessionBusy','cojvrClipBusy')
RUNTIME_FAULT_FIELDS=('cojvrManualFamiliesFaulted','cojvrManualCycleFaulted',
                      'cojvrIndexedFaulted','cojvrClipFaulted')
RUNTIME_GUARD_FIELDS=RUNTIME_BUSY_FIELDS+RUNTIME_FAULT_FIELDS


def _guard_prefix(editor,fields):
    c=Code()
    for name in fields:
        c.emit(0x2a);c.ref(0xb4,editor.ref('ArmedPlayerBeing',name,'Z',9));c.branch(0x9a,'reject')
    c.branch(0xa7,'body');c.label('reject');c.emit(0x03,0xac);c.label('body')
    prefix=c.finish()
    return prefix+b'\0'*(-len(prefix)%4)


def _prepend_guards(player,targets):
    """Preserve bytecode/branches/switch alignment and relocate catch tables.

    ClassEdit.changes intentionally rejects exception-bearing targets. Entry
    guards are field-only and cannot own native effects, so keep original catch
    ranges scoped to the original body after this aligned prefix.
    """
    e=ClassEdit(player)
    prefixes={target:_guard_prefix(e,fields) for target,fields in targets.items()}
    r=e.r
    count=r.u2();members=bytearray(u2(count));found=set()
    for _ in range(count):
        header=r.read(6);members.extend(header)
        target=(e.pool.utf8(int.from_bytes(header[2:4],'big')),
                e.pool.utf8(int.from_bytes(header[4:6],'big')))
        attributes=r.u2();members.extend(u2(attributes))
        for _ in range(attributes):
            index=r.u2();attribute=r.read(r.u4())
            if e.pool.utf8(index)=='Code' and target in prefixes:
                code_reader=Reader(attribute)
                stack,locals_=code_reader.u2(),code_reader.u2()
                code=code_reader.read(code_reader.u4())
                prefix=prefixes[target];shift=len(prefix)
                handlers=[tuple(code_reader.u2() for _ in range(4)) for _ in range(code_reader.u2())]
                table=b''.join(u2(start+shift)+u2(end+shift)+u2(handler+shift)+u2(catch)
                               for start,end,handler,catch in handlers)
                # Java 1.4 has no StackMapTable. As in ClassEdit, obsolete code
                # debug attributes are dropped after changing byte offsets.
                attribute=(u2(max(stack,1))+u2(locals_)+u4(shift+len(code))+prefix+code+
                           u2(len(handlers))+table+u2(0))
                found.add(target)
            members.extend(u2(index)+u4(len(attribute))+attribute)
    if found!=set(targets):
        raise ValueError('Missing runtime family guard target: '+repr(set(targets)-found))
    return (player[:8]+u2(e.next)+player[10:e.cp_end]+e.extra+
            player[e.cp_end:e.fields_end]+members+player[r.stream.tell():])


def append_runtime_families(player:bytes)->bytes:
    """Append once AFTER native-cycle emission and BEFORE the public dispatcher.

    New endpoints: cojvrInsertManualIndexedRound(I)Z and
    cojvrInsertManualClip()Z. Existing heartbeat/completion wrappers survive.
    Common Active/Faulted fields are available for main's legacy entry latch;
    each other transaction acquires its existing own latch before callbacks.
    """
    names={(m.name,m.descriptor) for m in parse_class(player)[1]}
    if ('cojvrInsertManualNativeCycleRound','()Z') not in names:
        raise ValueError('Production cycle/probe must be emitted before runtime families')
    if ('cojvrInsertManualIndexedRound','(I)Z') in names:
        raise ValueError('Runtime families already appended')
    e=ClassEdit(player)
    e.field('cojvrManualFamiliesActive','Z')
    e.field('cojvrManualFamiliesFaulted','Z')
    player=e.finish()
    player=_append_indexed_session(player,helper_name='cojvrInsertManualIndexedRound',
                                   runtime=True,cycle_animation=True)
    player=_add_offline_clip(player,CLIP_WEAPONS,retained_close=True)
    targets={
        ('cojvrInsertManualIndexedRound','(I)Z'):RUNTIME_GUARD_FIELDS,
        ('cojvrInsertManualClip','()Z'):RUNTIME_GUARD_FIELDS,
        ('cojvrInsertManualNativeCycleRound','()Z'):RUNTIME_GUARD_FIELDS,
        ('cojvrKeepManualReloadAlive','()Z'):RUNTIME_GUARD_FIELDS,
        # The only legitimate inner caller holds IndexedSessionBusy and gives
        # a one-shot Dispatch permit. Foreign transactions must not enter it.
        ('cojvrLabInsertIndexedUnit','(LWeapon;II)Z'):tuple(
            name for name in RUNTIME_GUARD_FIELDS if name!='cojvrIndexedSessionBusy'),
    }
    return _prepend_guards(player,targets)
