"""Typed Blender / authenticated Unreal MCP evidence transport for texture handoff."""
import json,os,sys,urllib.request,uuid,subprocess,csv,hashlib,shutil
from pathlib import Path
import blender_mcp_client as blender
ROOT=Path(__file__).resolve().parents[2]
DEST=ROOT/'Docs/Libregrove/Unreal_Textured_V7_20260908'
blender.EVIDENCE=DEST/'evidence'
EXCHANGE=Path('C:/Users/Ndus_072/AppData/Local/Revvy/BlenderExchange/Libregrove_Textured_V7_20260908')
_UE_URL=None

def save(name,value):
    path=DEST/name;path.parent.mkdir(parents=True,exist_ok=True)
    with path.open('x',encoding='utf-8') as f:json.dump(value,f,ensure_ascii=False,indent=2)
    return value

def unwrap(value):
    if 'content' in value:
        value=value.get('structuredContent') or json.loads(next(c['text'] for c in value['content'] if c['type']=='text'))
    value=value.get('result',value)
    return value.get('result',value) if isinstance(value,dict) and value.get('state')=='finished' else value

def ue_rpc(method,params):
    # .mcp.json currently points at an older endpoint. Use the CLI-resolved,
    # verified active project proxy; never change permissions or restart it.
    global _UE_URL
    if _UE_URL is None:
        status=json.loads(subprocess.check_output([str(ROOT/'Plugins/Revvy/revvy.cmd'),'status','--json'],encoding='utf-8'))
        assert Path(status['active_project']['path']).resolve()==ROOT.resolve()
        assert status['ue_running'];_UE_URL=status['proxy_url']
    url=_UE_URL
    token=os.environ.get('REVVY_FULL_ACCESS_TOKEN');assert token,'Existing Full grant token not available in process environment'
    request={'jsonrpc':'2.0','id':uuid.uuid4().hex,'method':method,'params':params}
    req=urllib.request.Request(url,data=json.dumps(request).encode(),headers={'Content-Type':'application/json','Accept':'application/json, text/event-stream','Authorization':'Bearer '+token})
    with urllib.request.urlopen(req,timeout=60) as response:raw=response.read().decode('utf-8')
    if raw.lstrip().startswith(('event:','data:')):raw='\n'.join(line[5:].strip() for line in raw.splitlines() if line.startswith('data:'))
    result=json.loads(raw)
    if result.get('error'):raise RuntimeError(result['error'])
    return result['result']

def ue_call(name,args,evidence):
    save('evidence/'+evidence+'_request.json',{'tool':name,'arguments':args})
    result=ue_rpc('tools/call',{'name':name,'arguments':args})
    save('evidence/'+evidence+'_result.json',result)
    if result.get('isError'):raise RuntimeError(result)
    value=result.get('structuredContent') or json.loads(next(c['text'] for c in result['content'] if c['type']=='text'))
    if isinstance(value,dict) and value.get('success') is False:raise RuntimeError(value)
    return value

def preflight():
    c=blender.Client('main',quiet=True)
    try:
        schema=save('blender_schema.json',c.rpc('tools/list',{}))
        names={t['name'] for t in schema['tools']}
        assert {'blender_bridge_status','blender_scene_inspect','blender_mesh_inspect','blender_model_export','blender_save_copy'}<=names
        status=unwrap(c.call('blender_bridge_status',{},'V7_blender_status'))
        save('blender_status.json',status)
        assert status['blender']['runtime_supported'] and status['mcp']['bridge_compatibility']['compatible']
        scene=unwrap(c.call('blender_scene_inspect',{'include_hidden':True,'max_objects':2000},'V7_blender_scene'))
        save('blender_scene.json',scene)
        print(json.dumps({'status':status,'scene':scene},ensure_ascii=False,indent=2))
    finally:c.close()

def ue_discovery():
    schema=save('unreal_schema.json',ue_rpc('tools/list',{}))
    print(json.dumps([t['name'] for t in schema['tools']],indent=2))
    print(json.dumps(ue_call('get_editor_context',{},'V7_ue_context'),indent=2))
    print(json.dumps(ue_call('manage_asset',{'action':'search','search_term':'Libregrove'},'V7_ue_existing'),indent=2))

def inspect_sources():
    scene=json.loads((DEST/'blender_scene.json').read_text(encoding='utf-8'));c=blender.Client('main',quiet=True)
    try:
        reports=[]
        for i,ob in enumerate(scene['objects']):
            r=unwrap(c.call('blender_mesh_inspect',{'object_name':ob['name']},f'V7_source_mesh_{i:02d}'))
            assert r['fingerprint']==ob['fingerprint'];reports.append(r)
            print(json.dumps({'name':ob['name'],'geometry':r['geometry'],'materials':r.get('materials'),'bounds':r['bounds'],'uv':r.get('uv_layers')},ensure_ascii=False))
        save('source_meshes.json',reports)
    finally:c.close()

