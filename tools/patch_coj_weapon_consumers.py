"""Exact CoJ Java 1.4 shot/effects/interaction/whip patch, restored with code.pak.

Nullable instance fields opt only the bridge-owned player into tracked rays.
The untouched original methods handle other players, missing tracking and net fire.
"""
from __future__ import annotations
import argparse
import hashlib
import struct
import zipfile
from pathlib import Path
from inspect_java_bytecode import Reader, parse_constant_pool, disassemble

CLASS_SHA256 = "039b0c12682b99b6560d4597737d794acd4a2b4bb71060c1dc70a8b79ffc4478"
FIRE_CLASS_SHA256 = "d12b513c32db6e3223348f7058799c3702ea3a78746817753998f0de13c9c421"
TRIGGERED_CLASS_SHA256 = "df43daa7fc55299f7a22907da8540a5e44a2bf57c1613f96cdc0d6a19d1045d2"
PAK_SHA256 = "f9db47c166e03f23e37cbcdfd5344e4ad4c5c9134f35e8f6dcdf66db7e71ce12"
SAVE_MODULE_SHA256 = "ded02549df857e99f25d47c34c5ec9c23f0718cad0dd46079db6818d0dcd3c47"
WHIP_CLASS_SHA256 = "0a7ca8d2059582320251646fe2a992bf43ecf04470986128d99ff503366f5746"
WHIP_FIELDS = ("cojvrTrackedPosition", "cojvrTrackedUp", "cojvrTrackedForward")
FIELDS = ("cojvrRightOrigin", "cojvrLeftOrigin", "cojvrRightDirection", "cojvrLeftDirection",
          "cojvrRightFxUp", "cojvrLeftFxUp", "cojvrRightFxForward", "cojvrLeftFxForward")
INTERACTION_FIELDS = ("cojvrInteractionOrigin", "cojvrInteractionDirection")
SHOT_DIAGNOSTIC_FIELDS = ("cojvrShotSerial", "cojvrHitSerial", "cojvrFxSerial",
                          "cojvrFxStatus", "cojvrShotSuppressEffects",
                          "cojvrCombHandle", "cojvrSmokeHandle")
TARGET_OWNER_FIELDS = ("cojvrRightTargetWeapon", "cojvrLeftTargetWeapon")
MANUAL_RELOAD_FIELDS = ("cojvrManualReloadWeapon", "cojvrSingleRoundWeapon",
                        "cojvrSingleRoundHand", "cojvrSingleRoundCompleted")

def reload_class_guard(c, local, get_class, get_name, equals, class_names, whole_clip):
    c.emit(0x19,local);c.ref(0xb6,whole_clip);c.branch(0x9a,'done')
    # Java 1.4 has no ldc Class literal; compare exact runtime class names.
    for i,name in enumerate(class_names):
        c.emit(0x19,local);c.ref(0xb6,get_class);c.ref(0xb6,get_name)
        c.ref(0x13,name);c.ref(0xb6,equals)
        c.branch(0x9a,'consume') if i==0 else c.branch(0x99,'done')
    c.label('consume')

def manual_reload_admission(pending, net, get_class, get_name, equals,
                            class_names, whole_clip, net_owner, net_game,
                            active, active_hand, completed):
    """At original offset 12: reject invalid manual mode before native request.

    Null pending preserves Square/automatic reload. Whole-clip configuration and
    all multiplayer sessions fail closed, rather than reloading a full magazine.
    """
    c=Code()
    c.emit(0x2a);c.ref(0xb4,pending);c.branch(0xc6,'ordinary')
    c.emit(0x1b);c.branch(0x99,'hand_ok')
    c.emit(0x1b,0x04);c.branch(0xa0,'done')
    c.label('hand_ok')
    c.emit(0x2a);c.ref(0xb4,pending);c.emit(0x2c);c.branch(0xa6,'done')
    c.emit(0x1d,0x10,21);c.branch(0xa0,'done')
    c.emit(0x2a);c.ref(0xb4,net);c.branch(0x9a,'done')
    c.ref(0xb8,net_game);c.branch(0x9a,'done')
    c.emit(0x2a);c.ref(0xb6,net_owner);c.branch(0x99,'done')
    reload_class_guard(c,2,get_class,get_name,equals,class_names,whole_clip)
    # All throwing validation has finished. Publish the limiter before the
    # native setter can commit reload or throw after a partial commit.
    c.emit(0x2a,0x1b);c.ref(0xb5,active_hand)
    c.emit(0x2a,0x03);c.ref(0xb5,completed)
    c.emit(0x2a,0x2c);c.ref(0xb5,active)
    c.branch(0xa7,'native')
    c.label('done')
    c.emit(0x2a,0x01);c.ref(0xb5,pending)
    c.emit(0x2a);c.ref(0xb4,active_hand);c.emit(0x1b);c.branch(0xa0,'reject')
    c.emit(0x2a,0x01);c.ref(0xb5,active)
    c.emit(0x2a,0x03);c.ref(0xb5,completed)
    c.label('reject');c.emit(0xb1)
    c.label('ordinary')
    c.emit(0x2a);c.ref(0xb4,active_hand);c.emit(0x1b);c.branch(0xa0,'native')
    c.emit(0x2a,0x01);c.ref(0xb5,active)
    c.emit(0x2a,0x03);c.ref(0xb5,completed)
    c.label('native')
    return c.finish()

def manual_reload_public_reconcile(active, is_reloading, pending, active_hand, current_state):
    c=Code()
    # Desired state becomes move before current ReloadEnd finishes. Do not let
    # another public request discard the consumed latch during that interval.
    c.emit(0x2a);c.ref(0xb4,active);c.branch(0xc6,'reconcile')
    for state in (20,21,22):
        c.emit(0x2a,0x2a);c.ref(0xb4,active_hand);c.ref(0xb6,current_state)
        c.emit(0x10,state);c.branch(0x9f,'closing')
    c.label('reconcile')
    c.emit(0x2a);c.ref(0xb6,is_reloading);c.branch(0x9a,'done')
    c.emit(0x2a,0x01);c.ref(0xb5,active)
    c.branch(0xa7,'done')
    c.label('closing')
    c.emit(0x2a,0x01);c.ref(0xb5,pending);c.emit(0xb1)
    c.label('done')
    return c.finish()

