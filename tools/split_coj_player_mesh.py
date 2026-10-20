"""Exact CoJ player split; preserve proportions and scope Ray's free coat in VR."""
import argparse, copy, hashlib, shutil, struct, zipfile
from pathlib import Path

def chunks(data, start, end):
    while start < end:
        if start+16>end:raise ValueError('Truncated mesh chunk')
        kind,version,size,header=struct.unpack_from('<4I',data,start)
        if version!=0 or size<16 or start+size>end or header>size-16:raise ValueError('Invalid mesh chunk')
        yield kind,start,size,header
        start+=size

def split_player_mesh(data, body_name):
    if len(data)<24 or struct.unpack_from('<4I',data)!= (0x48534d,0,len(data),8):
        raise ValueError('Unsupported mesh root')
    root=list(chunks(data,24,len(data)))
    nodes=[c for c in root if c[0]==1]
    if len(nodes)!=struct.unpack_from('<I',data,16)[0]:raise ValueError('Node count mismatch')
    names=[data[p+20:p+52].split(b'\0')[0].decode('ascii') for _,p,_,_ in nodes]
    if len(set(names))!=len(names) or names.count(body_name)!=1 or 'CoJVRHiddenArms' in names:
        raise ValueError('Ambiguous player mesh')
    _,body,size,header=nodes[names.index(body_name)]
    if header!=240 or struct.unpack_from('<I',data,body+16)[0]!=2:raise ValueError('Unsupported body node')
    geometry=list(chunks(data,body+16+header,body+size))
    if len(geometry)!=1 or geometry[0][0]!=256 or geometry[0][3]!=16:raise ValueError('Unsupported body geometry')
    _,gp,gs,gh=geometry[0]
    nv,ni,nb,morphs=struct.unpack_from('<4I',data,gp+16)
    if morphs or nv<1 or ni%3 or nb<1:raise ValueError('Unsupported skin geometry')
    child=list(chunks(data,gp+16+gh,gp+gs)); cs={k:(p+16,h) for k,p,s,h in child}
    if len(cs)!=len(child) or any(k not in cs for k in (0x101,0x130,0x140,0x151)):
        raise ValueError('Missing or duplicate skin arrays')
    if cs[0x101][1]!=nv*12 or cs[0x130][1]!=nv*12 or cs[0x140][1]!=ni*2:
        raise ValueError('Skin array size mismatch')
    ip=cs[0x140][0]; wp=cs[0x130][0]; q=cs[0x151][0]; qe=q+cs[0x151][1]
    indices=struct.unpack_from('<'+'H'*ni,data,ip)
    result=bytearray(data); extra=bytearray(data[body:body+size]); next_index=0; removed=retained=coat_removed=0
    # Exact Ray cloth grid: row 00 is anchored; only simulated distal rows
    # 01/02/03 join the existing scoped hidden node. Billy is unchanged.
    free_coat_bones={f'Lip_Cloak_{side}_{column}0{row}'
        for side in 'LR' for column in 'ABCD' for row in (1,2,3)} if body_name=='RayBody' else set()
    for _ in range(nb):
        if q+12>qe:raise ValueError('Truncated skin batch')
        material,start,count,np=struct.unpack_from('<HIIH',data,q);q+=12
        if start!=next_index or count%3 or start+count>ni or np<1 or q+2*np>qe:
            raise ValueError('Invalid skin batch coverage')
        palette=struct.unpack_from('<'+'H'*np,data,q);q+=2*np
        if any(n>=len(names) for n in palette):raise ValueError('Invalid skin bone palette')
        forbidden={}; free_coat={}; hand_vertices={}
        for ix in set(indices[start:start+count]):
            if ix>=nv:raise ValueError('Invalid vertex index')
            bi=struct.unpack_from('<4B',data,wp+12*ix);weights=struct.unpack_from('<4H',data,wp+12*ix+4)
            if sum(weights)!=32767 or any(j>=np for j,w in zip(bi,weights) if w):
                raise ValueError('Invalid vertex skin weights')
            influences=[names[palette[j]] for j,w in zip(bi,weights) if w]
            hand=any(n.startswith(('Bip01 L Hand','Bip01 R Hand','Bip01 L Finger','Bip01 R Finger')) for n in influences)
            arm=any(n.startswith(('Bip01 L UpperArm','Bip01 R UpperArm','Bip01 L Fore','Bip01 R Fore')) for n in influences)
            if hand:
                side=next(prefix for prefix in ('Bip01 L ','Bip01 R ')
                    if any(n.startswith((prefix+'Hand',prefix+'Finger')) for n in influences))
                lower={side+s for s in ('Forearm','ForeTwist','ForeTwist1','Hand',
                    'Finger0','Finger01','Finger02','Finger1','Finger11','Finger12',
                    'Finger2','Finger21','Finger22','Finger3','Finger31','Finger32',
                    'Finger4','Finger41','Finger42')}
                if any(n not in lower for n in influences):
                    raise ValueError('Hand skin contributor is outside the rigid hand overlay')
            forbidden[ix]=arm and not hand
            free_coat[ix]=any(n in free_coat_bones for n in influences)
            hand_vertices[ix]=hand
        for offset in range(start,start+count,3):
            tri=indices[offset:offset+3]
            is_arm=any(forbidden[ix] for ix in tri)
            is_coat=any(free_coat[ix] for ix in tri)
            if is_coat and any(hand_vertices[ix] for ix in tri):
                raise ValueError('Ray cloth partition overlaps retained hand geometry')
            # Keep draw/vertex counts and all palette/bind data unchanged.
            hidden=is_arm or is_coat
            destination=result if hidden else extra
            at=ip+2*offset if hidden else ip-body+2*offset
            struct.pack_into('<3H',destination,at,tri[0],tri[0],tri[0])
            removed+=is_arm;coat_removed+=is_coat and not is_arm;retained+=not hidden
        next_index=start+count
    if next_index!=ni or q!=qe or removed==0 or retained==0:raise ValueError('Incomplete arm partition')
    extra[20:52]=b'CoJVRHiddenArms'.ljust(32,b'\0')
    # Append without relocating any existing node or auxiliary file offsets.
    result.extend(extra)
    struct.pack_into('<I',result,8,len(result));struct.pack_into('<I',result,16,len(nodes)+1)
    return bytes(result),{'arm_triangles':removed,'free_coat_triangles':coat_removed,
        'hidden_triangles':removed+coat_removed,'retained_triangles':retained}

