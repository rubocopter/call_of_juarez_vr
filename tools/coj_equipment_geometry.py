#!/usr/bin/env python3
"""Exact CoJ equipment geometry, offline only. No setters, JVM or extraction.

Row vectors, native centimetres: G * inverse(H) * B. Full affine bases are
retained, including positive scale. This is geometry, never contact admission.
Use explicit original backup paths while staged. Evidence output stays in work/.
"""
from __future__ import annotations

import argparse
from contextlib import ExitStack
from dataclasses import dataclass
import hashlib
import json
import math
from pathlib import Path
import struct
import sys
import zipfile

from split_coj_player_mesh import chunks
from inspect_java_bytecode import parse_class, disassemble

ARCHIVES = {'code': 'f9db47c166e03f23e37cbcdfd5344e4ad4c5c9134f35e8f6dcdf66db7e71ce12',
            'data0': '5eb6ec4856e53226dfabad929522706a21f02d428923a8c9c38804cc2cf11a94'}
BODY_HASHES = {'Ray':'9b3f4579cd32476f866dc1aad5a1bd0f1bc913449548f40f7c76bd37cd8d33b5',
               'Billy':'ce1e70dfab7ddae4f4036518820e34c84a18d9dc18ab48163815d4ac1dd15be7'}
WEAPONS = {
    'peacemaker':('ColtPeacemaker/ColtPeacemaker.msh','0df76de44edefcba8ea640b85f119d5dbd20a31c1d2df7d1ab1e1c909aff63f0'),
    'frontier':('ColtFrontier/ColtFrontier.MSH','6d4577416a2c775783bed8294c3d72b3cf966381a5c2e6779a23761f3ded6044'),
    'walker':('ColtWalker/ColtWalker.MSH','9ae885d726a94c9f34d26a42de9066832ab8bfe6b79d9d3a410b8e1ac2a98a8b'),
    'cogswell':('Cogswell/Cogswell.MSH','5b159e7a114986ef23e5e9de77277fe85e59089c5dabcf011ca1990bee31bae4')}
BOW_HASH = 'd16928ae927620f567f8384c65c190459460d042ac4cf8b05e9e8eb3d2e27d53'
ARROW_HASH = 'b13419d78caa2adf54ce3b1e68632b8b79a75ac45f3c29be336dc79767654bde'
SCOPED_RIFLE_HASH = 'a989d5c4bcaad39e250a0fcaa070c985ec832ec51e2f48e6f2d62eb580d532cd'


def identity():
    return [[float(i==j) for j in range(4)] for i in range(4)]


def multiply(a, b):
    return [[sum(a[i][k]*b[k][j] for k in range(4)) for j in range(4)] for i in range(4)]


def validate_frame(m):
    if len(m)!=4 or any(len(r)!=4 for r in m) or any(not math.isfinite(v) for r in m for v in r):
        raise ValueError('nonfinite or malformed frame')
    if any(abs(m[i][3])>1e-9 for i in range(3)) or abs(m[3][3]-1)>1e-9:
        raise ValueError('projective frame')
    scales=[math.sqrt(sum(v*v for v in row[:3])) for row in m[:3]]
    if any(s<1e-4 or s>1e4 for s in scales):
        raise ValueError('degenerate or excessive scale')
    a,b,c=m[:3]
    det=a[0]*(b[1]*c[2]-b[2]*c[1])-a[1]*(b[0]*c[2]-b[2]*c[0])+a[2]*(b[0]*c[1]-b[1]*c[0])
    if det/math.prod(scales)<1e-6:
        raise ValueError('reflected or ill-conditioned frame')


def inverse(m):
    validate_frame(m)
    # Full inverse, not transpose: authored scale is not silently discarded.
    a=[list(row)+ident for row,ident in zip(m,identity())]
    for j in range(4):
        at=max(range(j,4),key=lambda i:abs(a[i][j]))
        a[j],a[at]=a[at],a[j]
        if abs(a[j][j])<1e-12:raise ValueError('singular frame')
        scale=a[j][j];a[j]=[v/scale for v in a[j]]
        for i in range(4):
            if i!=j:
                scale=a[i][j];a[i]=[v-scale*w for v,w in zip(a[i],a[j])]
    out=[row[4:] for row in a]
    validate_frame(out)
    for product in (multiply(m,out),multiply(out,m)):
        if max(abs(product[i][j]-float(i==j)) for i in range(4) for j in range(4))>1e-6:
            raise ValueError('inverse recomposition failed')
    return out


