"""Reimport final FBX through the bounded read-only MCP tool, never the live scene."""
import hashlib,json,sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from precision_workflow import DOC,call
from blender_mcp_client import Client

def inspect_one(path):
    h=json.loads(path.read_text(encoding='utf-8'));name=h['id'];p=Path(h['exports']['fbx'])
    suffix='_FINAL' if path.stem.endswith('_FINAL') else ''
    label=name+suffix+'_FBX_QA_V2'
    r=call('blender_precision_inspect_fbx',{'job_id':label,'fbx_path':str(p),'expected_sha256':hashlib.sha256(p.read_bytes()).hexdigest()},label)
    g=r['report']['geometry'];expected=h['final_inspection']['geometry']
    assert g['triangles']==expected['triangles'],(name,'triangle mismatch')
    width,depth,height=h['expected_width_depth_height_cm'];want=[depth/100,width/100,height/100]
    actual=r['report']['object']['dimensions'];error=max(abs(a-b) for a,b in zip(want,actual));assert error<1e-5,(name,error)
    assert r['uv_audit']['passed'],(name,r['uv_audit'])
    for field in ('non_manifold_edges','degenerate_faces','invalid_face_normals','non_finite_vertices','loose_vertices','loose_edges','zero_length_edges'):assert g[field]==0,(name,field,g[field])
    r['dimensions_match']=True;r['max_dimension_error_m']=error;r['triangle_count_matches']=True
    dest=DOC/(name+suffix+'_FBX_QA.json');assert not dest.exists();dest.write_text(json.dumps(r,indent=2),encoding='utf-8')
    print(json.dumps({'id':name,'fbx_uv_passed':True,'triangles':g['triangles'],'dimensions_match':True}),flush=True)

if __name__=='__main__':
    c=Client('precision')
    try:
        schema=c.rpc('tools/list',{});dest=DOC/'live_precision_tools_v2_fbx.json'
        if not dest.exists():dest.write_text(json.dumps(schema,indent=2),encoding='utf-8')
    finally:c.close()
    final='final' in sys.argv
    if final:sys.argv.remove('final')
    paths=sorted(DOC.glob('LG_*_FINAL.json' if final else 'LG_*_DELIVERY.json'))
    if sys.argv[1:]:paths=[p for p in paths if int(p.name[3:5]) in [int(v) for v in sys.argv[1:]]]
    with ThreadPoolExecutor(max_workers=2) as pool:list(pool.map(inspect_one,paths))