def manual_reload_completion_guard(active, active_hand, completed, actual_weapon):
    """Entry guard runs before the original transfer and event, including reentry."""
    c=Code()
    c.emit(0x2a);c.ref(0xb4,active);c.branch(0xc6,'done')
    c.emit(0x2a);c.ref(0xb4,active_hand);c.emit(0x1b);c.branch(0xa0,'done')
    c.emit(0x1c,0x10,21);c.branch(0xa0,'done')
    c.emit(0x2a,0x1b);c.ref(0xb6,actual_weapon)
    c.emit(0x2a);c.ref(0xb4,active);c.branch(0xa6,'done')
    c.emit(0x2a);c.ref(0xb4,completed);c.branch(0x99,'first')
    c.emit(0xb1)
    c.label('first')
    # Mark before WeaponReload, not merely afterwards: callbacks may reenter.
    c.emit(0x2a,0x04);c.ref(0xb5,completed)
    c.label('done')
    return c.finish()

def manual_reload_consume(pending):
    """Post-setter cleanup is field-only and cannot erase the committed limiter."""
    c=Code()
    c.emit(0x2a,0x01);c.ref(0xb5,pending)
    return c.finish()

def manual_reload_end_cleanup(active, active_hand, completed):
    c=Code()
    c.emit(0x2a);c.ref(0xb4,active_hand);c.emit(0x1b);c.branch(0xa0,'done')
    c.emit(0x2a,0x01);c.ref(0xb5,active)
    c.emit(0x2a,0x03);c.ref(0xb5,completed)
    c.label('done')
    return c.finish()

def manual_reload_replacement_cleanup(active, active_hand, desired_weapon, completed):
    """Replacement revokes obsolete ownership without touching native states."""
    c=Code()
    c.emit(0x2a);c.ref(0xb4,active);c.branch(0xc6,'done')
    c.emit(0x2a);c.ref(0xb4,active_hand);c.emit(0x1b);c.branch(0xa0,'done')
    c.emit(0x2a,0x1b);c.ref(0xb6,desired_weapon)
    c.emit(0x2a);c.ref(0xb4,active);c.branch(0xa5,'done')
    c.emit(0x2a,0x01);c.ref(0xb5,active)
    c.emit(0x2a,0x03);c.ref(0xb5,completed)
    c.label('done')
    return c.finish()

def manual_reload_limiter(ticket, ticket_hand, completed):
    """At original offset 49, retain completed ownership and force native local 8.

    Pending may be cancelled by the bridge; active belongs to native callbacks.
    Native transfer/result local 7, cleanup and game event remain untouched.
    """
    c=Code()
    c.emit(0x2a);c.ref(0xb4,ticket);c.branch(0xc6,'done')
    c.emit(0x1b);c.branch(0x99,'hand_ok')
    c.emit(0x1b,0x04);c.branch(0xa0,'done')
    c.label('hand_ok')
    c.emit(0x2a);c.ref(0xb4,ticket_hand);c.emit(0x1b);c.branch(0xa0,'done')
    c.emit(0x2a);c.ref(0xb4,ticket);c.emit(0x19,4);c.branch(0xa6,'done')
    c.emit(0x19,4,0x19,5);c.branch(0xa6,'done')
    c.emit(0x1c,0x10,21);c.branch(0xa0,'done')
    c.emit(0x15,6,0x10,21);c.branch(0xa0,'done')
    c.emit(0x2a,0x04);c.ref(0xb5,completed)
    c.emit(0x04,0x36,8)
    c.label('done')
    return c.finish()

def target_owner_tag(fields):
    c=Code();c.emit(0x1b);c.branch(0x99,"right")
    c.emit(0x1b,0x04);c.branch(0xa0,"end")
    c.emit(0x2a,0x19,4);c.ref(0xb5,fields[1]);c.branch(0xa7,"end")
    c.label("right");c.emit(0x2a,0x19,4);c.ref(0xb5,fields[0]);c.label("end")
    return c.finish()

def insert_target_tag(code, at, tag, pool, include_target=False):
    positions=[int(line.split(':',1)[0]) for line in disassemble(code,pool)]
    if at not in positions:raise ValueError("Target tag is not at an instruction boundary")
    if any(code[pc] in (0xaa,0xab) for pc in positions):raise ValueError("Unexpected switch in exact target method")
    changed=bytearray(code[:at]+tag+code[at:])
    for pc in positions:
        op=code[pc]
        size=2 if 0x99<=op<=0xa8 or op in (0xc6,0xc7) else (4 if op in (0xc8,0xc9) else 0)
        if not size:continue
        target=pc+int.from_bytes(code[pc+1:pc+1+size],'big',signed=True)
        new_pc=pc+(len(tag) if pc>=at else 0)
        new_target=target+(len(tag) if target>at or (target==at and not include_target) else 0)
        delta=new_target-new_pc
        if delta<-(1<<(size*8-1)) or delta>=(1<<(size*8-1)):raise ValueError("Target branch relocation overflow")
        changed[new_pc+1:new_pc+1+size]=delta.to_bytes(size,'big',signed=True)
    return bytes(changed)

def u2(n): return struct.pack(">H", n)
def u4(n): return struct.pack(">I", n)

class Code:
    def __init__(self): self.data = bytearray(); self.labels = {}; self.fixups = []
    def emit(self, *values): self.data.extend(values)
    def ref(self, op, index): self.emit(op); self.data.extend(u2(index))
    def label(self, name): self.labels[name] = len(self.data)
    def branch(self, op, target):
        self.fixups.append((len(self.data), target)); self.emit(op, 0, 0)
    def finish(self):
        for at, target in self.fixups:
            self.data[at+1:at+3] = struct.pack(">h", self.labels[target] - at)
        return bytes(self.data)

def prefix(fields, net_field, hand_method, set_method, weapon=False):
    c = Code()
    # Original null-output and network-forced semantics always win.
    c.emit(0x2c); c.branch(0xc6, "fallback")
    c.emit(0x2a); c.ref(0xb4, net_field); c.branch(0x9a, "fallback")
    if weapon:
        c.emit(0x2a, 0x2b); c.ref(0xb6, hand_method); c.emit(0x3e)  # hand local 3
    c.emit(0x1d if weapon else 0x1b); c.branch(0x99, "right")
    c.emit(0x1d if weapon else 0x1b, 0x04); c.branch(0xa0, "fallback")
    c.emit(0x2a); c.ref(0xb4, fields[1]); c.branch(0xa7, "value")
    c.label("right"); c.emit(0x2a); c.ref(0xb4, fields[0])
    c.label("value"); c.emit(0x4e if not weapon else 0x3a)
    if weapon: c.emit(4)
    c.emit(0x2d if not weapon else 0x19)
    if weapon: c.emit(4)
    c.branch(0xc6, "fallback")
    c.emit(0x2c, 0x2d if not weapon else 0x19)
    if weapon: c.emit(4)
    c.ref(0xb6, set_method); c.emit(0xac)
    c.label("fallback")
    return c.finish()

