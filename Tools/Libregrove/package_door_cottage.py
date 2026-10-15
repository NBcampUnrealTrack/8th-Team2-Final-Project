"""Read-only export audit and immutable V5 delivery packaging (no Blender edits)."""
import hashlib,json,math,shutil,zipfile
from pathlib import Path
import numpy as np
from inspect_glb import inspect,read_glb,accessor
from precision_uv import audit_triangles

ROOT=Path(__file__).resolve().parents[2]
DEST=ROOT/'Docs/Libregrove/DirectModel_Cottage_V5_20260908'
EXCHANGE=ROOT/'Saved/Revvy/BlenderExchange/LibregroveDoorV5/revision02'
STATE=ROOT/'Saved/LibregroveDoorJobs'

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def data(p):return json.loads(Path(p).read_text(encoding='utf-8'))
def newjson(p,v):
    with Path(p).open('x',encoding='utf-8') as f:json.dump(v,f,ensure_ascii=False,indent=2)
def copy(source,target):
    target.parent.mkdir(parents=True,exist_ok=True);assert not target.exists();shutil.copy2(source,target);assert sha(source)==sha(target)

def audit_exports():
    verification=data(DEST/'verification_r2.json');checks=[];glbs=[]
    for index,stem in enumerate(['LG_Cottage_OpenClose','SM_LG_Cottage_Body','SM_LG_Cottage_Door'],1):
        p=EXCHANGE/f'exports/{stem}.glb';info=inspect(p);g,b=read_glb(p)
        expected=[332,296,36][index-1];assert info['total_triangles']==expected
        assert not info['images'] and not info['materials']
        # Binary UV audit after float32 GLB V-flip, independently per texture set.
        uv_reports=[]
        for mesh in g['meshes']:
            for prim in mesh['primitives']:
                uv=accessor(g,b,prim['attributes']['TEXCOORD_0']);indices=accessor(g,b,prim['indices']).reshape(-1,3)
                qa=audit_triangles(uv[indices],np.arange(len(indices)));assert qa['passed'];uv_reports.append(qa)
        info['uv_byte_audits']=uv_reports
        if index==1:
            assert len(g.get('animations',[]))==1
            anim=g['animations'][0];assert any(c['target']['path']=='rotation' for c in anim['channels'])
            times=[accessor(g,b,s['input']) for s in anim['samplers']]
            assert all(abs(float(t.max()-t.min())-3)<1e-5 for t in times)
        else:assert not g.get('animations')
        if index<3:assert np.allclose(info['dimensions_m_xyz_y_up'],[2.25,2.43,2.7],atol=1e-5)
        glbs.append(info)
        for ext in ['glb','fbx']:
            job=data(STATE/f'DoorV5_Reimport{index:03d}_{ext}_r2/result.json');assert job['success']
            r=job['result'];assert r['input_preserved'] and all(x['uv']['passed'] for x in r['meshes'])
            assert sum(x['mesh']['geometry']['triangles'] for x in r['meshes'])==expected
            max_error=0.
            for sample in r['motion_samples']:
                frame=sample['frame'];angle=math.radians(verification['motion']['samples'][frame-1]['z_degrees'])
                for obj in sample['objects']:
                    actual=np.asarray(obj['matrix_world']);expected_matrix=np.eye(4)
                    if index==1 and 'Door' in obj['name']:
                        expected_matrix[:3,:3]=[[math.cos(angle),-math.sin(angle),0],[math.sin(angle),math.cos(angle),0],[0,0,1]]
                        expected_matrix[:3,3]=[1.04,-.285,.1]
                    error=float(np.abs(actual-expected_matrix).max());max_error=max(max_error,error)
            assert max_error<1e-5,(stem,ext,max_error)
            if index==1:
                door_actions=[o for o in r['animated_objects'] if 'Door' in o['name']];assert len(door_actions)==1
                assert np.allclose(door_actions[0]['range'],[1,73],atol=1e-5)
            else:assert not r['animated_objects']
            checks.append({'file':stem+'.'+ext,'reimport_passed':True,'pose_matrix_max_error':max_error,'sha256':sha(EXCHANGE/f'exports/{stem}.{ext}')})
    return {'blender':verification,'exports':checks,'glbs':glbs,'passed':True}

