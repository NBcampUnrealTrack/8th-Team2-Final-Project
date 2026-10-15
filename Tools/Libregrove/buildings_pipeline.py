"""Granular typed MCP controller for V6. Each mutation inspected and recorded."""
import argparse,hashlib,json,shutil
from pathlib import Path
import blender_mcp_client as transport
from buildings_catalog import CATALOG,parts_for
ROOT=Path(__file__).resolve().parents[2]
DEST=ROOT/'Docs/Libregrove/DirectBuildings_V6_20260908'
EXCHANGE=ROOT/'Saved/Revvy/BlenderExchange/LibregroveBuildingsV6'
LIVE_EXCHANGE=Path('C:/Users/Ndus_072/AppData/Local/Revvy/BlenderExchange/Libregrove_Buildings_V6_20260908')
transport.EVIDENCE=DEST/'evidence'
def newjson(p,v):
    p.parent.mkdir(parents=True,exist_ok=True)
    with p.open('x',encoding='utf-8') as f:json.dump(v,f,ensure_ascii=False,indent=2)
def unwrap(v):
    v=v.get('result',v)
    return v.get('result',v) if v.get('state')=='finished' else v
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def common(p):return {'blend_path':str(p),'expected_sha256':sha(p)}

def preflight():
    clients=[]
    try:
        for kind in ['main','buildings']:
            client=transport.Client(kind,quiet=True);clients.append(client);schema=client.rpc('tools/list',{});newjson(DEST/f'{kind}_schema.json',schema)
            if kind=='main':
                status=unwrap(client.call('blender_bridge_status',{},'preflight_status'))
                assert status['blender']['runtime_supported'] and status['mcp']['bridge_compatibility']['compatible']
                scene=unwrap(client.call('blender_scene_inspect',{'include_hidden':True,'max_objects':2000},'preflight_scene'))
                assert not scene['truncated']
                newjson(DEST/'source_baseline.json',{'scene':scene,'file':status['blender']['file'],'file_sha256':sha(Path(status['blender']['file']))})
        result=unwrap(clients[1].call('blender_building_self_test',{'job_id':'B6_SelfTest001','output_blend':str(EXCHANGE/'tests/fixture001.blend')},'selftest001'))
        assert result['passed'];print('PREFLIGHT_AND_DOUBLE_DOOR_TEST_PASS',flush=True)
    finally:
        for client in clients:client.close()

def build():
    selftest=json.loads((ROOT/'Saved/LibregroveBuildingJobs/B6_SelfTest001/result.json').read_text());assert selftest['success']
    baseline=json.loads((DEST/'source_baseline.json').read_text(encoding='utf-8'))
    client=transport.Client('main',poll_interval=.25,quiet=True)
    try:
        client.call('blender_scene_inspect',{'include_hidden':True,'max_objects':2000},'build_before_backup')
        client.call('blender_save_copy',{'path':str(LIVE_EXCHANGE/'Backup/00_Before_V6.blend'),'overwrite':False},'build_backup')
        for spec in CATALOG:
            ident=spec['id'];parts,doors=parts_for(spec);collection='LGV6_'+ident
            scene=unwrap(client.call('blender_scene_inspect',{},f'{ident}_before_collection'));assert collection not in scene['collections']
            client.call('blender_collection_create',{'collection_name':collection},f'{ident}_collection')
            client.call('blender_scene_inspect',{},f'{ident}_after_collection')
            reports=[]
            for i,part in enumerate(parts,1):
                scene=unwrap(client.call('blender_scene_inspect',{'include_hidden':True,'max_objects':2000},f'{ident}_{i:02d}_before'))
                assert not any(o['name']==part['object_name'] for o in scene['objects'])
                request={k:v for k,v in part.items() if k!='role'};request['collection_name']=collection
                client.call('blender_primitive_create',request,f'{ident}_{i:02d}_create')
                report=unwrap(client.call('blender_mesh_inspect',{'object_name':part['object_name'],'requirements':{'require_manifold':True,'require_uv':True,'max_triangles':16}},f'{ident}_{i:02d}_after'))
                for key in ['boundary_edges','non_manifold_edges','degenerate_faces','non_finite_vertices']:assert report['geometry'][key]==0
                reports.append(report)
                if i%8==0 or i==len(parts):print(f'BUILT {ident} {i}/{len(parts)}',flush=True)
            scene=unwrap(client.call('blender_scene_inspect',{'include_hidden':True,'max_objects':2000},f'{ident}_before_save'))
            path=LIVE_EXCHANGE/f'{ident}_Parts.blend'
            client.call('blender_save_copy',{'path':str(path),'overwrite':False},f'{ident}_save_parts')
            copied=EXCHANGE/f'source/{ident}_Parts.blend';copied.parent.mkdir(parents=True,exist_ok=True);assert not copied.exists();shutil.copy2(path,copied)
            refs={r['object']['name']:{'object_name':r['object']['name'],'expected_fingerprint':r['fingerprint']} for r in reports}
            doorrefs=[]
            for d in doors:
                doorrefs.append({'output_name':'SM_LG_'+ident+'_'+d['tag'],'parts':[refs[n] for n in d['parts']],'pivot_world_m':d['pivot_world_m'],'side':d['side'],'opening_y_min':d['doorway_y_min'],'opening_y_max':d['doorway_y_max'],'opening_z_min':d['doorway_z_min'],'opening_z_max':d['doorway_z_max'],'wall_front_x':d['wall_front_x']})
            request={'job_id':'B6_'+ident+'_Assemble001',**common(copied),'static_parts':[refs[p['object_name']] for p in parts if p['role']=='body'],'doors':doorrefs,'body_name':'SM_LG_'+ident+'_Body','output_blend':str(EXCHANGE/ident/'01_Assembly.blend')}
            newjson(DEST/f'plans/{ident}.json',{'spec':spec,'parts':parts,'doors':doors,'assembly':request,'reports':reports})
            print('ASSET_READY '+ident,flush=True)
        scene=unwrap(client.call('blender_scene_inspect',{'include_hidden':True,'max_objects':2000},'build_final_scene'))
        current={o['name']:o.get('fingerprint') for o in scene['objects']}
        for ob in baseline['scene']['objects']:assert current[ob['name']]==ob.get('fingerprint')
        assert sha(Path(baseline['file']))==baseline['file_sha256']
        print('ALL_SOURCE_OBJECTS_AND_FILE_PRESERVED',flush=True)
    finally:client.close()