def visual_direction_source(fields, net_field, native_array):
    # Return only the base vector. The original visualization method still
    # applies the same shot accuracy/rotation as the ballistic consumer.
    c=Code()
    c.emit(0x2a);c.ref(0xb4,net_field);c.branch(0x9a,"fallback")
    c.emit(0x1b);c.branch(0x99,"right")
    c.emit(0x1b,0x04);c.branch(0xa0,"fallback")
    c.emit(0x2a);c.ref(0xb4,fields[1]);c.branch(0xa7,"value")
    c.label("right");c.emit(0x2a);c.ref(0xb4,fields[0])
    c.label("value");c.emit(0x4d,0x2c);c.branch(0xc6,"fallback")
    c.emit(0x2c,0xb0)
    c.label("fallback");c.emit(0x2a);c.ref(0xb4,native_array);c.emit(0x1b,0x32,0xb0)
    return c.finish()

def fire_effect_prefix(owner_field, player_class, net_field, hand_method,
                       origins, directions, effects, create_method, ups, forwards,
                       set_frame_method, delete_method, detach_method, diagnostics=None):
    c=Code()
    c.emit(0x2a);c.ref(0xb4,owner_field);c.ref(0xc1,player_class);c.branch(0x99,"fallback")
    c.emit(0x2a);c.ref(0xb4,owner_field);c.ref(0xc0,player_class);c.emit(0x4d)
    c.emit(0x2c);c.ref(0xb4,net_field);c.branch(0x9a,"fallback")
    c.emit(0x2c,0x2a);c.ref(0xb6,hand_method);c.emit(0x3e)
    c.emit(0x1d);c.branch(0x99,"right")
    c.emit(0x1d,0x04);c.branch(0xa0,"fallback")
    c.emit(0x2c);c.ref(0xb4,origins[1]);c.emit(0x3a,4)
    c.emit(0x2c);c.ref(0xb4,directions[1]);c.emit(0x3a,5)
    c.emit(0x2c);c.ref(0xb4,ups[1]);c.emit(0x3a,6)
    c.emit(0x2c);c.ref(0xb4,forwards[1]);c.emit(0x3a,7);c.branch(0xa7,"value")
    c.label("right")
    c.emit(0x2c);c.ref(0xb4,origins[0]);c.emit(0x3a,4)
    c.emit(0x2c);c.ref(0xb4,directions[0]);c.emit(0x3a,5)
    c.emit(0x2c);c.ref(0xb4,ups[0]);c.emit(0x3a,6)
    c.emit(0x2c);c.ref(0xb4,forwards[0]);c.emit(0x3a,7)
    c.label("value")
    c.emit(0x19,4);c.branch(0xc6,"fallback")
    c.emit(0x19,5);c.branch(0xc6,"fallback")
    c.emit(0x19,6);c.branch(0xc6,"fallback")
    c.emit(0x19,7);c.branch(0xc6,"fallback")
    def status(bit):
        if diagnostics:
            c.emit(0x2a,0x2a);c.ref(0xb4,diagnostics[1]);c.emit(0x10,bit,0x80)
            c.ref(0xb5,diagnostics[1])
    if diagnostics:
        c.emit(0x2a,0x2a);c.ref(0xb4,diagnostics[0]);c.emit(0x04,0x60);c.ref(0xb5,diagnostics[0])
        c.emit(0x2a,0x03);c.ref(0xb5,diagnostics[1])
        for field in diagnostics[2:4]:
            c.emit(0x2a,0x03);c.ref(0xb5,field)
    for i,(emitter,particle) in enumerate(effects):
        c.emit(0x2b);c.ref(0xb4,emitter);c.branch(0x99,f"next{i}")
        c.emit(0x2b);c.ref(0xb4,particle);c.branch(0x99,f"next{i}")
        status(1 << i)
        c.emit(0x2a,0x2b);c.ref(0xb4,emitter)
        c.emit(0x2b);c.ref(0xb4,particle)
        # Create detached, then retain the entire measured Barrel frame. The
        # create-call's direction parameter is LOCAL +Y, while barrel particles
        # emit along LOCAL -X. Aim direction alone would rotate them sideways.
        c.emit(0x19,4,0x01,0x01,0x02);c.ref(0xb6,create_method);c.emit(0x36,8)
        c.emit(0x15,8);c.branch(0x99,f"next{i}")
        status(1 << (i+2))
        c.emit(0x2a,0x15,8,0x19,4,0x19,6,0x19,7);c.ref(0xb6,set_frame_method)
        c.branch(0x99,f"failed{i}")
        # FXSetStartXForm writes only the starting matrix (+0x18). FXDetach
        # resolves that matrix into the active/previous world positions and
        # removes any parent reference before the native weapon is restored.
        c.emit(0x2a,0x15,8);c.ref(0xb6,detach_method)
        c.branch(0x99,f"failed{i}")
        status(1 << (i+4))
        if diagnostics:
            c.emit(0x2a,0x15,8);c.ref(0xb5,diagnostics[2+i])
        c.branch(0xa7,f"next{i}")
        c.label(f"failed{i}")
        # sipush is needed for the smoke failure's bit 7 (signed byte 128).
        if diagnostics:
            c.emit(0x2a,0x2a);c.ref(0xb4,diagnostics[1]);c.emit(0x11)
            c.data.extend(u2(1 << (i+6)));c.emit(0x80);c.ref(0xb5,diagnostics[1])
        # A failed full-frame write must not leave a misoriented emitter alive.
        c.emit(0x2a,0x15,8);c.ref(0xb6,delete_method)
        c.label(f"next{i}")
    c.emit(0xb1);c.label("fallback")
    return c.finish()

def tracked_event_prefix(owner_field, player_class, net_field, origins, counter, suppressed=None):
    """Observe native entry without invoking getters, spread or attack code."""
    c=Code()
    c.emit(0x2a);c.ref(0xb4,owner_field);c.ref(0xc1,player_class);c.branch(0x99,'done')
    def owner():
        c.emit(0x2a);c.ref(0xb4,owner_field);c.ref(0xc0,player_class)
    owner();c.ref(0xb4,net_field);c.branch(0x9a,'done')
    owner();c.ref(0xb4,origins[0]);c.branch(0xc6,'left');c.branch(0xa7,'observe')
    c.label('left');owner();c.ref(0xb4,origins[1]);c.branch(0xc6,'done')
    c.label('observe')
    c.emit(0x2a,0x2a);c.ref(0xb4,counter);c.emit(0x04,0x60);c.ref(0xb5,counter)
    if suppressed:
        c.emit(0x2a,0x15,4);c.ref(0xb5,suppressed)
    c.label('done')
    return c.finish()