def uv_layout(glb,out):
    g,b=read_glb(glb);lines=['<svg xmlns="http://www.w3.org/2000/svg" width="1024" height="1024" viewBox="0 0 1024 1024">','<rect x="0" y="0" width="1024" height="1024" fill="white"/>','<g fill="none" stroke="#1a292a" stroke-width="0.65">']
    for mesh in g['meshes']:
        for p in mesh['primitives']:
            uv=accessor(g,b,p['attributes']['TEXCOORD_0']);indices=accessor(g,b,p['indices']).reshape(-1,3)
            for tri in indices:lines.append('<polygon points="'+' '.join(f'{uv[i,0]*1024:.4f},{uv[i,1]*1024:.4f}' for i in tri)+'"/>')
    lines+=['</g></svg>'];out.parent.mkdir(parents=True,exist_ok=True)
    with out.open('x',encoding='utf-8') as f:f.write('\n'.join(lines))

def main():
    audit=audit_exports()
    assert sha('D:/Users/Ndus_072/Desktop/TextureCover.blend')=='e4f4a1c80027142c34c9a6c870fb7520c96d87f2109eb5d052cbf053a0f9dac9'
    assert sha(ROOT/'Docs/Libregrove_DirectCottage_V4_20260908.zip')=='39c3eb87289dd706ee8aaffc27678660e4578cb7acb7b8cb0348a622e5ade1fd'
    copy(EXCHANGE/'04_Cottage_OpenClose.blend',DEST/'LG_Cottage_V5_FINAL.blend')
    for p in (EXCHANGE/'exports').iterdir():copy(p,DEST/'Export'/p.name)
    for p in (EXCHANGE/'previews').iterdir():copy(p,DEST/'Preview'/p.name)
    copy(Path('C:/Users/Ndus_072/AppData/Local/Revvy/BlenderExchange/Libregrove_DirectCottage_V5_20260908/Backup/00_V4_before_door.blend'),DEST/'Backup/00_V4_before_door.blend')
    for stem in ['SM_LG_Cottage_Body','SM_LG_Cottage_Door']:uv_layout(DEST/f'Export/{stem}.glb',DEST/f'UV/{stem}.svg')
    placement={'units':'Blender meters','axis':{'front':'+X','width':'Y','up':'+Z'},'body_origin_m':[0,0,0],'door_closed_hinge_m':[1.04,-.285,.1],'door_closed_rotation_degrees':[0,0,0],'door_open_rotation_degrees':[0,0,-100],'open_duration_seconds':1,'hold_seconds':1,'close_duration_seconds':1,'fps':24,'frame_range':[1,73],'doorway_width_cm':62,'doorway_height_cm':104,'door_leaf_width_cm':57,'door_leaf_height_cm':99,'door_leaf_thickness_cm':4,'body_dimensions_width_depth_height_cm':[270,225,243],'scale_basis':'Project 88cm player, not real adult architecture','texture_sets':[{'mesh':'body','recommended_resolution':2048},{'mesh':'door','recommended_resolution':512}],'game_engine_interaction_implemented':False}
    newjson(DEST/'door_placement.json',placement)
    newjson(DEST/'final_signoff.json',audit)
    stats={'mesh_vertex_budget':300,'triangle_budget':500,'measured_mesh_vertices':222,'measured_triangles':332,'measured_glb_render_vertices':audit['glbs'][0]['total_render_vertices'],'source_TextureCover_preserved':True,'V4_zip_preserved':True,'final_blend_sha256':sha(DEST/'LG_Cottage_V5_FINAL.blend')}
    newjson(DEST/'asset_stats.json',stats)
    print(json.dumps(stats,indent=2));print('PACKAGING_AUDIT_PASS')

if __name__=='__main__':main()