MESH_HASHES = {
    'Ray':'9b3f4579cd32476f866dc1aad5a1bd0f1bc913449548f40f7c76bd37cd8d33b5',
    'Billy':'ce1e70dfab7ddae4f4036518820e34c84a18d9dc18ab48163815d4ac1dd15be7',
}

def patch_archive(source, output, independent_hands=True, native_settings=False):
    source,output=Path(source),Path(output)
    if source.resolve()==output.resolve() or output.exists():raise ValueError('Output must be a new separate archive')
    if not independent_hands and not native_settings:raise ValueError('No resource transformation selected')
    paths={f'Data/Player/{name}/{name}.msh':name for name in ('Ray','Billy')} if independent_hands else {}
    with zipfile.ZipFile(source) as original:
        if len(original.namelist())!=len(set(original.namelist())):raise ValueError('Duplicate archive entries')
        changes={}
        for path,name in paths.items():
            data=original.read(path)
            if hashlib.sha256(data).hexdigest()!=MESH_HASHES.get(name):raise ValueError('Unknown player mesh SHA-256')
            changes[path]=split_player_mesh(data,name+'Body')[0]
        if native_settings:
            from coj_native_settings import MENU_SCRIPT_NAME,patch_menu_script
            changes[MENU_SCRIPT_NAME]=patch_menu_script(original.read(MENU_SCRIPT_NAME))
        infos={i.filename:copy.copy(i) for i in original.infolist()}; order=original.namelist(); prefix=original.start_dir
    shutil.copyfile(source,output)
    try:
        # Retain every original compressed record byte-for-byte. Replaced mesh
        # records become unreferenced; the central directory has no duplicates.
        with zipfile.ZipFile(output,'a') as result:
            result.filelist=[i for i in result.filelist if i.filename not in changes]
            for path in changes:result.NameToInfo.pop(path)
            for path,data in changes.items():result.writestr(infos[path],data)
            by_name={i.filename:i for i in result.filelist};result.filelist=[by_name[n] for n in order]
        with source.open('rb') as a,output.open('rb') as b:
            remaining=prefix
            while remaining:
                count=min(1024*1024,remaining)
                if a.read(count)!=b.read(count):raise ValueError('Unrelated compressed archive data changed')
                remaining-=count
        with zipfile.ZipFile(source) as original,zipfile.ZipFile(output) as result:
            if result.namelist()!=order:raise ValueError('Archive entry identity changed')
            for name in order:
                a,b=original.getinfo(name),result.getinfo(name)
                if name in changes:
                    if result.read(name)!=changes[name]:raise ValueError('Mesh publication mismatch')
                elif (a.CRC,a.compress_size,a.file_size,a.header_offset)!=(b.CRC,b.compress_size,b.file_size,b.header_offset):
                    raise ValueError('Unrelated archive record changed')
    except Exception:
        output.unlink(missing_ok=True);raise

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('source',type=Path);parser.add_argument('output',type=Path)
    parser.add_argument('--native-settings',action='store_true')
    parser.add_argument('--settings-only',action='store_true',help='Keep original player meshes; requires --native-settings')
    args=parser.parse_args()
    if args.settings_only and not args.native_settings:parser.error('--settings-only requires --native-settings')
    patch_archive(args.source,args.output,not args.settings_only,args.native_settings)
    print('Selected exact player/menu resources transformed; other compressed archive records unchanged')