def single_hand_attack_guard(origins, net_field, actual_weapon, firearm_class, two_handed, whip_class=None):
    """Reject native cross-hand fallback only for a tracked one-hand firearm."""
    c=Code()
    c.emit(0x2a);c.ref(0xb4,net_field);c.branch(0x9a,'native')
    c.emit(0x1b,0x1c);c.branch(0x9f,'native')
    c.emit(0x1b);c.branch(0x99,'right')
    c.emit(0x1b,0x04);c.branch(0xa0,'native')
    c.emit(0x2a);c.ref(0xb4,origins[1]);c.branch(0xa7,'cache')
    c.label('right');c.emit(0x2a);c.ref(0xb4,origins[0])
    c.label('cache');c.branch(0xc6,'native')
    c.emit(0x2a,0x1b);c.ref(0xb6,actual_weapon);c.emit(0x4e)
    c.emit(0x2d);c.ref(0xc1,firearm_class);c.branch(0x99,'native')
    if whip_class is not None:
        c.emit(0x2d);c.ref(0xc1,whip_class);c.branch(0x9a,'native')
    c.emit(0x2a,0x1b);c.ref(0xb6,two_handed);c.branch(0x9a,'native')
    c.emit(0x03,0xac)
    c.label('native')
    return c.finish()

