import json,sys
from concurrent.futures import ThreadPoolExecutor
from precision_stage import stage
from precision_workflow import DOC,OUT,EVIDENCE,record
from blender_mcp_client import Client

def render(name,stage_name,view):
    h=json.loads((DOC/(name+'_'+stage_name+'.json')).read_text(encoding='utf-8'));c=Client('repair')
    try:
        label=name+'_'+stage_name+'_'+view;dest=OUT/'views'/(label+'.png');dest.parent.mkdir(exist_ok=True)
        return c.call('blender_mesh_review_render',{'job_id':label,'blend_path':h['output_blend'],'object_name':h['output_object'],'output_path':str(dest),'view':view,'mode':'clay'},label)
    finally:c.close()

def roof(name):
    plan=next(x for x in json.loads((DOC/'roof_backing_plan.json').read_text(encoding='utf-8')) if x['id']==name)
    stage(name,'Roof23','Split22',{'tool':'blender_backing_panels_copy','panels':plan['panels'],'max_bounds_growth':.002})
    stage(name,'Normals24','Roof23',{'tool':'blender_sharp_normals_copy','angle_degrees':45})

def uv(name,source):
    stage(name,'Tri25',source,{'tool':'blender_triangulate_copy'})
    stage(name,'UV26','Tri25',{'tool':'blender_uv_native_copy','method':'smart_project','angle_degrees':60,'margin_fraction':.001})

def repair_uv(name,source='UV26',output='UV27'):
    h=json.loads((DOC/(name+'_'+source+'.json')).read_text(encoding='utf-8'));q=h['uv_audit']
    ids=sorted(set(q['degenerate_face_ids'])|{i for hit in q['overlap_details'] for i in hit['face_ids']})
    assert ids,'No affected UV faces; no mutation needed'
    def options(before):
        assert before['report']['fingerprint']==h['after']['fingerprint'],'UV evidence is stale'
        return {'tool':'blender_uv_faces_reproject_copy','face_ids':ids,'margin_fraction':.001}
    return stage(name,output,source,options)

if __name__=='__main__':
    assets=[json.loads(p.read_text(encoding='utf-8'))['id'] for p in sorted(EVIDENCE.glob('review_handoff_*.json'))]
    numbers=[int(n) for n in sys.argv[2:]];assets=[n for n in assets if int(n[3:5]) in numbers]
    if sys.argv[1]=='roof':
        with ThreadPoolExecutor(max_workers=2) as pool:list(pool.map(roof,assets))
    elif sys.argv[1]=='render':
        with ThreadPoolExecutor(max_workers=2) as pool:list(pool.map(lambda x:render(*x),[(n,'Normals24',v) for n in assets for v in ('front_right','back_left')]))
    elif sys.argv[1]=='uv':
        with ThreadPoolExecutor(max_workers=2) as pool:list(pool.map(lambda n:uv(n,'Normals24'),assets))
    elif sys.argv[1]=='repair_uv':
        with ThreadPoolExecutor(max_workers=2) as pool:list(pool.map(repair_uv,assets))
    elif sys.argv[1]=='repair_uv2':
        with ThreadPoolExecutor(max_workers=2) as pool:list(pool.map(lambda n:repair_uv(n,'UV27','UV28'),assets))
    elif sys.argv[1]=='repair_uv3':
        with ThreadPoolExecutor(max_workers=2) as pool:list(pool.map(lambda n:repair_uv(n,'UV28','UV29'),assets))
    elif sys.argv[1]=='repair_uv32':
        with ThreadPoolExecutor(max_workers=2) as pool:list(pool.map(lambda n:repair_uv(n,'UV32','UV33'),assets))
    elif sys.argv[1]=='organic_normals':
        with ThreadPoolExecutor(max_workers=2) as pool:list(pool.map(lambda n:stage(n,'Normals24','Split22',{'tool':'blender_sharp_normals_copy','angle_degrees':60 if int(n[3:5]) in (6,8) else 45}),assets))
