"""Orchestrate explicit typed MCP calls; never execute arbitrary Blender code."""
import json,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parent
PROJECT=ROOT.parents[2];PIPE=ROOT.parent/'Pipeline'
PY=PROJECT/'Saved/CindehearthTripoEnv/Scripts/python.exe'
EX=Path.home()/'AppData/Local/Revvy/BlenderExchange/Cindehearth/Polish_v003'
EX.mkdir(parents=True,exist_ok=True);(EX/'Review').mkdir(exist_ok=True)
CAT={x['name']:x for x in json.loads((ROOT/'catalog.json').read_text(encoding='utf-8'))}
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def write(p,x):p.write_text(json.dumps(x,indent=2),encoding='utf-8')
def payload(d):return d.get('structuredContent') or json.loads(d['content'][0]['text'])
def run(cmd,log):
    with log.open('w',encoding='utf-8') as f:r=subprocess.run([str(x) for x in cmd],cwd=PROJECT,stdout=f,stderr=subprocess.STDOUT)
    if r.returncode:raise RuntimeError('Typed MCP failed; inspect '+str(log))
def mcp(name,tool,args):
    request=ROOT/'requests'/(name+'_'+tool+'.json');out=ROOT/'verification'/(name+'_'+tool+'.json')
    write(request,args)
    run([PY,PIPE/'mcp_client.py','tripo',tool,'--args-file',request,'--out',out],ROOT/'jobs'/(name+'_'+tool+'.log'))
    d=payload(read(out));assert not d.get('success') is False;return d
def blender(name,stage,tool,args):
    evidence='polish_'+name+'_'+stage;out=PIPE/'verification'/(evidence+'.json')
    if not out.exists():
        request=ROOT/'requests'/(evidence+'.json');write(request,args)
        run([PY,PIPE/'blender_step.py',tool,request,evidence],ROOT/'jobs'/(evidence+'.log'))
    d=payload(read(out)[-1]);assert d.get('success'),d
    v=d['result'];assert v.get('state','finished')=='finished',v
    return v.get('result',v)
def repair(name,stage,tool,args):
    job='polish_'+name.lower()+'_'+stage
    out=PROJECT/'Saved/CindehearthRepair'/job/'result.json'
    if not out.exists():
        request=ROOT/'requests'/(job+'.json');write(request,dict(args,job_id=job))
        run([PY,PIPE/'repair_step.py',tool,request],ROOT/'jobs'/(job+'.log'))
    d=read(out);assert d.get('success') and d['state']=='finished',d
    return d['result']
def generate(name):
    out=ROOT/'verification'/(name+'_tripo_generate_p2_image.json')
    if out.exists():
        d=payload(read(out));assert d.get('task_id'),d
    else:
        d=mcp(name,'tripo_generate_p2_image',dict(asset_name='CH_Polish_'+name+'_P2_v003',request_id='CH_Polish_'+name+'_P2_20260905_v003',image_path=str(ROOT/'reference'/(name+'.png')),face_limit=CAT[name]['faces']))
    assert d.get('task_id'),d
    print(name,'SUBMITTED',d['task_id'],flush=True)
def poll(name):
    d=payload(read(ROOT/'verification'/(name+'_tripo_generate_p2_image.json')))
    d=mcp(name,'tripo_task_status',dict(task_id=d['task_id'],api_version='v3'))
    print(name,d['status'],d.get('progress'),flush=True)
def download(name):
    d=payload(read(ROOT/'verification'/(name+'_tripo_task_status.json')));assert d['status']=='success',d['status']
    out=ROOT/'verification'/(name+'_tripo_download_result.json')
    if not out.exists():d=mcp(name,'tripo_download_result',dict(task_id=d['task_id'],api_version='v3',asset_name='CH_Polish_'+name+'_P2_v003'))
    else:d=payload(read(out))
    assert Path(d['files']['model']).is_file();print(name,'DOWNLOADED',flush=True)
def prepare(name):
    file=payload(read(ROOT/'verification'/(name+'_tripo_download_result.json')))['files']['model']
    d=blender(name,'import','blender_model_import',dict(path=file,name_prefix='CH_Polish_'+name+'_REF__'))
    assert len(d['mesh_reports'])==1,d.keys()
    r=d['mesh_reports'][0];lo=r['bounds']['local_min'];hi=r['bounds']['local_max'];size=[hi[i]-lo[i] for i in range(3)]
    s=3/max(size);obj='CH_Polish_'+name+'_Game_v003'
    v=blender(name,'normalize','blender_mesh_edit',dict(source_object_name=r['object']['name'],output_object_name=obj,operations=[dict(type='transform_object',rotation_degrees=[0,0,0]),dict(type='transform_vertices',scale=[s]*3,translation=[-(lo[0]+hi[0])*.5*s,-(lo[1]+hi[1])*.5*s,-lo[2]*s]),dict(type='delete_loose')]))
    fp=EX/(obj+'.blend')
    blender(name,'save','blender_save_copy',dict(path=str(fp),overwrite=False))
    result=repair(name,'topology003','blender_topology_inspect',dict(blend_path=str(fp),object_name=obj))
    write(ROOT/'verification'/(name+'_source.json'),dict(blend=str(fp),object=obj,fingerprint=result['report']['fingerprint'],report=result['report']))
    topo=result['topology'];loops=topo['boundary_loops']
    summary=dict(name=name,geometry=result['report']['geometry'],dimensions=result['report']['object']['dimensions'],components=[dict(id=c['id'],faces=c['face_count'],diameter=round(c['bounds']['diameter'],4)) for c in topo['components']],loops=[dict(id=c['id'],edges=c['edge_count'],simple=c['simple_closed'],diameter=round(c['bounds']['diameter'],4),center=c['bounds']['center']) for c in loops])
    write(ROOT/'verification'/(name+'_topology_summary.json'),summary)
    print(name,'PREPARED',result['report']['geometry'],'components',len(topo['components']),'loops',len(loops),flush=True)
def review(name):
    d=read(ROOT/'verification'/(name+'_source.json'))
    for view in ['front_right','back_left']:
        repair(name,'review003_'+view,'blender_mesh_review_render',dict(blend_path=d['blend'],object_name=d['object'],output_path=str(EX/'Review'/(name+'_'+view+'_003.png')),view=view,mode='textured'))
    print(name,'RENDERED',flush=True)
def export(name):
    d=read(ROOT/'verification'/(name+'_source.json'));r=d['report'];g=r['geometry']
    assert g['non_manifold_edges']==0 and g['loose_vertices']==0 and g['degenerate_faces']==0,g
    dest=EX/(d['object']+'.glb')
    repair(name,d['object']+'_export','blender_repair_export',dict(blend_path=d['blend'],object_name=d['object'],expected_fingerprint=d['fingerprint'],output_path=str(dest)))
    write(ROOT/'verification'/(name+'_export.json'),dict(name=name,source=str(dest),blend=d['blend'],object=d['object'],geometry=g))
    print(name,'EXPORTED',flush=True)
if __name__=='__main__':
    stage=sys.argv[1];assert stage in ['generate','poll','download','prepare','review','export']
    for n in sys.argv[2:]:assert n in CAT;globals()[stage](n)