def compose_holster(holder, holster, body_mount):
    for m in (holder,holster,body_mount):validate_frame(m)
    weapon=multiply(inverse(holster),body_mount)
    grasp=multiply(holder,weapon)
    validate_frame(weapon);validate_frame(grasp)
    return weapon,grasp


def remap_element(source_root,target_root,element):
    for m in (source_root,target_root,element):validate_frame(m)
    result=multiply(multiply(element,inverse(source_root)),target_root)
    validate_frame(result)
    return result


def body_mount(sha256, side):
    if sha256.lower()==BODY_HASHES['Ray'] and side in ('right','left'):
        return ('HolsterR',14) if side=='right' else ('HolsterL',8)
    if sha256.lower()==BODY_HASHES['Billy'] and side=='right':return ('HolsterR',16)
    raise ValueError('unknown body or absent mount; no mirror/fallback')


def weapon_profile(sha256):
    for name,(_,digest) in WEAPONS.items():
        if sha256.lower()==digest:return name
    raise ValueError('unknown weapon mesh; filenames/classes are not identities')


@dataclass
class BowSample:
    """Copied current values from ONE natural owner/frame, never native handles.

    A future adapter must read m_cArrowMesh, cOwner, native parent/mount, exact
    class/model, active/renderable state and repeat them around the frame reads.
    This helper neither acquires that snapshot nor qualifies native lifetime.
    """
    actor: int
    bow: int
    arrow: int
    parent: int
    generation: int
    frame: int
    parent_element: int
    resolved_right_element: int
    bow_sha256: str
    arrow_sha256: str
    hand: list
    root: list
    holder: list
    mesh: list
    player_sha256: str = ''
    exact_weapon_bow: bool = False
    visible: bool = True
    holder_element: int = -1
    mesh_element: int = -1


def observe_bow(before,after):
    if before!=after:raise ValueError('incoherent identity/frame/value read')
    s=before
    if (not s.exact_weapon_bow or not s.visible or s.bow_sha256.lower()!=BOW_HASH or
        s.arrow_sha256.lower()!=ARROW_HASH or s.player_sha256.lower() not in BODY_HASHES.values()):
        raise ValueError('unqualified bow/arrow/player profile or visibility')
    if (any(not 0<v<0xffffffff for v in (s.actor,s.bow,s.arrow,s.parent)) or
        len({s.actor,s.bow,s.arrow})!=3 or s.parent!=s.actor or s.generation<=0 or s.frame<=0 or
        s.parent_element<0 or s.parent_element!=s.resolved_right_element or
        s.holder_element!=0 or s.mesh_element!=1):
        raise ValueError('unavailable/replaced owner or hierarchy')
    for m in (s.hand,s.root,s.holder,s.mesh):validate_frame(m)
    return {'root_in_hand':multiply(s.root,inverse(s.hand)),
            'holder_in_root':multiply(s.holder,inverse(s.root)),
            'mesh_in_root':multiply(s.mesh,inverse(s.root)),
            'writer_admitted':False, 'string_writer_admitted':False}


def mesh_frames(data):
    if len(data)<24 or struct.unpack_from('<4I',data)!=(0x48534d,0,len(data),8):
        raise ValueError('unsupported mesh root')
    nodes=[]
    for kind,offset,size,header in chunks(data,24,len(data)):
        if kind!=1:continue
        if header<140:raise ValueError('short mesh node header')
        name=data[offset+20:offset+52].split(b'\0')[0].decode('ascii')
        matrix=list(struct.unpack_from('<16f',data,offset+92))
        local=[matrix[i:i+4] for i in range(0,16,4)]
        validate_frame(local)
        nodes.append({'name':name,'index':len(nodes),'file_offset':offset,
                      'kind':struct.unpack_from('<I',data,offset+16)[0],
                      'parent_index':struct.unpack_from('<H',data,offset+84)[0],
                      'local':local,'child_kinds':[c[0] for c in chunks(data,offset+16+header,offset+size)]})
    if len(nodes)!=struct.unpack_from('<I',data,16)[0] or len({n['name'].casefold() for n in nodes})!=len(nodes):
        raise ValueError('node count or duplicate name')
    def world(index,seen):
        if index in seen:raise ValueError('mesh hierarchy cycle')
        if not 0<=index<len(nodes):raise ValueError('invalid mesh parent')
        n=nodes[index];p=n['parent_index']
        if len(seen)>256:raise ValueError('excessive mesh hierarchy depth')
        return n['local'] if p==65535 else multiply(n['local'],world(p,seen+(index,)))
    for n in nodes:
        n['world']=world(n['index'],())
        validate_frame(n['world'])
    return nodes