def prepare_exports():
    baseline=json.loads((DEST/'blender_scene.json').read_text(encoding='utf-8'))
    original_reports=json.loads((DEST/'source_meshes.json').read_text(encoding='utf-8'))
    csvpath=ROOT/'Docs/Libregrove/Export_Texturing_V3_20260908/모델별_버텍스_크기_UV_검수표.csv'
    with csvpath.open(encoding='utf-8-sig') as f:catalog={r['id']:r for r in csv.DictReader(f)}
    c=blender.Client('main',quiet=True);exports=[]
    try:
        current=unwrap(c.call('blender_scene_inspect',{'include_hidden':True,'max_objects':2000},'V7_before_backup'))
        assert current==baseline,'Scene changed; re-inspect before preparing copies'
        backup=unwrap(c.call('blender_save_copy',{'path':str(EXCHANGE/'Backup/00_UserTextured_Original.blend'),'overwrite':False},'V7_source_backup'))
        save('source_backup.json',backup)
        for i,source in enumerate(original_reports):
            name=source['object']['name'];ident=name.removesuffix('.fbx');spec=catalog[ident]
            before=unwrap(c.call('blender_mesh_inspect',{'object_name':name},'V7_'+ident+'_before'))
            assert before['fingerprint']==source['fingerprint']
            height=before['bounds']['local_max'][2]-before['bounds']['local_min'][2]
            factor=float(spec['height_cm'])/100/height;newname='SM_'+ident+'_V7'
            args={'source_object_name':name,'output_object_name':newname,'operations':[{'type':'transform_vertices','scale':[factor]*3},{'type':'transform_object','location':[0,0,0],'rotation_degrees':[0,0,0],'scale':[1,1,1]}]}
            result=unwrap(c.call('blender_mesh_edit',args,'V7_'+ident+'_resize'))
            assert result['source_preserved']
            after=unwrap(c.call('blender_mesh_inspect',{'object_name':newname},'V7_'+ident+'_scaled'))
            assert after['geometry']['triangles']==before['geometry']['triangles'] and after['geometry']['vertices']==before['geometry']['vertices']
            assert after['uv_layers']==before['uv_layers'] and after['materials']==before['materials']
            for error in ['degenerate_faces','non_manifold_edges','boundary_edges','non_finite_vertices','invalid_face_normals']:assert after['geometry'][error]<=before['geometry'][error]
            dims=[after['bounds']['world_max'][a]-after['bounds']['world_min'][a] for a in range(3)]
            assert abs(dims[2]-float(spec['height_cm'])/100)<1e-5
            exported=unwrap(c.call('blender_model_export',{'path':str(EXCHANGE/f'Export/{newname}.glb'),'object_names':[newname],'overwrite':False,'apply_modifiers':True},'V7_'+ident+'_export'))
            unchanged=unwrap(c.call('blender_mesh_inspect',{'object_name':name},'V7_'+ident+'_original_after'))
            assert unchanged['fingerprint']==before['fingerprint']
            local=DEST/f'Export/{newname}.glb';local.parent.mkdir(parents=True,exist_ok=True);assert not local.exists();shutil.copy2(exported['path'],local)
            row={'id':ident,'name_ko':spec['name_ko'],'source_name':name,'output_name':newname,'source_fingerprint':before['fingerprint'],'output_report':after,'scale_factor':factor,'dimensions_cm':[d*100 for d in dims],'glb_path':str(local),'sha256':hashlib.sha256(local.read_bytes()).hexdigest(),'recommended_vertex_max':int(spec['recommended_vertex_max']),'initial_triangle_budget':int(spec['initial_triangle_budget']),'ue_destination':'/Game/Libregrove/Textured_V7/Models/'+ident,'geometry_uv_unchanged_except_uniform_scale':True}
            save('exports/'+ident+'.json',row);exports.append(row);print('EXPORTED '+ident,flush=True)
        save('export_manifest.json',exports)
        current=unwrap(c.call('blender_scene_inspect',{'include_hidden':True,'max_objects':2000},'V7_after_exports_scene'))
        lookup={o['name']:o for o in current['objects']}
        for ob in baseline['objects']:assert lookup[ob['name']]==ob
        save('source_preservation.json',{'original_objects_exactly_preserved':9,'new_scaled_copies':9,'source_was_unsaved':True,'passed':True})
        c.call('blender_save_copy',{'path':str(EXCHANGE/'Libregrove_Textured_V7_WorkingCopy.blend'),'overwrite':False},'V7_final_working_copy')
    finally:c.close()

if __name__=='__main__':
    sys.stdout.reconfigure(encoding='utf-8')
    {'blender':preflight,'unreal':ue_discovery,'inspect':inspect_sources,'exports':prepare_exports}[sys.argv[1]]()
