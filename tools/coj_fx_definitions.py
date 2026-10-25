"""Exact CoJ FX definition pointers across Java signed jint/LAA.

Native FXLoad*Def returns a heap pointer, null on failure. This does not apply
to generated emitter handles, rates, timers or damage. Preserve their checks.
"""
import hashlib
from inspect_java_bytecode import parse_class

FX_EMITTER_SHA256='920777e1953681d22a19b78e1d354f4b50c0e508c14bf8372722d3473ad31bdf'
FIRE_CORE_SHA256='4a18a1051e1c9326d8ff19655ea1dba763ac93a54fbe6ff9374a3e12afc21e56'

def patch_fx_definitions(data):
    identity=hashlib.sha256(data).hexdigest()
    if identity==FX_EMITTER_SHA256:
        targets=(('Start',((11,'m_nFXEmitterDefID'),(18,'m_nFXParticleDefID'))),
                 ('DestroyFXDefs',((4,'m_nFXEmitterDefID'),(23,'m_nFXParticleDefID'))))
    elif identity==FIRE_CORE_SHA256:
        fields=tuple('m_n'+effect+'FX'+kind+'DefID' for effect in ('Smoke','Sparks','Steam')
                     for kind in ('Emitter','Particle'))
        targets=(('Start',tuple(zip((23,30,81,88),fields[:4]))),
                 ('StartSteam',tuple(zip((18,25),fields[4:]))),
                 ('DestroyFXDefs',tuple(zip((8,27,46,65,84,103),fields))))
    else:
        raise ValueError('Unknown FXEmitter.class SHA-256; no mutation')
    from coj_reload_wait_probe import ClassEdit
    pool,methods=parse_class(data)
    edit=ClassEdit(data)
    for name,branches in targets:
        offsets=tuple(at for at,_ in branches)
        original=next(m.code for m in methods if m.name==name and m.descriptor=='()V')
        for at,field in branches:
            ref=int.from_bytes(original[at-2:at],'big')
            if original[at-3]!=0xb4 or original[at]!=0x9e or field not in pool.describe(ref):
                raise ValueError('Unexpected native FX definition branch')
        def change(code,offsets=offsets):
            result=bytearray(code)
            for at in offsets:result[at]=0x99 # ifle -> ifeq; same stack and branches
            return bytes(result)
        edit.changes[name,'()V']=change
    return edit.finish()
