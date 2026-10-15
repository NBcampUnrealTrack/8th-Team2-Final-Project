"""Texture-free UV finalization, each mutation inspected before and afterward."""
import json,sys,collections
from concurrent.futures import ThreadPoolExecutor
from polish_v3 import ASSETS,DOC,stage,record

def finish(n,resume=None):
    name=ASSETS[n]['id'];source='BASE' if n in (6,8,9) else ('Contact12' if n==5 else 'Normals05')
    h=json.loads((DOC/(name+'_'+resume+'.json')).read_text(encoding='utf-8')) if resume else stage(n,source,'UV06','blender_uv_native_copy',{'method':'pack' if source=='BASE' else 'smart_project','angle_degrees':60,'margin_fraction':.002})
    for output in ('UV07','UV08','UV09'):
        q=h['uv_audit']
        if q['passed']:break
        ids=sorted(set(q['degenerate_face_ids'])|{i for pair in q['overlap_details'] for i in pair['face_ids']})
        assert ids and len(ids)<2000,(name,q)
        expected=h['after']['fingerprint']
        def options(r):
            assert r['report']['fingerprint']==expected
            return {'face_ids':ids,'margin_fraction':.002,'minimum_uv_area':1e-8}
        h=stage(n,h['stage'],output,'blender_uv_faces_reproject_copy',options)
    assert h['uv_audit']['passed'],(name,'UV repair failed')
    risky=[r['face_id'] for r in h['uv_export_precision']['risk_faces'] if r['glb_float32_uv_area']<=1e-12]
    if risky:h=stage(n,h['stage'],'UV10','blender_uv_sliver_guard_copy',{'face_ids':sorted(set(risky))})
    assert h['uv_audit']['passed']
    assert not any(r['glb_float32_uv_area']<=1e-12 for r in h['uv_export_precision']['risk_faces'])
    g=h['after']['geometry']
    for f in ('non_manifold_edges','degenerate_faces','invalid_face_normals','non_finite_vertices','loose_vertices','loose_edges','zero_length_edges'):assert not g[f],(name,f)
    top=record(f'V3_{name}_{h["stage"]}_AFTER')['topology'];faces=[tuple(sorted(tuple(top['vertices'][i]) for i in f)) for f in top['faces']]
    if len(set(faces))!=len(faces):
        duplicates=collections.defaultdict(list)
        for i,key in enumerate(faces):duplicates[key].append(i)
        pairs=[{'keep_face':group[0],'remove_face':i} for group in duplicates.values() for i in group[1:]]
        h=stage(n,h['stage'],'Dup11','blender_duplicate_faces_copy',{'pairs':pairs});g=h['after']['geometry']
        assert h['uv_audit']['passed']
        top=record(f'V3_{name}_Dup11_AFTER')['topology'];faces=[tuple(sorted(tuple(top['vertices'][i]) for i in f)) for f in top['faces']]
        assert len(set(faces))==len(faces)
    accepted={'id':name,'stage':h['stage'],'output_blend':h['output_blend'],'output_object':h['output_object'],'fingerprint':h['after']['fingerprint'],'geometry':g,'uv_overlap_pairs':0,'uv_degenerate_triangles':0,'uv_margin_fraction':.002,'textures_authored':False}
    dest=DOC/(name+'_ACCEPTED.json');assert not dest.exists();dest.write_text(json.dumps(accepted,indent=2),encoding='utf-8');print(json.dumps(accepted),flush=True)

if __name__=='__main__':
    if sys.argv[1]=='resume':finish(int(sys.argv[2]),sys.argv[3])
    else:
        with ThreadPoolExecutor(max_workers=2) as pool:list(pool.map(finish,[int(v) for v in sys.argv[1:]]))