def finish(ident,revision=1):
    suffix='' if revision==1 else '_r2';asset_root=EXCHANGE/ident if revision==1 else EXCHANGE/ident/'revision02'
    plan=json.loads((DEST/f'plans/{ident}{suffix}.json').read_text(encoding='utf-8'));spec=plan['spec'];client=transport.Client('buildings',quiet=True)
    def call(command,args,stage):return unwrap(client.call('blender_building_'+command,{'job_id':'B6_'+ident+'_'+stage+suffix,**args},ident+'_'+stage+suffix))
    try:
        request=dict(plan['assembly']);request.pop('job_id')
        out=call('assemble_copy',request,'Assemble001');p=Path(out['output']['path']);print(ident+' ASSEMBLED',flush=True)
        call('inspect',common(p),'InspectAssembly001')
        out=call('uv_copy',{**common(p),'output_blend':str(asset_root/'02_UV.blend')},'UV001');p=Path(out['output']['path']);print(ident+' UV_PASS',flush=True)
        call('inspect',common(p),'InspectUV001')
        if spec['doors']:
            out=call('animate_copy',{**common(p),'output_blend':str(asset_root/'03_Final.blend')},'Animate001');p=Path(out['output']['path'])
        final=call('inspect',common(p),'FinalInspect001')
        assert all(r['uv']['passed'] for r in final['objects'])
        if spec['doors']:assert final['motion']['passed']
        assert final['total_mesh_vertices']<=spec['budget_v'] and final['total_triangles']<=spec['budget_t']
        bounds=final['objects'][0]['mesh']['bounds'];sizes=[bounds['world_max'][i]-bounds['world_min'][i] for i in range(3)]
        assert max(abs(a-b) for a,b in zip(sizes,[spec['depth'],spec['width'],spec['height']]))<1e-5,(ident,sizes)
        newjson(DEST/f'verification/{ident}.json',{'spec':spec,'final_blend':str(p),'final_sha256':sha(p),'report':final,'artifact_root':str(asset_root),'revision':revision});print(ident+' QA_PASS '+str(final['total_mesh_vertices'])+'V '+str(final['total_triangles'])+'T',flush=True)
        views=[(1,'front_right','Closed'),(1,'back','Back')]
        if spec['doors']:views.insert(1,(25,'front_right','Open'))
        for frame,view,label in views:call('preview',{**common(p),'frame':frame,'view':view,'output_path':str(asset_root/f'previews/{label}.png')},'Preview'+label)
        selections=['assembly']+(['body','door_left']+(['door_right'] if spec['doors']==2 else []) if spec['doors'] else [])
        for selection in selections:
            for ext in ['glb','fbx']:
                dest=asset_root/f'exports/{ident}_{selection}.{ext}'
                exported=call('export',{**common(p),'selection':selection,'output_path':str(dest)},'Export_'+selection+'_'+ext)
                qa=call('export_inspect',{'export_path':str(dest),'expected_sha256':exported['sha256']},'Reimport_'+selection+'_'+ext)
                assert all(r['uv']['passed'] for r in qa['meshes'])
        print(ident+' EXPORT_REIMPORT_COMPLETE',flush=True)
    finally:client.close()

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('stage',choices=['preflight','build','finish']);parser.add_argument('--asset');parser.add_argument('--revision',type=int,choices=[1,2],default=1);args=parser.parse_args()
    if args.stage=='preflight':preflight()
    elif args.stage=='build':build()
    else:finish(args.asset,args.revision)