def patch_class(data: bytes) -> bytes:
    if hashlib.sha256(data).hexdigest() != CLASS_SHA256:
        raise ValueError("Unknown ArmedPlayerBeing.class SHA-256; no mutation")
    r = Reader(data); r.read(8); pool = parse_constant_pool(r)
    cp_end = r.stream.tell(); additions = bytearray(); next_index = len(pool.entries)
    def append(raw):
        nonlocal next_index
        index = next_index; next_index += 1; additions.extend(raw); return index
    def utf8(s):
        raw = s.encode("utf-8"); return append(b"\x01" + u2(len(raw)) + raw)
    def find(description):
        return next(i for i in range(1, len(pool.entries))
                    if pool.entries[i] is not None and pool.describe(i) == description)
    r.u2(); this_class = r.u2(); r.u2(); r.read(r.u2()*2)
    fields_at = r.stream.tell(); field_count = r.u2()
    def member():
        start = r.stream.tell(); r.read(6)
        for _ in range(r.u2()): r.u2(); r.read(r.u4())
        return data[start:r.stream.tell()]
    for _ in range(field_count): member()
    fields_end = r.stream.tell()
    descriptor = utf8("LVector;"); extra_fields = bytearray(); refs = []
    for name in FIELDS:
        name_index = utf8(name)
        extra_fields.extend(u2(1)+u2(name_index)+u2(descriptor)+u2(0))
        nt = append(b"\x0c"+u2(name_index)+u2(descriptor))
        refs.append(append(b"\x09"+u2(this_class)+u2(nt)))
    net = find("ArmedPlayerBeing.m_bNetAttackForcedZ")
    hand = find("ArmedPlayerBeing.GetActualHandForWeapon(LWeapon;)I")
    vector_set = find("Vector.SetIfNotNull(LVector;)Z")
    native_visual_array = find("ArmedPlayerBeing.m_avAimDir[LVector;")
    visual_name=utf8("cojvrGetVisualAimDir");visual_desc=utf8("(I)LVector;")
    visual_nt=append(b"\x0c"+u2(visual_name)+u2(visual_desc))
    visual_method=append(b"\x0a"+u2(this_class)+u2(visual_nt))
    visual_target=("GetFireDirVisualizationForHand","(ILVector;)Z")
    attack_target=("CanAttack","(II)Z")
    try: firearm_class=find("WeaponFire")
    except StopIteration: firearm_class=append(b"\x07"+u2(utf8("WeaponFire")))
    attack_guard=single_hand_attack_guard(refs[:2],net,
        find("ArmedPlayerBeing.GetActualWeapon(I)LWeapon;"),firearm_class,
        find("ArmedPlayerBeing.IsActualWeaponOperatedTwoHand(I)Z"),find("WeaponWhip"))
    targets = {
        ("GetFireOriginForWeapon", "(LWeapon;LVector;)Z"): (refs[:2], True),
        ("GetFireOriginVisualizationForHand", "(ILVector;)Z"): (refs[:2], False),
        ("GetBeingLookDirDevForHand", "(ILVector;)Z"): (refs[2:4], False),
    }
    target_refs=[];weapon_descriptor=utf8("LWeapon;")
    for name in TARGET_OWNER_FIELDS:
        name_index=utf8(name);extra_fields.extend(u2(1)+u2(name_index)+u2(weapon_descriptor)+u2(0))
        nt=append(b"\x0c"+u2(name_index)+u2(weapon_descriptor));target_refs.append(append(b"\x09"+u2(this_class)+u2(nt)))
    target_method=("CheckIfAimingAtFriendlyTarget","(IZ)V")
    manual_refs=[]
    for name,desc in zip(MANUAL_RELOAD_FIELDS,(weapon_descriptor,weapon_descriptor,utf8('I'),utf8('Z'))):
        name_index=utf8(name)
        # Transient instance fields; native SGSave/SGLoad remain authoritative.
        extra_fields.extend(u2(0x0081)+u2(name_index)+u2(desc)+u2(0))
        nt=append(b'\x0c'+u2(name_index)+u2(desc))
        manual_refs.append(append(b'\x09'+u2(this_class)+u2(nt)))
    def method_ref(owner,name,desc):
        cls=append(b'\x07'+u2(utf8(owner)))
        nt=append(b'\x0c'+u2(utf8(name))+u2(utf8(desc)))
        return append(b'\x0a'+u2(cls)+u2(nt))
    names=tuple(append(b'\x08'+u2(utf8(name))) for name in
                ('WeaponPistolFrontier1878_Regular','WeaponPistolPeacemaker'))
    class_methods=(
        method_ref('java/lang/Object','getClass','()Ljava/lang/Class;'),
        method_ref('java/lang/Class','getName','()Ljava/lang/String;'),
        method_ref('java/lang/String','equals','(Ljava/lang/Object;)Z'),names,
        method_ref('Weapon','IsReloadWholeClipAtOnce','()Z'),
        find('ArmedPlayerBeing.GetNetIsOwner()Z'))
    limiter=manual_reload_limiter(*manual_refs[1:])
    consume=manual_reload_consume(manual_refs[0])
    admission=manual_reload_admission(manual_refs[0],net,*class_methods,
        find('ArmedPlayerBeing.IsNetGame()Z'),*manual_refs[1:])
    reconcile=manual_reload_public_reconcile(manual_refs[1],
        find('ArmedPlayerBeing.IsWeaponReloading()Z'),manual_refs[0],manual_refs[2],
        find('ArmedPlayerBeing.GetHandStateMashineState(I)I'))
    completion_guard=manual_reload_completion_guard(*manual_refs[1:],
        find('ArmedPlayerBeing.GetActualWeapon(I)LWeapon;'))
    pending_clear=b'\x2a\x01\xb5'+u2(manual_refs[0])
    cleanup=manual_reload_end_cleanup(*manual_refs[1:])
    replacement_cleanup=manual_reload_replacement_cleanup(*manual_refs[1:3],
        find('ArmedPlayerBeing.GetDesiredWeapon(I)LWeapon;'),manual_refs[3])
    reload_target=('OnHandStateFinished_Reload','(IIZ)V')
    reload_request=('ReloadWeapon','(I)V')
    reload_public=('ReloadWeapon','()V')
    reload_end=('OnHandStateFinished_ReloadEnd','(IIZ)V')
    reload_replacement=('UpdateHandState','(IZ)V')
    reload_targets={reload_target,reload_request,reload_public,reload_end,reload_replacement}
    relocation_pool=parse_constant_pool(Reader(u2(next_index)+data[10:cp_end]+additions))
    method_count = r.u2(); methods = bytearray(u2(method_count+1)); patched = set()
    for _ in range(method_count):
        header = r.read(6); name, desc = pool.utf8(int.from_bytes(header[2:4], "big")), pool.utf8(int.from_bytes(header[4:6], "big"))
        methods.extend(header); count = r.u2(); methods.extend(u2(count))
        for _ in range(count):
            attr_index = r.u2(); attr = r.read(r.u4()); target = targets.get((name, desc))
            if pool.utf8(attr_index) == "Code" and (target or (name,desc) in {visual_target,attack_target,target_method}|reload_targets):
                cr = Reader(attr); stack, locals_ = cr.u2(), cr.u2(); code = cr.read(cr.u4())
                if cr.u2() != 0: raise ValueError("Unexpected shot exception table")
                if (name,desc)==reload_public:
                    if len(code)!=98 or code[7]!=0xb1 or code[97]!=0xb1:
                        raise ValueError('Unexpected exact public reload boundary')
                    for at in (97,7):
                        code=insert_target_tag(code,at,pending_clear,relocation_pool,include_target=True)
                    code=reconcile+code
                elif (name,desc)==reload_request:
                    if len(code)!=19 or code[15:18]!=b'\xb6'+u2(find('ArmedPlayerBeing.SetDesiredWeaponState(II)V')):
                        raise ValueError('Unexpected exact protected reload boundary')
                    code=insert_target_tag(code,18,consume,pool)
                    code=insert_target_tag(code,12,admission,relocation_pool)
                elif (name,desc)==reload_end:
                    code=cleanup+code
                elif (name,desc)==reload_replacement:
                    code=replacement_cleanup+code
                elif (name,desc)==reload_target:
                    expected=b'\x2a\x19\x04\xb6'+u2(find('ArmedPlayerBeing.WeaponReload(LWeapon;)Z'))+b'\x36\x07'
                    if len(code)!=117 or code[26:34]!=expected or code[47:49]!=b'\x36\x08':
                        raise ValueError('Unexpected exact reload completion boundary')
                    code=insert_target_tag(code,49,limiter,pool)
                    code=completion_guard+code
                elif (name,desc)==target_method:
                    # Tag the local Weapon used by the natural trace, only after
                    # both native LC segment vectors have been committed. Budget
                    # skips preserve the old owner; no trace/update is replayed.
                    expected=b'\x2a\xb4'+u2(find("ArmedPlayerBeing.m_vLCEnd[LVector;"))+b'\x1b\x32\xb2'+u2(find("ArmedPlayerBeing.sm_vrEndLVector;"))+b'\xb6'+u2(find("Vector.Set(LVector;)V"))
                    if len(code)!=683 or code[390:402]!=expected:raise ValueError("Unexpected exact target segment commit")
                    code=insert_target_tag(code,402,target_owner_tag(target_refs),pool)
                elif (name,desc)==attack_target:
                    code=attack_guard+code
                elif (name,desc)==visual_target:
                    if code[6:13] != b"\x2a\xb4"+u2(native_visual_array)+b"\x1b\x32\x4e":
                        raise ValueError("Unexpected native visualization source")
                    code=code[:6]+b"\x2a\x1b\xb7"+u2(visual_method)+b"\x00"+code[12:]
                else:
                    code = prefix(target[0], net, hand, vector_set, target[1]) + code
                # These exact methods have no switches/stackmaps. Debug tables
                # are omitted for the patched methods, never shifted incorrectly.
                attr = u2(max(3, stack))+u2(max(5 if target and target[1] else 4, locals_))+u4(len(code))+code+u2(0)+u2(0)
                patched.add((name, desc))
            methods.extend(u2(attr_index)+u4(len(attr))+attr)
    if patched != set(targets)|{visual_target,attack_target,target_method}|reload_targets: raise ValueError("Incomplete shot boundary patch")
    helper=visual_direction_source(refs[2:4],net,native_visual_array)
    attr=u2(2)+u2(3)+u4(len(helper))+helper+u2(0)+u2(0)
    methods.extend(u2(2)+u2(visual_name)+u2(visual_desc)+u2(1)+u2(find("'Code'"))+u4(len(attr))+attr)
    return (data[:8]+u2(next_index)+data[10:cp_end]+additions+
            data[cp_end:fields_at]+u2(field_count+len(FIELDS)+len(TARGET_OWNER_FIELDS)+len(MANUAL_RELOAD_FIELDS))+data[fields_at+2:fields_end]+
            extra_fields+methods+data[r.stream.tell():])

