"""Exact-game VR loading continuation. No mission/fade/timer bypass."""
import hashlib
from inspect_java_bytecode import parse_class

LOADING_SHA256 = '8d8f06444c3f0e066598d313e80e8e9c9661cd3e70d20a4310342f3d45aaf848'

def patch_loading_ui(data):
    if hashlib.sha256(data).hexdigest()!=LOADING_SHA256:
        raise ValueError('Unknown GameUILoading.class SHA-256; no mutation')
    from coj_reload_wait_probe import ClassEdit
    from patch_coj_weapon_consumers import Code
    edit=ClassEdit(data);_,methods=parse_class(data)
    originals={(m.name,m.descriptor):m for m in methods}
    field=edit.field('cojvrContinueQueued','Z')
    request=edit.ref('GameUILoading','IsRegisteredKeyRequestExclusive','()Z')
    key=edit.ref('GameUILoading','OnInputKey','(IZC)V')
    # Match native exclusive input delivery. Repeated VR presses may not reset
    # the fade and replace its callback, including before WaitForUserInput.
    c=Code();c.emit(0x2a);c.ref(0xb4,field);c.branch(0x9a,'reject')
    c.emit(0x2a);c.ref(0xb6,request);c.branch(0x99,'reject')
    c.emit(0x2a,0x04);c.ref(0xb5,field)
    c.emit(0x2a,0x04,0x04,0x03);c.ref(0xb6,key);c.emit(0x04,0xac)
    c.label('reject');c.emit(0x03,0xac)
    edit.helper('cojvrContinueLoading','()Z',c.finish(),1)
    # Diagnostic state only: 0 loading, 1 ready, 2 continuation already queued.
    c=Code();c.emit(0x2a);c.ref(0xb4,field);c.branch(0x99,'native')
    c.emit(0x05,0xac);c.label('native');c.emit(0x2a);c.ref(0xb6,request);c.emit(0xac)
    edit.helper('cojvrLoadingState','()I',c.finish(),1)
    for name,desc in (('WaitForUserInput','()V'),('Hide','()V'),('ShowCommon','(Ljava/lang/Object;)V')):
        original=originals[name,desc];helper='cojvrOriginal'+name
        edit.helper(helper,desc,original.code,original.max_locals)
        c=Code();c.emit(0x2a,0x03);c.ref(0xb5,field);c.emit(0x2a)
        if name=='ShowCommon':c.emit(0x2b)
        c.ref(0xb6,edit.ref('GameUILoading',helper,desc));c.emit(0xb1)
        replacement=c.finish();edit.changes[name,desc]=lambda old,replacement=replacement:replacement
    return edit.finish()
