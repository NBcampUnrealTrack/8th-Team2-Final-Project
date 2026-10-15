"""Independent numeric export audit and immutable six-asset handoff."""
import csv,hashlib,json,math,shutil,zipfile
from pathlib import Path
import numpy as np
from buildings_catalog import CATALOG
from inspect_glb import inspect,read_glb,accessor
from precision_uv import audit_triangles
from package_door_cottage import uv_layout,audit_exports as audit_cottage
ROOT=Path(__file__).resolve().parents[2]
DEST=ROOT/'Docs/Libregrove/DirectBuildings_V6_20260908'
EXCHANGE=ROOT/'Saved/Revvy/BlenderExchange/LibregroveBuildingsV6'
STATE=ROOT/'Saved/LibregroveBuildingJobs'

def data(p):return json.loads(p.read_text(encoding='utf-8'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def newjson(p,v):
    p.parent.mkdir(parents=True,exist_ok=True)
    with p.open('x',encoding='utf-8') as f:json.dump(v,f,ensure_ascii=False,indent=2)
def copy(source,target):
    target.parent.mkdir(parents=True,exist_ok=True);assert not target.exists();shutil.copy2(source,target);assert sha(source)==sha(target)

def export_override(ident):
    path=DEST/f'verification/{ident}_export_override.json'
    return data(path) if path.is_file() else None

def export_path(ident,artifact_root,selection,ext):
    override=export_override(ident)
    if override and selection==override['selection'] and ext==override['format']:
        p=Path(override['path']);assert sha(p)==override['sha256'];return p
    return artifact_root/f'exports/{ident}_{selection}.{ext}'

def audit(spec):
    ident=spec['id'];v=data(DEST/f'verification/{ident}.json');r=v['report'];records=[]
    artifact_root=Path(v.get('artifact_root',EXCHANGE/ident));suffix='' if v.get('revision',1)==1 else '_r2'
    meshes={o['mesh']['object']['name']:o['mesh'] for o in r['objects']}
    triangles={n:m['geometry']['triangles'] for n,m in meshes.items()}
    selections=['assembly']+(['body','door_left']+(['door_right'] if spec['doors']==2 else []) if spec['doors'] else [])
    render_vertices=0;symmetry=None
    for selection in selections:
        model_path=export_path(ident,artifact_root,selection,'glb');g,b=read_glb(model_path);report=inspect(model_path)
        qa=[]
        for mesh in g['meshes']:
            for p in mesh['primitives']:
                uv=accessor(g,b,p['attributes']['TEXCOORD_0']);idx=accessor(g,b,p['indices']).reshape(-1,3)
                q=audit_triangles(uv[idx],np.arange(len(idx)));assert q['passed'];qa.append(q)
        assert not report['images'] and not report['materials']
        if selection=='assembly':
            render_vertices=report['total_render_vertices'];assert report['total_triangles']==r['total_triangles']
            assert np.allclose(report['dimensions_m_xyz_y_up'],[spec['depth'],spec['height'],spec['width']],atol=1e-5)
            if spec['doors']:
                assert len(g.get('animations',[]))==1
                targets=[]
                for animation in g['animations']:
                    assert len(animation['channels'])==spec['doors']
                    for channel in animation['channels']:
                        target=channel['target'];assert target['path']=='rotation'
                        targets.append(g['nodes'][target['node']]['name'])
                assert sorted(targets)==sorted(r['door_names'])
            else:assert not g.get('animations')
            # Body vertex mirror check, width is glTF Z (Blender Y).
            body_node=next(n for n in g['nodes'] if n.get('name')==r['body_name'])
            p=g['meshes'][body_node['mesh']]['primitives'][0];points=accessor(g,b,p['attributes']['POSITION']).astype(float)
            reflected=points.copy();reflected[:,2]*=-1
            errors=[float(np.linalg.norm(points-p,axis=1).min()) for p in reflected];symmetry=max(errors)
            assert symmetry<1e-5,(ident,symmetry)
        else:assert not g.get('animations')
        for ext in ['glb','fbx']:
            override=export_override(ident)
            job_id=override['job_id'] if override and selection==override['selection'] and ext==override['format'] else f'B6_{ident}_Reimport_{selection}_{ext}{suffix}'
            job=data(STATE/job_id/'result.json');assert job['success'];out=job['result']
            assert out['input_preserved'] and all(x['uv']['passed'] for x in out['meshes'])
            # Five sampled matrices compare against reopened authoritative .blend.
            max_error=0.
            for s in out['motion_samples']:
                reference={} if not spec['doors'] else {d['name']:np.asarray(d['matrix_world']) for d in r['motion']['samples'][s['frame']-1]['doors']}
                for ob in s['objects']:
                    expected=reference.get(ob['name'],np.eye(4)) if selection=='assembly' else np.eye(4)
                    error=float(np.abs(np.asarray(ob['matrix_world'])-expected).max());max_error=max(max_error,error)
                    assert error<1e-5,(ident,selection,ext,s['frame'],error)
            expected_n=len(r['objects']) if selection=='assembly' else 1;assert len(out['meshes'])==expected_n
            actual_tris=sum(x['mesh']['geometry']['triangles'] for x in out['meshes'])
            if selection=='assembly':assert actual_tris==r['total_triangles']
            elif selection=='body':assert actual_tris==triangles[r['body_name']]
            else:assert actual_tris==36
            if selection=='assembly' and spec['doors']:
                for name in r['door_names']:
                    action=next(x for x in out['animated_objects'] if x['name']==name);assert np.allclose(action['range'],[1,73],atol=1e-5)
            elif selection!='assembly' or not spec['doors']:assert not out['animated_objects']
            records.append({'selection':selection,'format':ext,'sha256':sha(export_path(ident,artifact_root,selection,ext)),'pose_max_error':max_error,'triangles':actual_tris,'passed':True})
    return {'id':ident,'name':spec['name'],'mesh_vertices':r['total_mesh_vertices'],'triangles':r['total_triangles'],'render_vertices':render_vertices,'budget_vertices':spec['budget_v'],'budget_triangles':spec['budget_t'],'width_cm':spec['width']*100,'depth_cm':spec['depth']*100,'height_cm':spec['height']*100,'doors':spec['doors'],'body_symmetry_max_error_m':symmetry,'uv_sets':len(r['objects']),'exports':records,'final_blend_sha256':v['final_sha256'],'passed':True}

def package():
    audited=[audit(spec) for spec in CATALOG]
    cottage_audit=audit_cottage();assert cottage_audit['passed']
    cottage=ROOT/'Docs/Libregrove/DirectModel_Cottage_V5_20260908'
    assert sha(cottage/'LG_Cottage_V5_FINAL.blend')=='c8e68c6c81588548ed145e540fa31fcbc9164e0d104fce4b41a84f4215da78db'
    for item in cottage_audit['exports']:assert sha(cottage/'Export'/item['file'])==item['sha256']
    source=data(DEST/'source_baseline.json');assert sha(Path(source['file']))==source['file_sha256']
    assert data(DEST/'verification/source_preservation_final.json')['passed']
    assert sha(ROOT/'Docs/Libregrove_Cottage_V5_OpenClose_20260908.zip')=='1d5b1a3be48c18b78b854e71500ef48a4ca3c477633c21b3c4acbc63be3411b2'
    assert sha(ROOT/'Docs/Libregrove_Texturing_V3_20260908.zip')=='11fc6b503096bcb3bdb3d8361456c88d5291ac03a6d81398025b58a9b6eb4530'
    rows=[]
    for spec,item in zip(CATALOG,audited):
        ident=spec['id'];folder=DEST/'Models'/ident;v=data(DEST/f'verification/{ident}.json')
        artifact_root=Path(v.get('artifact_root',EXCHANGE/ident))
        copy(Path(v['final_blend']),folder/(ident+'_FINAL.blend'))
        for exported in item['exports']:
            p=export_path(ident,artifact_root,exported['selection'],exported['format']);copy(p,folder/'Export'/p.name)
        for p in (artifact_root/'previews').iterdir():copy(p,folder/'Preview'/p.name)
        for p in (folder/'Export').glob('*.glb'):
            if 'assembly' not in p.stem or not spec['doors']:uv_layout(p,folder/'UV'/(p.stem+'.svg'))
        plan_suffix='' if v.get('revision',1)==1 else '_r2'
        newjson(folder/'asset_info.json',{'audit':item,'plan':{k:v for k,v in data(DEST/f'plans/{ident}{plan_suffix}.json').items() if k in ['spec','doors']},'texture_resolution_recommendation':{'body':2048 if spec['doors'] else 1024,'each_door':512},'texture_sets_separate':True,'glb_animation_clips':'one combined scene clip for assembly','scale_basis':'88cm player game scale; not adult architecture','game_interaction_implemented':False})
        rows.append(item);print(ident+' PACKAGED',flush=True)
    cottage=ROOT/'Docs/Libregrove/DirectModel_Cottage_V5_20260908';folder=DEST/'Models/01_Cottage_A'
    copy(cottage/'LG_Cottage_V5_FINAL.blend',folder/'01_Cottage_A_FINAL.blend')
    for directory in ['Export','Preview','UV']:
        for p in (cottage/directory).iterdir():copy(p,folder/directory/p.name)
    for file in ['door_placement.json','asset_stats.json','final_signoff.json','verification_r2.json']:copy(cottage/file,folder/file)
    rows.insert(0,{'id':'01_Cottage_A','name':'기본 주택 (검증된 V5 재사용)','mesh_vertices':222,'triangles':332,'render_vertices':666,'budget_vertices':300,'budget_triangles':500,'width_cm':270,'depth_cm':225,'height_cm':243,'doors':1,'uv_sets':2,'passed':True,'final_blend_sha256':sha(cottage/'LG_Cottage_V5_FINAL.blend'),'reused_verified_V5':True})
    copy(Path('C:/Users/Ndus_072/AppData/Local/Revvy/BlenderExchange/Libregrove_Buildings_V6_20260908/Backup/00_Before_V6.blend'),DEST/'Backup/00_Before_V6.blend')
    manifest={'status':'MODEL_UV_RIGID_DOOR_EXPORT_VERIFIED','assets':rows,'newly_modeled':5,'reused_direct_models':1,'total_assets':6,'total_mesh_vertices':sum(x['mesh_vertices'] for x in rows),'total_triangles':sum(x['triangles'] for x in rows),'total_render_vertices':sum(x['render_vertices'] for x in rows),'new_export_files':sum(len(x.get('exports',[])) for x in rows),'total_export_files':sum(len(x.get('exports',[])) for x in rows)+6,'textures_authored':False,'game_engine_modified':False,'original_source_preserved':True,'excluded_preserved_assets':['LG_06_NaiaShelter','LG_07_Sanctuary','LG_08_BroadleafTree']}
    newjson(DEST/'manifest.json',manifest)
    cols=['id','name','width_cm','depth_cm','height_cm','mesh_vertices','triangles','render_vertices','budget_vertices','budget_triangles','doors','uv_sets']
    with (DEST/'Asset_Specifications.csv').open('x',encoding='utf-8-sig',newline='') as f:
        writer=csv.DictWriter(f,fieldnames=cols,extrasaction='ignore');writer.writeheader();writer.writerows(rows)
    print(json.dumps({k:v for k,v in manifest.items() if k!='assets'},indent=2),flush=True)

def archive():
    dest=ROOT/'Docs/Libregrove_DirectBuildings_V6_20260908.zip';assert not dest.exists()
    files=sorted(p for p in DEST.rglob('*') if p.is_file())
    with zipfile.ZipFile(dest,'x',compression=zipfile.ZIP_DEFLATED,compresslevel=7) as z:
        for p in files:z.write(p,(Path(DEST.name)/p.relative_to(DEST)).as_posix())
    with zipfile.ZipFile(dest) as z:
        assert z.testzip() is None
        for p in files:assert hashlib.sha256(z.read((Path(DEST.name)/p.relative_to(DEST)).as_posix())).hexdigest()==sha(p)
    report={'file':str(dest),'sha256':sha(dest),'bytes':dest.stat().st_size,'entries':len(files),'all_entry_hashes_passed':True}
    newjson(dest.with_suffix('.manifest.json'),report);print(json.dumps(report,indent=2))

if __name__=='__main__':
    import argparse
    parser=argparse.ArgumentParser();parser.add_argument('stage',choices=['package','archive']);args=parser.parse_args();package() if args.stage=='package' else archive()