def patch_fire_class(data: bytes) -> bytes:
    if hashlib.sha256(data).hexdigest() != FIRE_CLASS_SHA256:
        raise ValueError("Unknown WeaponFire.class SHA-256; no mutation")
    r=Reader(data);r.read(8);pool=parse_constant_pool(r)
    cp_end=r.stream.tell();additions=bytearray();next_index=len(pool.entries)
    def append(raw):
        nonlocal next_index
        index=next_index;next_index+=1;additions.extend(raw);return index
    def utf8(s):
        raw=s.encode();return append(b"\x01"+u2(len(raw))+raw)
    def find(description):
        return next(i for i in range(1,len(pool.entries)) if pool.entries[i] is not None and pool.describe(i)==description)
    player=append(b"\x07"+u2(utf8("ArmedPlayerBeing")))
    def ref(tag,name,desc):
        nt=append(b"\x0c"+u2(utf8(name))+u2(utf8(desc)))
        return append(bytes([tag])+u2(player)+u2(nt))
    net=ref(9,"m_bNetAttackForced","Z")
    hand=ref(10,"GetActualHandForWeapon","(LWeapon;)I")
    origins=[ref(9,name,"LVector;") for name in FIELDS[:2]]
    directions=[ref(9,name,"LVector;") for name in FIELDS[2:4]]
    ups=[ref(9,name,"LVector;") for name in FIELDS[4:6]]
    forwards=[ref(9,name,"LVector;") for name in FIELDS[6:8]]
    def weapon_method(name,desc):
        nt=append(b"\x0c"+u2(utf8(name))+u2(utf8(desc)))
        return append(b"\x0a"+u2(find("WeaponFire"))+u2(nt))
    extra_fields=bytearray(); diagnostic_refs=[]
    for name in SHOT_DIAGNOSTIC_FIELDS:
        name_id,desc_id=utf8(name),utf8('I')
        extra_fields.extend(u2(1)+u2(name_id)+u2(desc_id)+u2(0))
        nt=append(b'\x0c'+u2(name_id)+u2(desc_id))
        diagnostic_refs.append(append(b'\x09'+u2(find('WeaponFire'))+u2(nt)))
    effects=[(find("WpnFXFire.m_nFXCombEmiterDefIDI"),find("WpnFXFire.m_nFXCombParticleDefIDI")),
             (find("WpnFXFire.m_nFXSmokeEmiterDefIDI"),find("WpnFXFire.m_nFXSmokeParticleDefIDI"))]
    pre=fire_effect_prefix(find("WeaponFire.cOwnerLPawnInventory;"),player,net,hand,origins,directions,effects,
        find("WeaponFire.FXCreateParticleEmiter(IILVector;LVector;LControlObject;I)I"),ups,forwards,
        weapon_method("FXSetStartXForm","(ILVector;LVector;LVector;)Z"),weapon_method("FXDelete","(I)V"),
        weapon_method("FXDetach","(I)Z"),diagnostic_refs[2:4]+diagnostic_refs[5:7])
    r.read(6);r.read(r.u2()*2)
    fields_at=r.stream.tell();field_count=r.u2()
    for _ in range(field_count):
        r.read(6)
        for _ in range(r.u2()):r.u2();r.read(r.u4())
    methods_at=r.stream.tell();method_count=r.u2();methods=bytearray(u2(method_count));patched=set()
    for _ in range(method_count):
        header=r.read(6);methods.extend(header)
        name=pool.utf8(int.from_bytes(header[2:4],"big"));desc=pool.utf8(int.from_bytes(header[4:6],"big"))
        attr_count=r.u2();methods.extend(u2(attr_count))
        for _ in range(attr_count):
            index=r.u2();attr=r.read(r.u4())
            target=(name,desc)
            observed=target in (("AttackFire","(LWpnAttackFire;IFZ)V"),
                ("OnHit","(LPawnInventory;LWpnAttackFire;FLControlObject;ILVector;LVector;LVector;IIZZZ)V"))
            if pool.utf8(index)=="Code" and ((name=="ExecFXFire" and desc=="(LWpnFXFire;)V") or observed):
                cr=Reader(attr);stack,locals_=cr.u2(),cr.u2();original=cr.read(cr.u4())
                if cr.u2()!=0:raise ValueError("Unexpected fire effect exception table")
                prefix_code=pre if not observed else tracked_event_prefix(
                    find('WeaponFire.cOwnerLPawnInventory;'),player,net,origins,
                    diagnostic_refs[0 if name=='AttackFire' else 1],
                    diagnostic_refs[4] if name=='AttackFire' else None)
                # Preserve any original switch alignment and relative branch offsets.
                prefix_code+=b'\x00'*((-len(prefix_code))%4)
                code=prefix_code+original
                attr=u2(max(stack,7))+u2(max(locals_,9))+u4(len(code))+code+u2(0)+u2(0)
                patched.add(target)
            methods.extend(u2(index)+u4(len(attr))+attr)
    if len(patched)!=3:raise ValueError("Incomplete fire effects/observation patch")
    return (data[:8]+u2(next_index)+data[10:cp_end]+additions+data[cp_end:fields_at]+
        u2(field_count+len(SHOT_DIAGNOSTIC_FIELDS))+data[fields_at+2:methods_at]+extra_fields+
        methods+data[r.stream.tell():])

def interaction_getter(fields, selected_field, vector_set, native_getter):
    """Paired local cache, with the original virtual getter as fallback."""
    c = Code()
    c.emit(0x2b); c.branch(0xc6, "native")
    for field in fields:
        c.emit(0x2a); c.ref(0xb4, field); c.branch(0xc6, "native")
    c.emit(0x2b, 0x2a); c.ref(0xb4, selected_field)
    c.ref(0xb6, vector_set); c.emit(0xac)
    c.label("native")
    c.emit(0x2a, 0x2b); c.ref(0xb6, native_getter); c.emit(0xac)
    return c.finish()