def bow_skin_profile(data):
    """Inventory original Bow's skin palette without labeling triangles string.

    Node names alone do not qualify string endpoints. Counts retain the exact
    geometry/skin boundary needed to investigate authored draw channels later.
    """
    nodes=mesh_frames(data)
    named={n['name']:n for n in nodes}
    node=named.get('Bow')
    if node is None or node['kind']!=2:raise ValueError('missing skinned Bow node')
    offset=node['file_offset']
    _,_,size,header=struct.unpack_from('<4I',data,offset)
    geometry=list(chunks(data,offset+16+header,offset+size))
    if len(geometry)!=1 or geometry[0][0]!=256 or geometry[0][3]!=16:
        raise ValueError('unsupported bow skin geometry')
    _,gp,gs,gh=geometry[0]
    nv,ni,nb,morphs=struct.unpack_from('<4I',data,gp+16)
    if morphs or nv<1 or ni%3 or nb<1:raise ValueError('invalid bow skin counts')
    sub=list(chunks(data,gp+16+gh,gp+gs));arrays={k:(p+16,h) for k,p,s,h in sub}
    if len(arrays)!=len(sub) or any(k not in arrays for k in (0x130,0x140,0x151)):
        raise ValueError('missing/duplicate bow skin arrays')
    wp,wn=arrays[0x130];ip,ins=arrays[0x140];q,palette_bytes=arrays[0x151];end=q+palette_bytes
    if wn!=nv*12 or ins!=ni*2:raise ValueError('bow skin array size mismatch')
    indices=struct.unpack_from('<'+'H'*ni,data,ip)
    counts={};batches=[];next_index=0
    for _ in range(nb):
        if q+12>end:raise ValueError('truncated bow palette')
        material,start,count,np=struct.unpack_from('<HIIH',data,q);q+=12
        if np<1 or start!=next_index or count%3 or start+count>ni or q+np*2>end:
            raise ValueError('invalid bow batch')
        palette=struct.unpack_from('<'+'H'*np,data,q);q+=np*2
        if any(i>=len(nodes) for i in palette):raise ValueError('invalid bow bone index')
        for vertex in set(indices[start:start+count]):
            if vertex>=nv:raise ValueError('invalid bow vertex')
            bone_indices=struct.unpack_from('<4B',data,wp+vertex*12)
            weights=struct.unpack_from('<4H',data,wp+vertex*12+4)
            if sum(weights)!=32767 or any(i>=np for i,w in zip(bone_indices,weights) if w):
                raise ValueError('invalid bow skin weight')
            for i,w in zip(bone_indices,weights):
                if w:
                    name=nodes[palette[i]]['name'];counts.setdefault(name,set()).add(vertex)
        batches.append({'material_index':material,'first_index':start,'index_count':count,
                        'bone_palette':[nodes[i]['name'] for i in palette]})
        next_index+=count
    if q!=end or next_index!=ni:raise ValueError('incomplete bow palette/index coverage')
    return {'node':'Bow','vertex_count':nv,'index_count':ni,'batches':batches,
            'bone_vertex_counts':{name:len(vertices) for name,vertices in counts.items()},
            'string_endpoints_qualified':False,'string_writer_admitted':False}


