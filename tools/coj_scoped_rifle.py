"""Exact native desktop scope-mask suppression for a locally tracked rifle."""
import hashlib
from inspect_java_bytecode import parse_class

CAMERA_SHA256='de67c26449bbf708cecfe09eef920808b56ba7490dec42e5b386edda4b3d40f8'

def patch_scope_camera(data):
    if hashlib.sha256(data).hexdigest()!=CAMERA_SHA256:
        raise ValueError('Unknown FPPCamera.class SHA-256; no mutation')
    from coj_reload_wait_probe import ClassEdit
    from patch_coj_weapon_consumers import Code
    edit=ClassEdit(data);_,methods=parse_class(data)
    original=next(m for m in methods if m.name=='UpdateScopeObject' and m.descriptor=='()V')
    edit.helper('cojvrOriginalUpdateScopeObject','()V',original.code,original.max_locals)
    c=Code()
    being=edit.ref('FPPCamera','GetBeing','()LBeing;')
    player=edit.cls('ArmedPlayerBeing')
    c.emit(0x2a);c.ref(0xb6,being);c.ref(0xc1,player);c.branch(0x99,'native')
    c.emit(0x2a);c.ref(0xb6,being);c.ref(0xc0,player);c.emit(0x4d)
    c.emit(0x2c);c.ref(0xb4,edit.ref('ArmedPlayerBeing','m_bNetAttackForced','Z',9));c.branch(0x9a,'native')
    # The existing bridge clears its ray when tracked ownership ends. Do not
    # alter Being.IsScopeVisible: native alternate fire depends on that state.
    c.emit(0x2c);c.ref(0xb4,edit.ref('ArmedPlayerBeing','cojvrRightOrigin','LVector;',9));c.branch(0xc6,'native')
    actual=edit.ref('ArmedPlayerBeing','GetActualWeaponNotEmpty','(I)LWeapon;')
    c.emit(0x2c,0x03);c.ref(0xb6,actual);c.emit(0x4e)
    c.emit(0x2d);c.branch(0xc6,'native')
    c.emit(0x2d);c.ref(0xb6,edit.ref('java/lang/Object','getClass','()Ljava/lang/Class;'))
    c.ref(0xb6,edit.ref('java/lang/Class','getName','()Ljava/lang/String;'))
    scope_name=edit.append(b'\x08'+edit.utf8('WeaponRifleWinchesterScope').to_bytes(2,'big'))
    c.ref(0x13,scope_name);c.ref(0xb6,edit.ref('java/lang/String','equals','(Ljava/lang/Object;)Z'));c.branch(0x99,'native')
    for name in ('GetDesiredWeapon','GetActiveWeapon'):
        c.emit(0x2c,0x03);c.ref(0xb6,edit.ref('ArmedPlayerBeing',name,'(I)LWeapon;'))
        c.emit(0x2d);c.branch(0xa6,'native')
    for name in ('GetActualWeaponNotEmpty','GetDesiredWeapon','GetActiveWeapon'):
        c.emit(0x2c,0x04);c.ref(0xb6,edit.ref('ArmedPlayerBeing',name,'(I)LWeapon;'));c.branch(0xc7,'native')
    c.emit(0x2a);c.ref(0xb6,edit.ref('FPPCamera','HideScopeObject','()V'));c.emit(0xb1)
    # Camera script owns show/hide, transform, scale and replacement again on
    # the first ordinary update. No native scope object is deleted or moved.
    c.label('native');c.emit(0x2a);c.ref(0xb6,edit.ref('FPPCamera','cojvrOriginalUpdateScopeObject','()V'));c.emit(0xb1)
    wrapper=c.finish();edit.changes['UpdateScopeObject','()V']=lambda old:wrapper
    return edit.finish()