def patch_triggered_class(data: bytes) -> bytes:
    if hashlib.sha256(data).hexdigest() != TRIGGERED_CLASS_SHA256:
        raise ValueError("Unknown BeingTriggered.class SHA-256; no mutation")
    r = Reader(data); r.read(8); pool = parse_constant_pool(r)
    cp_end = r.stream.tell(); additions = bytearray(); next_index = len(pool.entries)
    def append(raw):
        nonlocal next_index
        index = next_index; next_index += 1; additions.extend(raw); return index
    def utf8(value):
        raw = value.encode(); return append(b"\x01" + u2(len(raw)) + raw)
    def find(description):
        return next(i for i in range(1, len(pool.entries))
                    if pool.entries[i] is not None and pool.describe(i) == description)
    r.u2(); this_class = r.u2(); r.u2(); r.read(r.u2() * 2)
    fields_at = r.stream.tell(); field_count = r.u2()
    for _ in range(field_count):
        r.read(6)
        for _ in range(r.u2()): r.u2(); r.read(r.u4())
    fields_end = r.stream.tell()
    desc = utf8("LVector;"); extra_fields = bytearray(); fields = []
    for name in INTERACTION_FIELDS:
        name_id = utf8(name)
        extra_fields.extend(u2(1) + u2(name_id) + u2(desc) + u2(0))
        nt = append(b"\x0c" + u2(name_id) + u2(desc))
        fields.append(append(b"\x09" + u2(this_class) + u2(nt)))
    vector_class = find("Vector")
    set_nt = append(b"\x0c" + u2(utf8("SetIfNotNull")) + u2(utf8("(LVector;)Z")))
    vector_set = append(b"\x0a" + u2(vector_class) + u2(set_nt))
    helpers = []
    for name, native_name in (("cojvrGetInteractionOrigin", "GetBeingLookFromPoint"),
                              ("cojvrGetInteractionDirection", "GetBeingLookDir")):
        name_id, desc_id = utf8(name), utf8("(LVector;)Z")
        nt = append(b"\x0c" + u2(name_id) + u2(desc_id))
        method = append(b"\x0a" + u2(this_class) + u2(nt))
        native = find("BeingTriggered." + native_name + "(LVector;)Z")
        helpers.append((name_id, desc_id, method, native))
    method_count = r.u2(); methods = bytearray(u2(method_count + 2)); patched = 0
    for _ in range(method_count):
        header = r.read(6); methods.extend(header)
        name = pool.utf8(int.from_bytes(header[2:4], "big"))
        descriptor = pool.utf8(int.from_bytes(header[4:6], "big"))
        attr_count = r.u2(); methods.extend(u2(attr_count))
        for _ in range(attr_count):
            index = r.u2(); attr = r.read(r.u4())
            if name == "CheckTriggers" and descriptor == "()V" and pool.utf8(index) == "Code":
                cr = Reader(attr); stack, locals_ = cr.u2(), cr.u2()
                code = bytearray(cr.read(cr.u4()))
                # Same-length substitutions retain every branch, native selection
                # condition/range, exception table and debug offset unchanged.
                if len(code) != 148:
                    raise ValueError("Unexpected CheckTriggers length")
                for offset, helper in zip((12, 20), helpers):
                    if code[offset:offset+3] != b"\xb6" + u2(helper[3]):
                        raise ValueError("Unexpected CheckTriggers getter boundary")
                    code[offset:offset+3] = b"\xb7" + u2(helper[2])
                attr = u2(stack) + u2(locals_) + u4(len(code)) + code + attr[8+len(code):]
                patched += 1
            methods.extend(u2(index) + u4(len(attr)) + attr)
    if patched != 1: raise ValueError("Incomplete interaction boundary patch")
    for field, (name, desc_id, _, native) in zip(fields, helpers):
        code = interaction_getter(fields, field, vector_set, native)
        attr = u2(2) + u2(2) + u4(len(code)) + code + u2(0) + u2(0)
        methods.extend(u2(2) + u2(name) + u2(desc_id) + u2(1) + u2(find("'Code'")) + u4(len(attr)) + attr)
    return (data[:8] + u2(next_index) + data[10:cp_end] + additions +
            data[cp_end:fields_at] + u2(field_count + 2) + data[fields_at+2:fields_end] +
            extra_fields + methods + data[r.stream.tell():])

def redirect_save_thumbnail(code: bytes, native_ref: int, cleanup_ref: int) -> bytes:
    if len(code) != 270 or code[94:105] != (
            b'\x2a\x2d\x11\x02\x00\x11\x01\x00\xb6'+u2(native_ref)):
        raise ValueError('Unknown QuickSave thumbnail call boundary')
    # Same-length replacement preserves save flow, branches and debug offsets.
    return code[:102]+b'\xb7'+u2(cleanup_ref)+code[105:]

def patch_save_module_class(data: bytes) -> bytes:
    if hashlib.sha256(data).hexdigest() != SAVE_MODULE_SHA256:
        raise ValueError('Unknown LawmanModuleSingle.class SHA-256; no mutation')
    r=Reader(data);r.read(8);pool=parse_constant_pool(r)
    cp_end=r.stream.tell();extra=bytearray();next_index=len(pool.entries)
    def append(raw):
        nonlocal next_index
        index=next_index;next_index+=1;extra.extend(raw);return index
    def utf8(value):
        raw=value.encode();return append(b'\x01'+u2(len(raw))+raw)
    def find(description):
        return next(i for i in range(1,len(pool.entries)) if pool.entries[i] is not None
                    and pool.describe(i)==description)
    desc='(Ljava/lang/String;II)V'
    name_id,desc_id=utf8('cojvrPrepareSaveThumbnail'),utf8(desc)
    nt=append(b'\x0c'+u2(name_id)+u2(desc_id))
    helper_ref=append(b'\x0a'+u2(find('LawmanModuleSingle'))+u2(nt))
    native_ref=find('LawmanModuleSingle.TakeScreenshot'+desc)
    r.read(6);r.read(r.u2()*2)
    for _ in range(r.u2()):
        r.read(6)
        for _ in range(r.u2()):r.u2();r.read(r.u4())
    methods_at=r.stream.tell();count=r.u2();methods=bytearray(u2(count+1))
    cleanup=None;patched=0
    for _ in range(count):
        header=r.read(6);methods.extend(header)
        name=pool.utf8(int.from_bytes(header[2:4],'big'))
        descriptor=pool.utf8(int.from_bytes(header[4:6],'big'))
        attributes=r.u2();methods.extend(u2(attributes))
        for _ in range(attributes):
            index=r.u2();attr=r.read(r.u4())
            if pool.utf8(index)=='Code':
                cr=Reader(attr);stack,locals_=cr.u2(),cr.u2();code=cr.read(cr.u4())
                if name=='TakeScreenshot' and descriptor==desc:
                    if len(code)!=61 or code[47]!=0xb2 or code[57]!=0xb8 or code[60]!=0xb1:
                        raise ValueError('Unknown thumbnail cleanup boundary')
                    # Preserve native filename construction and stale-preview
                    # deletion; omit only the queued GPU capture and ForceRender.
                    cleanup=(stack,locals_,code[:47]+b'\xb1')
                if name=='QuickSave' and descriptor=='(I)V':
                    code=redirect_save_thumbnail(code,native_ref,helper_ref)
                    attr=attr[:8]+code+attr[8+len(code):];patched+=1
            methods.extend(u2(index)+u4(len(attr))+attr)
    if patched!=1 or cleanup is None:raise ValueError('Incomplete save-thumbnail boundary patch')
    stack,locals_,code=cleanup
    attr=u2(stack)+u2(locals_)+u4(len(code))+code+u2(0)+u2(0)
    methods.extend(u2(2)+u2(name_id)+u2(desc_id)+u2(1)+u2(find("'Code'"))+u4(len(attr))+attr)
    return data[:8]+u2(next_index)+data[10:cp_end]+extra+data[cp_end:methods_at]+methods+data[r.stream.tell():]