def scoped_rifle_contact(data):
    """Original scoped GunBody LOD0 wood island, in owning-element cm.

    The exact 60-face island was identified from welded geometry and original
    atlas inspection. Count is a selector ONLY under this full mesh hash, never
    a heuristic for other models. No runtime/optic/contact-volume qualification.
    """
    if hashlib.sha256(data).hexdigest()!=SCOPED_RIFLE_HASH:
        raise ValueError('unrecognized scoped rifle mesh')
    node=next(n for n in mesh_frames(data) if n['name']=='GunBody')
    p=node['file_offset'];_,_,size,header=struct.unpack_from('<4I',data,p)
    kind,gp,gs,gh=next(chunks(data,p+16+header,p+size))
    if kind!=256 or gh!=16:raise ValueError('unsupported scoped geometry')
    nv,ni,nb,morphs=struct.unpack_from('<4I',data,gp+16)
    arrays={k:(q+16,s-16) for k,q,s,h in chunks(data,gp+16+gh,gp+gs)}
    if morphs or ni%3 or arrays[0x101][1]!=nv*8 or arrays[0x140][1]!=ni*2:
        raise ValueError('invalid scoped geometry counts')
    origin=struct.unpack_from('<3f',data,arrays[0x160][0]+4)
    scale=struct.unpack_from('<f',data,arrays[0x160][0]+16)[0]
    packed=[struct.unpack_from('<4h',data,arrays[0x101][0]+i*8)[:3] for i in range(nv)]
    points=[[v*scale+o for v,o in zip(xyz,origin)] for xyz in packed]
    indices=struct.unpack_from('<'+'H'*ni,data,arrays[0x140][0])
    if any(i>=nv for i in indices):raise ValueError('scoped vertex index out of range')
    bounds=[[f(v[j] for v in points) for j in range(3)] for f in (min,max)]
    centre=struct.unpack_from('<3f',data,p+220);extent=struct.unpack_from('<3f',data,p+232)
    error=max(abs(bounds[k][j]-(centre[j]+(-1 if k==0 else 1)*extent[j])) for k in range(2) for j in range(3))
    if error>.002:raise ValueError('scoped packed-position decode disagrees with node bounds')
    parent=list(range(nv));representatives={}
    def find(i):
        while parent[i]!=i:parent[i]=parent[parent[i]];i=parent[i]
        return i
    def union(i,j):parent[find(i)]=find(j)
    for i,xyz in enumerate(packed):
        if xyz in representatives:union(i,representatives[xyz])
        else:representatives[xyz]=i
    triangles=[indices[i:i+3] for i in range(0,ni,3)]
    for a,b,c in triangles:union(a,b);union(a,c)
    groups={}
    for i,tri in enumerate(triangles):groups.setdefault(find(tri[0]),[]).append(i)
    candidates=[tis for tis in groups.values() if len(tis)==60]
    if len(candidates)!=1:raise ValueError('ambiguous scoped wood island')
    tis=candidates[0];vertices=sorted({v for i in tis for v in triangles[i]})
    bounds=[[f(points[i][j] for i in vertices) for j in range(3)] for f in (min,max)]
    return {'element':node['name'],'element_index':node['index'],'lod':0,
            'triangle_count':len(tis),'triangle_indices':tis,'vertex_indices':vertices,
            'bounds_cm':bounds,'point_cm':[(a+b)*.5 for a,b in zip(*bounds)],
            'packed_position_origin':origin,'packed_position_scale':scale,
            'node_bounds_error_cm':error,'contact_volume_qualified':False}


def file_digest(handle):
    handle.seek(0)
    digest=hashlib.file_digest(handle,'sha256').hexdigest()
    handle.seek(0)
    return digest


class OriginalArchive:
    def __init__(self,handle,label):
        self.label=label
        if file_digest(handle)!=ARCHIVES[label]:raise ValueError('unrecognized original '+label)
        self.zip=zipfile.ZipFile(handle)
        self.members={}
        for info in self.zip.infolist():
            key=info.filename.replace('\\','/').casefold()
            if key in self.members:raise ValueError('ambiguous archive member')
            self.members[key]=info

    def read(self,name):
        info=self.members.get(name.casefold())
        if info is None or info.file_size>64*1024*1024:raise ValueError('missing/oversized member '+name)
        data=self.zip.read(info)
        if len(data)!=info.file_size:raise ValueError('member size mismatch')
        return data,{'member':info.filename,'sha256':hashlib.sha256(data).hexdigest(),'size':len(data)}


