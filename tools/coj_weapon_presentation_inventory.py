#!/usr/bin/env python3
"""Read-only exact CoJ scope/audio/loading-mouth inventory and copied-value math.

Explicit pinned originals only; no extraction, native calls or runtime admission.
The report records authored geometry and call sites, not live lifetime, acoustic
content, loading clearance or headset validation. JSON output stays in work/.
"""
from __future__ import annotations

import argparse
import ast
from contextlib import ExitStack
import copy
import hashlib
import io
import json
import math
from pathlib import Path
import re
import struct
import wave
import zipfile

from coj_equipment_geometry import ARCHIVES, WEAPONS, mesh_frames, validate_frame
from inspect_java_bytecode import Reader, parse_constant_pool, parse_class, disassemble
from split_coj_player_mesh import chunks

PINNED_ARCHIVES=dict(ARCHIVES,data2='f5f48e4e7361df06695e13a88f5c9aac9d3d86ec34c58c6974d266257d4885fc')
SCOPE_MESH='Data/Player/Equipment/Scope/Scope.MSH'
RIFLE_MESH='Data/Player/Equipment/WinchesterScope/WinchesterScope.msh'
MESH_HASHES={SCOPE_MESH:'9d3bc986a910fe22b470aec981987c1825bb66554819f939e687da9747dc1e2a',
             RIFLE_MESH:'a989d5c4bcaad39e250a0fcaa070c985ec832ec51e2f48e6f2d62eb580d532cd'}
AUDIO_ASSET='Data/Sounds/weapon/Frontier/Frontier_Ammo_Open.wav'
AUDIO_HASH='ef0ee1f5785336a444242c271504cb74e2a44c54e4e4a4b081ea29a603b51aed'
# Existing measured REAR-ring centres, native cm; seed order matches the current
# diagnostic profile, not cylinder occupancy or native chamber order.
MOUTH_PROFILES={
    'peacemaker':{'node':'ColtDrum','gate':'ColtLock','radius':.593,
        'path':'Data/Player/Equipment/ColtPeacemaker/ColtPeacemaker.msh',
        'centres':[[-.000764,1.424893,-2.383278],[-1.234752,.712396,-2.383278],
            [1.233232,.712472,-2.383278],[-1.234718,-.712462,-2.383278],
            [-.000696,-1.424891,-2.383278],[1.233266,-.712394,-2.383278]]},
    'frontier':{'node':'GunDrum','gate':'GunLoader','radius':.519,
        'path':'Data/Player/Equipment/ColtFrontier/ColtFrontier.MSH',
        'centres':[[1.196130,.009727,-2.007212],[.591903,1.036132,-2.008803],
            [-1.185826,-.010422,-2.010485],[.609356,-1.026731,-2.007292],
            [-.598805,1.025625,-2.010424],[-.581604,-1.036808,-2.008913]]}}


def number(value,low,high):
    if isinstance(value,bool) or not isinstance(value,(int,float)) or not math.isfinite(value) or not low<=value<=high:
        raise ValueError('nonfinite/out-of-range numeric value')
    return value


def vector(v):
    if len(v)!=3:raise ValueError('expected three components')
    return [number(x,-1e9,1e9) for x in v]


def scope_zoom(authored_zoom,default_fov_degrees):
    """Native replacement zoom normalization; no scope/cache/visibility admission."""
    z=number(authored_zoom,1e-6,1e6);f=number(default_fov_degrees,1e-6,179.999999)
    return z*math.tan(math.radians(f/2))/math.tan(math.radians(40))


def scale_tangents(eyes,zoom):
    """Offline copied-eye tangent experiment; metadata/poses remain independent."""
    z=number(zoom,1e-6,1e6)
    if len(eyes)!=2:raise ValueError('two eyes required')
    out=copy.deepcopy(eyes)
    for eye in out:
        t=eye.get('tangents',[])
        if len(t)!=4 or any(isinstance(v,bool) or not isinstance(v,(int,float)) or not math.isfinite(v) for v in t):
            raise ValueError('invalid eye tangents')
        if not t[0]<0<t[1] or not t[2]<0<t[3]:raise ValueError('unordered signed tangents')
        eye['tangents']=[v/z for v in t]
        if any(not math.isfinite(v) for v in eye['tangents']):raise ValueError('tangent scaling overflow')
    return out


