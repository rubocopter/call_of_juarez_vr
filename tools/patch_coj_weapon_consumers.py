"""Exact CoJ Java 1.4 shot/visual/effects patch, staged/restored with code.pak.

Nullable instance fields opt only the bridge-owned player into tracked rays.
The untouched original methods handle other players, missing tracking and net fire.
"""
from __future__ import annotations
import argparse
import hashlib
import struct
import zipfile
from pathlib import Path
from inspect_java_bytecode import Reader, parse_constant_pool

CLASS_SHA256 = "039b0c12682b99b6560d4597737d794acd4a2b4bb71060c1dc70a8b79ffc4478"
FIRE_CLASS_SHA256 = "d12b513c32db6e3223348f7058799c3702ea3a78746817753998f0de13c9c421"
PAK_SHA256 = "f9db47c166e03f23e37cbcdfd5344e4ad4c5c9134f35e8f6dcdf66db7e71ce12"
FIELDS = ("cojvrRightOrigin", "cojvrLeftOrigin", "cojvrRightDirection", "cojvrLeftDirection")

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
                       origins, directions, effects, create_method):
    c=Code()
    c.emit(0x2a);c.ref(0xb4,owner_field);c.ref(0xc1,player_class);c.branch(0x99,"fallback")
    c.emit(0x2a);c.ref(0xb4,owner_field);c.ref(0xc0,player_class);c.emit(0x4d)
    c.emit(0x2c);c.ref(0xb4,net_field);c.branch(0x9a,"fallback")
    c.emit(0x2c,0x2a);c.ref(0xb6,hand_method);c.emit(0x3e)
    c.emit(0x1d);c.branch(0x99,"right")
    c.emit(0x1d,0x04);c.branch(0xa0,"fallback")
    c.emit(0x2c);c.ref(0xb4,origins[1]);c.emit(0x3a,4)
    c.emit(0x2c);c.ref(0xb4,directions[1]);c.emit(0x3a,5);c.branch(0xa7,"value")
    c.label("right")
    c.emit(0x2c);c.ref(0xb4,origins[0]);c.emit(0x3a,4)
    c.emit(0x2c);c.ref(0xb4,directions[0]);c.emit(0x3a,5)
    c.label("value")
    c.emit(0x19,4);c.branch(0xc6,"fallback")
    c.emit(0x19,5);c.branch(0xc6,"fallback")
    for i,(emitter,particle) in enumerate(effects):
        c.emit(0x2b);c.ref(0xb4,emitter);c.branch(0x99,f"next{i}")
        c.emit(0x2b);c.ref(0xb4,particle);c.branch(0x99,f"next{i}")
        c.emit(0x2a,0x2b);c.ref(0xb4,emitter)
        c.emit(0x2b);c.ref(0xb4,particle)
        # Explicit world vectors, no attachment to the temporarily restored
        # native weapon. Exact shipped explosion FX demonstrate this route.
        c.emit(0x19,4,0x19,5,0x01,0x02);c.ref(0xb6,create_method);c.emit(0x57)
        c.label(f"next{i}")
    c.emit(0xb1);c.label("fallback")
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
    targets = {
        ("GetFireOriginForWeapon", "(LWeapon;LVector;)Z"): (refs[:2], True),
        ("GetFireOriginVisualizationForHand", "(ILVector;)Z"): (refs[:2], False),
        ("GetBeingLookDirDevForHand", "(ILVector;)Z"): (refs[2:], False),
    }
    method_count = r.u2(); methods = bytearray(u2(method_count+1)); patched = set()
    for _ in range(method_count):
        header = r.read(6); name, desc = pool.utf8(int.from_bytes(header[2:4], "big")), pool.utf8(int.from_bytes(header[4:6], "big"))
        methods.extend(header); count = r.u2(); methods.extend(u2(count))
        for _ in range(count):
            attr_index = r.u2(); attr = r.read(r.u4()); target = targets.get((name, desc))
            if pool.utf8(attr_index) == "Code" and (target or (name,desc)==visual_target):
                cr = Reader(attr); stack, locals_ = cr.u2(), cr.u2(); code = cr.read(cr.u4())
                if cr.u2() != 0: raise ValueError("Unexpected shot exception table")
                if (name,desc)==visual_target:
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
    if patched != set(targets)|{visual_target}: raise ValueError("Incomplete shot boundary patch")
    helper=visual_direction_source(refs[2:],net,native_visual_array)
    attr=u2(2)+u2(3)+u4(len(helper))+helper+u2(0)+u2(0)
    methods.extend(u2(2)+u2(visual_name)+u2(visual_desc)+u2(1)+u2(find("'Code'"))+u4(len(attr))+attr)
    return (data[:8]+u2(next_index)+data[10:cp_end]+additions+
            data[cp_end:fields_at]+u2(field_count+4)+data[fields_at+2:fields_end]+
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
    directions=[ref(9,name,"LVector;") for name in FIELDS[2:]]
    effects=[(find("WpnFXFire.m_nFXCombEmiterDefIDI"),find("WpnFXFire.m_nFXCombParticleDefIDI")),
             (find("WpnFXFire.m_nFXSmokeEmiterDefIDI"),find("WpnFXFire.m_nFXSmokeParticleDefIDI"))]
    pre=fire_effect_prefix(find("WeaponFire.cOwnerLPawnInventory;"),player,net,hand,origins,directions,effects,
        find("WeaponFire.FXCreateParticleEmiter(IILVector;LVector;LControlObject;I)I"))
    r.read(6);r.read(r.u2()*2)
    for _ in range(r.u2()):
        r.read(6)
        for _ in range(r.u2()):r.u2();r.read(r.u4())
    methods_at=r.stream.tell();method_count=r.u2();methods=bytearray(u2(method_count));patched=0
    for _ in range(method_count):
        header=r.read(6);methods.extend(header)
        name=pool.utf8(int.from_bytes(header[2:4],"big"));desc=pool.utf8(int.from_bytes(header[4:6],"big"))
        attr_count=r.u2();methods.extend(u2(attr_count))
        for _ in range(attr_count):
            index=r.u2();attr=r.read(r.u4())
            if name=="ExecFXFire" and desc=="(LWpnFXFire;)V" and pool.utf8(index)=="Code":
                cr=Reader(attr);stack,locals_=cr.u2(),cr.u2();original=cr.read(cr.u4())
                if cr.u2()!=0:raise ValueError("Unexpected fire effect exception table")
                code=pre+original
                attr=u2(max(stack,7))+u2(max(locals_,6))+u4(len(code))+code+u2(0)+u2(0)
                patched+=1
            methods.extend(u2(index)+u4(len(attr))+attr)
    if patched!=1:raise ValueError("Incomplete fire effects patch")
    return data[:8]+u2(next_index)+data[10:cp_end]+additions+data[cp_end:methods_at]+methods+data[r.stream.tell():]

def patch_archive(source: Path, output: Path):
    if source.resolve() == output.resolve() or output.exists():
        raise ValueError("Patch output must be a new separate file")
    if hashlib.sha256(source.read_bytes()).hexdigest() != PAK_SHA256:
        raise ValueError("Unknown code.pak SHA-256; no mutation")
    with zipfile.ZipFile(source) as original:
        if len(original.namelist()) != len(set(original.namelist())):
            raise ValueError("Duplicate archive entries")
        patched = {"ArmedPlayerBeing.class":patch_class(original.read("ArmedPlayerBeing.class")),
                   "WeaponFire.class":patch_fire_class(original.read("WeaponFire.class"))}
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
    print("Exact shot consumers patched; all other archive payloads verified unchanged")