def build_inventory(code_path,data0_path):
    with ExitStack() as stack:
        handles={k:stack.enter_context(Path(p).open('rb')) for k,p in [('code',code_path),('data0',data0_path)]}
        archives={k:OriginalArchive(h,k) for k,h in handles.items()}
        for a in archives.values():stack.callback(a.zip.close)
        meshes={}
        specs={f'Data/Player/{who}/{who}.msh':digest for who,digest in BODY_HASHES.items()}
        specs.update({'Data/Player/Equipment/'+p:h for p,h in WEAPONS.values()})
        specs.update({'Data/Player/Equipment/Bow/Bow.msh':BOW_HASH,'Data/Player/Equipment/Bow/BowArrow.msh':ARROW_HASH})
        for path,expected in specs.items():
            data,source=archives['data0'].read(path)
            if source['sha256']!=expected:raise ValueError('mesh identity mismatch '+path)
            meshes[path]={'source':source,'nodes':mesh_frames(data)}
        holsters=[]
        for model,(suffix,digest) in WEAPONS.items():
            nodes=meshes['Data/Player/Equipment/'+suffix]['nodes'];by_name={n['name']:n for n in nodes}
            holder,holster=by_name['Holder'],by_name['Holster']
            if any(n['kind']!=4 or n['child_kinds'] for n in (holder,holster)):
                raise ValueError('attachment node is not a geometry-free dummy')
            relative=multiply(holder['world'],inverse(holster['world']))
            for who,player_hash in BODY_HASHES.items():
                body=meshes[f'Data/Player/{who}/{who}.msh']['nodes']
                for side in ('left','right'):
                    try:name,index=body_mount(player_hash,side)
                    except ValueError:continue
                    mount=body[index]
                    if mount['name']!=name or mount['kind']!=4 or mount['child_kinds']:
                        raise ValueError('body mount profile mismatch')
                    weapon,grasp=compose_holster(holder['world'],holster['world'],mount['world'])
                    error=max(abs(x-y) for a,b in zip(multiply(holster['world'],weapon),mount['world']) for x,y in zip(a,b))
                    if error>1e-6:raise ValueError('holster recomposition failed')
                    holsters.append({'model':model,'mesh_sha256':digest,'body':who,'side':side,
                        'mount_index':index,'holder_index':holder['index'],'holster_index':holster['index'],
                        'holder':holder['world'],'holster':holster['world'],'grip_in_holster':relative,
                        'grip_distance_cm':math.sqrt(sum(v*v for v in relative[3][:3])),
                        'bind_grasp':grasp,'recomposition_error':error,'contact_admitted':False})
        classes={}
        wanted={'WeaponBow.class':{'OnCreate','OnDestroy','CreateArrowMesh','DestroyArrowMesh','ShowArrowMesh',
                 'HideArrowMesh','UpdateAmmoSkin','DropArrowAmmo','SetAnimSpeed','SetAnimMorphTime','PlayAnim','SGLoadChunk','SGSaveChunk'},
                'InvObject.class':{'ShowInvObject','GetElementID','GetHolsterElementID','GetOwnerElementIDFromSlot','InvObjectInInventory'}}
        for name,methods in wanted.items():
            data,source=archives['code'].read(name);pool,parsed=parse_class(data)
            classes[name]={'source':source,'methods':{m.name+m.descriptor:disassemble(m.code,pool) for m in parsed if m.code and m.name in methods}}
        # Keep authored string/bone/arrow animation as metadata, never pretend a
        # named dummy identifies a physical string endpoint or draw parameter.
        bow_data,_=archives['data0'].read('Data/Player/Equipment/Bow/Bow.msh')
        skin=bow_skin_profile(bow_data)
        scoped_data,scoped_source=archives['data0'].read('Data/Player/Equipment/WinchesterScope/WinchesterScope.msh')
        scoped_contact=scoped_rifle_contact(scoped_data)
        scoped_contact['source']=scoped_source
        authored=[]
        for info in archives['data0'].members.values():
            if '/equipment/bow/' in info.filename.lower() and info.filename.lower().endswith(('.anm','.scr')):
                _,source=archives['data0'].read(info.filename);authored.append(source)
        for key,handle in handles.items():
            if file_digest(handle)!=ARCHIVES[key]:raise ValueError('original changed during inventory')
        return {'schema_version':1,'mode':'offline_geometry_only','archives':ARCHIVES,
                'meshes':meshes,'holsters':holsters,'classes':classes,'bow_authored_sources':authored,
                'bow_skin_profile':skin,
                'scoped_rifle_contact':scoped_contact,
                'runtime_writer_admitted':False,'contact_admitted':False,
                'residual_gates':['Arrow native deletion/save-load lifetime and same-frame natural parent/element correlation',
                    'DropArrowAmmo consumes arrow object world position/forward: exclude gameplay consumers during a visual transaction',
                    'Bow string skin/channel/endpoint ownership and event-free consumer evidence',
                    'Holstered native weapon parent/mount/state/mesh identity and live Holder/Holster recomposition']}


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--code-pak',type=Path,required=True)
    parser.add_argument('--data0-pak',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args(argv)
    work=Path(__file__).resolve().parents[1]/'work'
    if not args.output.resolve().is_relative_to(work.resolve()):raise ValueError('output must stay under ignored work/')
    report=build_inventory(args.code_pak,args.data0_pak)
    # Exclusive creation: never replace a candidate, asset, journal or evidence.
    with args.output.open('x',encoding='utf-8') as stream:json.dump(report,stream,indent=2,allow_nan=False)
    print(f"Verified {len(report['meshes'])} exact meshes and {len(report['holsters'])} holster recompositions; runtime writers remain unadmitted")


if __name__=='__main__':main()