def scope_position(position,forward,distance):
    """Observed native translation only: position - cameraForward * distance.

    This does not reconstruct a native basis or qualify per-eye mesh placement.
    """
    position,forward=vector(position),vector(forward)
    if abs(sum(v*v for v in forward)-1)>1e-6:raise ValueError('non-unit camera direction')
    number(distance,1e-6,1e6)
    return [p-f*distance for p,f in zip(position,forward)]


def wav_profile(data):
    if len(data)>32*1024*1024:raise ValueError('audio size bound')
    try:
        with wave.open(io.BytesIO(data),'rb') as w:
            channels,width,rate,frames,compression,_=w.getparams()
            if compression!='NONE' or width!=2 or channels not in (1,2) or not 1<=rate<=384000 or frames<1:
                raise ValueError('unsupported PCM audio profile')
            pcm=w.readframes(frames)
    except (wave.Error,EOFError,struct.error) as exc:raise ValueError('invalid WAV') from exc
    if len(pcm)!=frames*channels*width:raise ValueError('truncated PCM')
    values=[v[0]/32768 for v in struct.iter_unpack('<h',pcm)]
    # Physical envelope only; bin count/peaks are NOT cartridge clicks or events.
    bins=[];step=max(1,rate//100)*channels
    for start in range(0,len(values),step):
        group=values[start:start+step]
        bins.append({'start_seconds':start/channels/rate,'rms':math.sqrt(sum(v*v for v in group)/len(group))})
    return {'channels':channels,'sample_width':width,'sample_rate':rate,'frames':frames,
        'seconds':frames/rate,'peak_normalized':max(abs(v) for v in values),
        'rms_normalized':math.sqrt(sum(v*v for v in values)/len(values)),
        'envelope_bins':bins,'acoustic_content':'unqualified','duplication_proven':False}


def predicted_overlap(body_entry_seconds,clip_seconds,insertion_seconds):
    start=number(body_entry_seconds,0,1e12);duration=number(clip_seconds,0,1e6)
    insertion=number(insertion_seconds,start,1e12)
    return {'body_clip_remaining_seconds':max(0,start+duration-insertion),
            'mode':'predicted_nominal_window_only','duplication_proven':False,
            'additional_sound_admitted':False}


def soundbuffer_accept(candidate,prefix):
    """Exact SoundBuffer.Accept(String,String) predicate on copied strings.

    Case-sensitive lastIndexOf; caller owns the native search's lowercase/cache
    contract. At most 7 characters may follow prefix, including .wav/.ogg.
    This predicts eligibility, never VFS ordering, selection or playback.
    """
    if not isinstance(candidate,str) or not isinstance(prefix,str) or not prefix:
        raise ValueError('invalid sound candidate/prefix')
    at=candidate.rfind(prefix)
    return at>=0 and len(candidate)-at<=len(prefix)+7 and candidate.endswith(('.wav','.ogg'))


def solve3(a,b):
    rows=[list(row)+[v] for row,v in zip(a,b)]
    for j in range(3):
        pivot=max(range(j,3),key=lambda i:abs(rows[i][j]))
        if abs(rows[pivot][j])<1e-10:raise ValueError('degenerate ring')
        rows[j],rows[pivot]=rows[pivot],rows[j]
        scale=rows[j][j];rows[j]=[v/scale for v in rows[j]]
        for i in range(3):
            if i!=j:
                scale=rows[i][j];rows[i]=[v-scale*w for v,w in zip(rows[i],rows[j])]
    return [r[3] for r in rows]


def fit_ring(points):
    """Least-squares XY rear-ring and Z plane; no clearance/socket inference."""
    if not 6<=len(points)<=4096:raise ValueError('insufficient/excessive ring witnesses')
    pts=[vector(v) for v in points]
    rows=[[x,y,1] for x,y,z in pts]
    gram=[[sum(row[i]*row[j] for row in rows) for j in range(3)] for i in range(3)]
    d,e,f=solve3(gram,[-sum(row[i]*(p[0]**2+p[1]**2) for row,p in zip(rows,pts)) for i in range(3)])
    x,y=-d/2,-e/2;r2=x*x+y*y-f
    if r2<=0:raise ValueError('degenerate ring radius')
    radius=math.sqrt(r2)
    # Z is slightly tilted in Frontier; fit z = ax + by + c rather than flatten.
    za,zb,zc=solve3(gram,[sum(row[i]*p[2] for row,p in zip(rows,pts)) for i in range(3)])
    angles=sorted(math.atan2(p[1]-y,p[0]-x) for p in pts)
    gaps=[b-a for a,b in zip(angles,angles[1:])]+[angles[0]+2*math.pi-angles[-1]]
    if max(gaps)>math.pi/2+1e-5:raise ValueError('incomplete ring angular coverage')
    return {'centre_cm':[x,y,za*x+zb*y+zc],'radius_xy_cm':radius,'witness_count':len(pts),
            'max_radial_residual_cm':max(abs(math.hypot(p[0]-x,p[1]-y)-radius) for p in pts),
            'max_plane_residual_cm':max(abs(p[2]-(za*p[0]+zb*p[1]+zc)) for p in pts),
            'rear_plane_z_coefficients':[za,zb,zc],'socket_admitted':False}


def transform_point(point,frame):
    point=vector(point);validate_frame(frame)
    return [sum(point[k]*frame[k][j] for k in range(3))+frame[3][j] for j in range(3)]


def normalize_member(name):
    normalized=name.replace('\\','/')
    if not normalized or normalized.startswith('/') or ':' in normalized or any(p in ('..','.') for p in normalized.split('/')) or '\0' in normalized:
        raise ValueError('unsafe member name')
    return normalized.casefold()


def digest_handle(handle):
    handle.seek(0);digest=hashlib.file_digest(handle,'sha256').hexdigest();handle.seek(0)
    return digest


class PinnedArchive:
    def __init__(self,handle,label):
        if digest_handle(handle)!=PINNED_ARCHIVES[label]:raise ValueError('unrecognized original '+label)
        self.label=label;self.zip=zipfile.ZipFile(handle);self.members={};self.sources={}
        for info in self.zip.infolist():
            if info.is_dir():continue
            key=normalize_member(info.filename)
            if key in self.members:raise ValueError('duplicate normalized archive member')
            self.members[key]=info

    def read(self,name):
        info=self.members.get(normalize_member(name))
        if info is None or info.file_size>64*1024*1024:raise ValueError('missing/oversized member '+name)
        data=self.zip.read(info)
        if len(data)!=info.file_size:raise ValueError('member length mismatch')
        source={'archive':self.label,'member':info.filename,'sha256':hashlib.sha256(data).hexdigest(),'size':len(data)}
        self.sources[info.filename]=source
        return data,source


def geometry_parts(data,node):
    offset=node['file_offset'];_,_,size,header=struct.unpack_from('<4I',data,offset)
    result=[]
    for kind,p,size,header in chunks(data,offset+16+header,offset+size):
        if kind!=256 or header!=16:raise ValueError('unsupported geometry part')
        nv,ni,nb,morphs=struct.unpack_from('<4I',data,p+16)
        sub=list(chunks(data,p+16+header,p+size));arrays={k:(q+16,h) for k,q,s,h in sub}
        if len(sub)!=len(arrays) or nv<1 or ni%3 or morphs:raise ValueError('invalid geometry layout')
        result.append({'vertex_count':nv,'index_count':ni,'batch_count':nb,'arrays':arrays})
    return result


def packed_vertices(data,part):
    arrays=part['arrays'];count=part['vertex_count']
    if 0x160 not in arrays or 0x101 not in arrays:raise ValueError('missing position quantization')
    q,size=arrays[0x160]
    if size!=60:raise ValueError('unsupported position quantization header')
    kind,x,y,z,scale=struct.unpack_from('<I4f',data,q)
    p,size=arrays[0x101]
    if kind!=4 or size!=count*8 or not 0<scale<1 or any(not math.isfinite(v) for v in (x,y,z,scale)):
        raise ValueError('unsupported packed position format')
    points=[tuple(raw*scale+bias for raw,bias in zip(struct.unpack_from('<3h',data,p+i*8),(x,y,z))) for i in range(count)]
    return points,scale


def mesh_profile(data,source):
    nodes=mesh_frames(data)
    summaries=[]
    for node in nodes:
        parts=geometry_parts(data,node)
        summary=dict(node)
        summary['geometry_parts']=[{k:v for k,v in part.items() if k!='arrays'} for part in parts]
        # Only known packed rigid geometry is measured. No new skin/vertex
        # format is inferred from filenames; unsupported parts remain explicit.
        for summary_part,part in zip(summary['geometry_parts'],parts):
            try:
                points,scale=packed_vertices(data,part)
                summary_part['local_bounds_cm']=[[min(p[i] for p in points),max(p[i] for p in points)] for i in range(3)]
                summary_part['position_quantum_cm']=scale
            except ValueError as exc:summary_part['position_measurement_unavailable']=str(exc)
        summaries.append(summary)
    return {'source':source,'nodes':summaries}


class NativeInventory:
    def __init__(self,archive):self.archive=archive;self.classes={};self.parsed={}

    def read(self,name):
        if name not in self.parsed:
            data,source=self.archive.read(name+'.class');pool,methods=parse_class(data)
            reader=Reader(data);reader.read(8);parse_constant_pool(reader);reader.u2()
            this=pool.describe(reader.u2());parent=pool.describe(reader.u2())
            if this!=name:raise ValueError('unexpected class identity')
            self.parsed[name]=(pool,methods)
            self.classes[name]={'source':source,'parent':parent,'methods':{}}
        return self.parsed[name]

    def method(self,name,method,descriptor=None):
        pool,methods=self.read(name)
        found=[m for m in methods if m.name==method and (descriptor is None or m.descriptor==descriptor)]
        if len(found)!=1 or found[0].code is None:raise ValueError('missing/ambiguous method '+name+'.'+method)
        selected=found[0];lines=disassemble(selected.code,pool)
        self.classes[name]['methods'][selected.name+selected.descriptor]=lines
        return lines

    def literals(self,name):
        lines=self.method(name,'<init>','()V');result={}
        for i,line in enumerate(lines):
            if 'putfield' not in line or i<2 or 'aload_0' not in lines[i-2]:continue
            field=re.search(r'// [^.]+\.([\w$]+)(?:F|I|Z)$',line)
            if field is None:continue
            previous=lines[i-1];value=None
            if ' // ' in previous:
                try:value=ast.literal_eval(previous.split(' // ',1)[1])
                except (ValueError,SyntaxError):pass
            else:
                const=re.search(r'(?:f|i)const_(-?\d)',previous)
                if const:value=float(const[1])
            if value is not None:result[field[1]]=value
        return result

    def resolve_method(self,name,method,descriptor,seen=()):
        if name in seen or len(seen)>24:raise ValueError('cyclic/oversized method hierarchy')
        _,methods=self.read(name)
        if any(m.name==method and m.descriptor==descriptor for m in methods):
            return name,self.method(name,method,descriptor)
        return self.resolve_method(self.classes[name]['parent'],method,descriptor,seen+(name,))


def require_terms(lines,*terms):
    text='\n'.join(lines)
    if any(term not in text for term in terms):raise ValueError('original native contract mismatch: '+repr(terms))
    return text


def reload_buffers(lines):
    result={}
    for i,line in enumerate(lines):
        match=re.search(r'\.m_cSBReload(Begin|End)?LSoundBuffer;',line)
        if match is None or 'putfield' not in line:continue
        key=match[1] or 'Body'
        if i>0 and 'aconst_null' in lines[i-1]:result[key]=None;continue
        window=lines[max(0,i-15):i]
        strings=[]
        for candidate in window:
            if 'ldc' in candidate and ' // ' in candidate:
                try:value=ast.literal_eval(candidate.split(' // ',1)[1])
                except (SyntaxError,ValueError):continue
                if isinstance(value,str):strings.append(value)
        if len(strings)<2 or not any('SoundBuffer.<init>' in s for s in window):
            raise ValueError('unsupported reload buffer constructor')
        result[key]={'directory':strings[-2],'prefix':strings[-1]}
    return result


def build_inventory(code_path,data0_path,data2_path):
    with ExitStack() as stack:
        handles={label:stack.enter_context(Path(path).open('rb')) for label,path in
                 [('code',code_path),('data0',data0_path),('data2',data2_path)]}
        archives={label:PinnedArchive(h,label) for label,h in handles.items()}
        for archive in archives.values():stack.callback(archive.zip.close)
        code,data0,data2=(archives[k] for k in ('code','data0','data2'))
        native=NativeInventory(code)
        raw_meshes={};meshes={}
        wanted=dict(MESH_HASHES)
        for model,profile in MOUTH_PROFILES.items():wanted[profile['path']]=WEAPONS[model][1]
        for path,expected in wanted.items():
            data,source=data0.read(path)
            if source['sha256']!=expected:raise ValueError('unexpected exact mesh '+path)
            raw_meshes[path]=data;meshes[path]=mesh_profile(data,source)
        assets=[]
        for info in data0.members.values():
            if info.filename.casefold().startswith(('data/player/equipment/scope/','data/player/equipment/winchesterscope/')):
                data,source=data0.read(info.filename);item=dict(source)
                if info.filename.lower().endswith(('.mat','.skn','.scr')):
                    item['authored_text']=data.decode('cp1252').replace('\r\r\n','\n')
                if info.filename.lower().endswith('.dds'):
                    if len(data)<128 or data[:4]!=b'DDS ':raise ValueError('invalid DDS')
                    item['dds']={'height':struct.unpack_from('<I',data,12)[0],
                        'width':struct.unpack_from('<I',data,16)[0],'mip_count':struct.unpack_from('<I',data,28)[0],
                        'fourcc':data[84:88].decode('ascii',errors='backslashreplace')}
                assets.append(item)
        defaults=native.literals('FPPCamera');zoom=native.literals('WpnLogicRifleWinchesterScope').get('m_fScopeZoom')
        if zoom!=8:raise ValueError('unexpected scope parameter')
        scope_terms=require_terms(native.method('ArmedPlayerBeing','GetScopeZoom'),
            'GetActualWeaponNotEmpty','GetCurrentCamera','GetDefaultFOV','DegreesToPercents','fdiv')
        require_terms(native.method('Weapon','GetScopeZoom'),'GetLogicParams')
        require_terms(native.method('Weapon','GetLogicParams'),'GetParams')
        native.method('Weapon','GetParams')
        require_terms(native.method('LawmanModule','GetWeaponParam'),'CreateWeaponParam')
        native.method('LawmanModule','CreateWeaponParam')
        for method in ('IsScopeVisible','GetSquintZoom'):native.method('ArmedPlayerBeing',method)
        native.method('ArmedPlayerBeing','GetWeaponScopeZoom')
        native.method('BeingTools','DegreesToPercents')
        native.method('BeingCamera','UpdateZoom')
        require_terms(native.method('FPPCamera','OnPostBeingsUpdate'),'FromForwardUpPos','UpdateScopeObject')
        require_terms(native.method('FPPCamera','CreateScopeObject'),'Scope/Scope.msh','m_ScopeObject')
        require_terms(native.method('FPPCamera','UpdateScopeObject'),'IsScopeVisible','FromUpForwardPos','SetScale')
        for method in ('Init','Destroy','DestroyScopeObject','ShowScopeObject','HideScopeObject'):native.method('FPPCamera',method)
        native.method('WeaponRifleWinchesterScope','GetLogicParamsClass')
        native.method('WeaponRifleWinchesterScope','GetAmmoParamsClass')
        native.method('WpnAmmoRifleWinchesterScope','<init>','()V')
        weapon_scope=next(n for n in meshes[RIFLE_MESH]['nodes'] if n['name']=='Scope')
        optics={'authored_scope_zoom':zoom,'camera_defaults':defaults,
                'weapon_mesh':meshes[RIFLE_MESH],'overlay_mesh':meshes[SCOPE_MESH],
                'weapon_scope_element':weapon_scope,'authored_assets':assets,
                'native_formula':'scopeParameter * tan(defaultCameraFov/2) / tan(40 degrees)',
                'offline_examples':{str(f):scope_zoom(zoom,f) for f in (60,80,90)},
                'runtime_admitted':False}

        audio_data,audio_source=data2.read(AUDIO_ASSET)
        if audio_source['sha256']!=AUDIO_HASH:raise ValueError('unexpected shared reload WAV')
        wave_profile=wav_profile(audio_data)
        audio_profiles=[]
        for name in ('WpnAmmoPistolPeacemaker','WpnAmmoPistolFrontier','WpnAmmoPistolWalker',
            'WpnAmmoRifleCogswell','WpnAmmoShotgunRemington','WpnAmmoPistolDerringer',
            'WpnAmmoRifleLemant','WpnAmmoPistolLemant','WpnAmmoRifleWinchester'):
            lines=native.method(name,'<init>','()V');buffers=reload_buffers(lines)
            if set(buffers)!={'Begin','Body','End'}:raise ValueError('incomplete explicit reload audio mapping')
            matches={}
            for phase,buffer in buffers.items():
                matches[phase]=[]
                if buffer is None:continue
                prefix=normalize_member(buffer['directory']+'/'+buffer['prefix'])
                for key,info in data2.members.items():
                    if key.startswith(prefix):
                        _,source=data2.read(info.filename)
                        matches[phase].append(dict(source,accept_after_lowercase_normalization=
                            soundbuffer_accept(key,buffer['prefix'].lower())))
            audio_profiles.append({'class':name,'source':native.classes[name]['source'],
                'buffers':buffers,'prefix_matches_in_explicit_data2':matches})
        sound_assets={}
        for profile in audio_profiles:
            for matches in profile['prefix_matches_in_explicit_data2'].values():
                for source in matches:
                    if source['member'] in sound_assets:continue
                    data,_=data2.read(source['member']);item={'source':source}
                    try:item['wave']=wav_profile(data)
                    except ValueError as exc:item['wave_unavailable']=str(exc)
                    sound_assets[source['member']]=item
        # These are constructor/method inventory profiles, not runtime admission.
        weapon_profiles=[]
        for weapon,parameters in [
            ('WeaponPistolPeacemaker',['WpnAmmoPistolPeacemaker']),
            ('WeaponPistolFrontier1878_Regular',['WpnAmmoPistolFrontier']),
            ('WeaponPistolFrontier',['WpnAmmoPistolFrontier']),
            ('WeaponPistolWalker',['WpnAmmoPistolWalker']),
            ('WeaponPistolCogswell',['WpnAmmoRifleCogswell']),
            ('WeaponRifleRemington',['WpnAmmoShotgunRemington']*2),
            ('WeaponRifleRemingtonSawOff',['WpnAmmoShotgunRemington']*2),
            ('WeaponPistolDerringer',['WpnAmmoPistolDerringer']*2),
            ('WeaponPistolLemant',['WpnAmmoRifleLemant','WpnAmmoPistolLemant']),
            ('WeaponRifleWinchester',['WpnAmmoRifleWinchester'])]:
            declaring,lines=native.resolve_method(weapon,'GetAmmoParamsClass','(I)Ljava/lang/String;')
            for parameter in set(parameters):require_terms(lines,"'"+parameter+"'")
            weapon_profiles.append({'weapon_class':weapon,'source':native.classes[weapon]['source'],
                'declaring_ammo_method':declaring,'authored_parameters_by_index':parameters,
                'index_mapping_basis':'pinned original methods/research; not a bytecode interpreter',
                'runtime_admitted':False})
        body=require_terms(native.method('ArmedPlayerBeing','OnHandStateStarted_Reload'),'.Reload(II)I')
        require_terms(native.method('Weapon','ReloadInside'),'ExecSound','m_cSBReload','SetAmmoCount')
        transfer=native.method('PawnArmed','WeaponReload','(LWeapon;)Z')
        direct_sound=any('invoke' in line and 'Sound' in line for line in transfer)
        if direct_sound:raise ValueError('unexpected direct transfer audio call')
        for name,method,desc in [('Weapon','ExecSound','(LSoundBuffer;)I'),
            ('Weapon','ExecSound','(LSoundBuffer;Z)I'),('Weapon','PlayWeaponSound',None),
            ('PawnArmed','PlayWeaponSound',None),('Weapon','UpdateAmmoSkin',None),
            ('Weapon','SetAmmoCount',None),('WeaponFireAmmo','SetAmmoCount',None),
            ('ArmedPlayerBeing','OnHandStateStarted_ReloadBegin',None),
            ('ArmedPlayerBeing','OnHandStateFinished_Reload',None),
            ('SoundBuffer','SetSoundBuffer','(Ljava/lang/String;Ljava/lang/String;)V')]:
            native.method(name,method,desc)
        accept=native.method('SoundBuffer','Accept','(Ljava/lang/String;Ljava/lang/String;)Z')
        require_terms(accept,'lastIndexOf','bipush             7',"'.wav'","'.ogg'")
        # Cylinder phase belongs to reload-end/native post-update, not the
        # geometry fitter or the successful ammunition transaction.
        require_terms(native.method('ArmedPlayerBeing','OnHandStateFinished_ReloadEnd'),'ReloadRotate')
        require_terms(native.method('Weapon','ReloadRotate'),'m_fRotate')
        require_terms(native.method('Weapon','OnPostUpdate'),'RotateElementWithAnim')
        native.method('WpnVisualPistolPeacemaker','<init>','()V')
        native.method('WpnVisualPistolFrontier','<init>','()V')
        audio={'asset':audio_source,'wave':wave_profile,'profiles':audio_profiles,
            'weapon_profiles':weapon_profiles,'sound_assets':list(sound_assets.values()),
            'body_entry_plays_shared_clip':bool(body),'unit_transfer_has_direct_sound_call':direct_sound,
            'unqualified_consumers':['SetSkin','native animation/events','live VFS overrides','audible body/insertion overlap'],
            'asset_search_boundary':'only explicit pinned Data2 archive; loose files/other VFS mounts not inspected',
            'prefix_acceptance_basis':'exact SoundBuffer.Accept on lowercased archive candidates; native VFS casing/order/cache remains unobserved',
            'additional_sound_admitted':False,'duplication_proven':False,
            'predicted_early_insertion':predicted_overlap(0,wave_profile['seconds'],.2)}

        mouths=[]
        for model,profile in MOUTH_PROFILES.items():
            data=raw_meshes[profile['path']];mesh=meshes[profile['path']]
            drum=next(n for n in mesh['nodes'] if n['name']==profile['node'])
            gate=next(n for n in mesh['nodes'] if n['name']==profile['gate'])
            parts=geometry_parts(data,drum)
            # The largest authored cylinder submesh contains the measured rings.
            points,quantum=packed_vertices(data,max(parts,key=lambda part:part['vertex_count']))
            unique=sorted(set(points));rings=[]
            for centre in profile['centres']:
                # Seed bands are measurement search windows from prior geometry,
                # never insertion tolerances/contact volumes or discovery proof.
                witnesses=[point for point in unique if abs(point[2]-centre[2])<.01 and
                    abs(math.hypot(point[0]-centre[0],point[1]-centre[1])-profile['radius'])<.005]
                fitted=fit_ring(witnesses)
                error=math.dist(centre,fitted['centre_cm'])
                if error>.002 or fitted['max_radial_residual_cm']>.001:raise ValueError('ring witness/profile mismatch')
                fitted.update(seed_centre_cm=centre,centre_seed_error_cm=error,
                    bind_world_centre_cm=transform_point(fitted['centre_cm'],drum['world']))
                rings.append(fitted)
            # Relevant script/ANM sources are inventoried, never executed/sampled.
            animation_sources=[];player_animation_sources=[]
            family=profile['path'].split('/')[3].lower()
            player_stem={'peacemaker':'peacemaker','frontier':'frontier'}[model]
            for info in data0.members.values():
                lower=info.filename.lower()
                equipment='/equipment/'+family+'/animations/' in lower and lower.endswith(('.anm','.scr'))
                player=lower.startswith('data/player/animations/ray_'+player_stem) and lower.endswith(('.anm','.scr'))
                if equipment or player:
                    data,source=data0.read(info.filename);entry=dict(source)
                    if lower.endswith('.scr'):
                        text=data.decode('cp1252').replace('\r\r\n','\n')
                        entry['reload_lines']=[{'line':i+1,'text':line} for i,line in enumerate(text.splitlines()) if 'reload' in line.lower()]
                    else:entry['event_decode']='not performed; no negative sound proof'
                    (animation_sources if equipment else player_animation_sources).append(entry)
            mouths.append({'model':model,'source':mesh['source'],'drum':drum,'gate':gate,
                'position_quantum_cm':quantum,'rings':rings,'animation_sources':animation_sources,
                'player_animation_sources':player_animation_sources,
                'measurement_mode':'seeded original-vertex ring verification, not independent socket discovery',
                'seed_search_axial_cm':.01,'seed_search_radial_cm':.005,
                'native_phase_qualified':False,'gate_clearance_qualified':False,'socket_admitted':False,
                'approach':'cylinder-local +Z from rear; live phase/tip/clearance still required'})
        for label,handle in handles.items():
            if digest_handle(handle)!=PINNED_ARCHIVES[label]:raise ValueError('archive changed during read')
        sources=sorted([s for archive in archives.values() for s in archive.sources.values()],key=lambda s:(s['archive'],s['member']))
        return {'schema_version':1,'mode':'offline_original_inventory_and_math','archives':PINNED_ARCHIVES,
            'optics':optics,'audio':audio,'loading_mouths':mouths,'classes':native.classes,'sources':sources,
            'native_boundaries':{'scope_parameter_factory_on_cache_miss':True,'scope_overlay_independent_update':True,
                'native_skin_event_audio_silence_proven':False,'mouth_phase_and_clearance_proven':False},
            'runtime_admitted':False}


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--code-pak',type=Path,required=True)
    parser.add_argument('--data0-pak',type=Path,required=True)
    parser.add_argument('--data2-pak',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args(argv)
    work=Path(__file__).resolve().parents[1]/'work'
    if not args.output.resolve().is_relative_to(work.resolve()):raise ValueError('output must stay in ignored work/')
    if args.output.exists():raise ValueError('output already exists; preserve evidence')
    report=build_inventory(args.code_pak,args.data0_pak,args.data2_pak)
    with args.output.open('x',encoding='utf-8') as stream:json.dump(report,stream,indent=2,allow_nan=False)
    print(f"Verified scope owners, {len(report['audio']['profiles'])} audio profiles and 12 measured rings; no runtime admission")


if __name__=='__main__':main()