def whip_pose_prefix(fields, owner, net, in_hands, native_fields, vector_set, vector_cross):
    # Consumed only by the naturally scheduled ComputeWhipPosition. No ODE,
    # update or draw calls are injected; its caller retains all cloth ownership.
    c=Code()
    for field in fields:
        c.emit(0x2a);c.ref(0xb4,field);c.branch(0xc6,'native')
    c.emit(0x2a);c.ref(0xb4,owner);c.branch(0xc6,'native')
    c.emit(0x2a);c.ref(0xb4,owner);c.ref(0xb4,net);c.branch(0x9a,'native')
    c.emit(0x2a);c.ref(0xb4,in_hands);c.branch(0x99,'native')
    for target,source in zip(native_fields,fields):
        c.ref(0xb2,target);c.emit(0x2a);c.ref(0xb4,source);c.ref(0xb6,vector_set)
    # Native loop shaping also consumes s_vLeft (holding-socket Z). Complete
    # its rigid basis from the published socket X/Y; keep the cloth caller intact.
    c.ref(0xb2,native_fields[3]);c.ref(0xb2,native_fields[1]);c.ref(0xb2,native_fields[2])
    c.ref(0xb6,vector_cross)
    c.emit(0xb1);c.label('native')
    return c.finish()

def patch_whip_class(data: bytes) -> bytes:
    if hashlib.sha256(data).hexdigest()!=WHIP_CLASS_SHA256:
        raise ValueError('Unknown WeaponWhip.class SHA-256; no mutation')
    r=Reader(data);r.read(8);pool=parse_constant_pool(r);cp_end=r.stream.tell()
    extra=bytearray();next_index=len(pool.entries)
    def append(raw):
        nonlocal next_index
        index=next_index;next_index+=1;extra.extend(raw);return index
    def utf8(value):
        raw=value.encode('utf-8');return append(b'\x01'+u2(len(raw))+raw)
    def find(value):
        return next(i for i,e in enumerate(pool.entries) if e is not None and pool.describe(i)==value)
    r.u2();this_class=r.u2();r.u2();r.read(r.u2()*2)
    fields_at=r.stream.tell();field_count=r.u2()
    for _ in range(field_count):
        r.read(6)
        for _ in range(r.u2()): r.u2();r.read(r.u4())
    fields_end=r.stream.tell();descriptor=utf8('LVector;');fields=bytearray();refs=[]
    for name in WHIP_FIELDS:
        index=utf8(name);fields.extend(u2(1)+u2(index)+u2(descriptor)+u2(0))
        nt=append(b'\x0c'+u2(index)+u2(descriptor))
        refs.append(append(b'\x09'+u2(this_class)+u2(nt)))
    apb=append(b'\x07'+u2(utf8('ArmedPlayerBeing')))
    nt=append(b'\x0c'+u2(utf8('m_bNetAttackForced'))+u2(utf8('Z')))
    net=append(b'\x09'+u2(apb)+u2(nt))
    pre=whip_pose_prefix(refs,find('WeaponWhip.cOwnerAPBLArmedPlayerBeing;'),net,
        find('WeaponWhip.m_bInHandsZ'),[find('WeaponWhip.'+n+'LVector;') for n in
        ('s_vPosition','s_vUp','s_vForward','s_vLeft')],find('Vector.Set(LVector;)V'),
        find('Vector.CalcCross(LVector;LVector;)V'))
    method_count=r.u2();methods=bytearray(u2(method_count));patched=0
    for _ in range(method_count):
        header=r.read(6);name=pool.utf8(int.from_bytes(header[2:4],'big'));desc=pool.utf8(int.from_bytes(header[4:6],'big'))
        methods.extend(header);count=r.u2();methods.extend(u2(count))
        for _ in range(count):
            index=r.u2();attr=r.read(r.u4())
            if (name,desc)==('ComputeWhipPosition','()V') and pool.utf8(index)=='Code':
                cr=Reader(attr);stack,locals_=cr.u2(),cr.u2();code=cr.read(cr.u4())
                if cr.u2()!=0: raise ValueError('Unexpected whip exception table')
                code=pre+code
                attr=u2(max(3,stack))+u2(locals_)+u4(len(code))+code+u2(0)+u2(0);patched+=1
            methods.extend(u2(index)+u4(len(attr))+attr)
    if patched!=1: raise ValueError('Incomplete exact whip pose boundary')
    return (data[:8]+u2(next_index)+data[10:cp_end]+extra+data[cp_end:fields_at]+
        u2(field_count+len(WHIP_FIELDS))+data[fields_at+2:fields_end]+fields+methods+data[r.stream.tell():])

def patch_archive(source: Path, output: Path):
    if source.resolve() == output.resolve() or output.exists():
        raise ValueError("Patch output must be a new separate file")
    if hashlib.sha256(source.read_bytes()).hexdigest() != PAK_SHA256:
        raise ValueError("Unknown code.pak SHA-256; no mutation")
    with zipfile.ZipFile(source) as original:
        if len(original.namelist()) != len(set(original.namelist())):
            raise ValueError("Duplicate archive entries")
        patched = {"ArmedPlayerBeing.class":patch_class(original.read("ArmedPlayerBeing.class")),
                   "WeaponFire.class":patch_fire_class(original.read("WeaponFire.class")),
                   "BeingTriggered.class":patch_triggered_class(original.read("BeingTriggered.class")),
                   "LawmanModuleSingle.class":patch_save_module_class(original.read("LawmanModuleSingle.class")),
                   "WeaponWhip.class":patch_whip_class(original.read("WeaponWhip.class"))}
        with zipfile.ZipFile(output, "x") as result:
            result.comment = original.comment
            for info in original.infolist():
                result.writestr(info, patched[info.filename] if info.filename in patched else original.read(info))
    with zipfile.ZipFile(source) as original, zipfile.ZipFile(output) as result:
        if result.testzip() or result.namelist() != original.namelist():
            raise ValueError("Patched archive integrity failure")
        for name in original.namelist():
            if name not in patched and result.read(name) != original.read(name):
                raise ValueError("Unrelated archive content changed")

if __name__ == "__main__":
    p = argparse.ArgumentParser(); p.add_argument("source", type=Path); p.add_argument("output", type=Path)
    a = p.parse_args(); patch_archive(a.source, a.output)
    print("Exact shot, interaction and procedural-whip consumers patched; other payloads verified unchanged")
