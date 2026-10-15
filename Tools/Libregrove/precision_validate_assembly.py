"""Reopen the actual clean .blend and compare all nine meshes to final sources."""
import json
from concurrent.futures import ThreadPoolExecutor
from precision_workflow import DOC,record
from precision_stage import inspect

def check(pair):
    i,h=pair;name=h['id'];assembly=json.loads((DOC/'clean_assembly_final.json').read_text(encoding='utf-8'))
    r=inspect(assembly['output_blend'],'SM_'+name,name+'_ASSEMBLY_FINAL_QA',uv=True);source=record(name+'_EXPORT_FINAL_BEFORE')
    assert r['report']['geometry']==source['report']['geometry']
    assert r['topology']['vertices']==source['topology']['vertices'] and r['topology']['faces']==source['topology']['faces']
    assert r['report']['uv_layers']==source['report']['uv_layers'] and r['uv_audit']==source['uv_audit']
    assert r['report']['object']['location']==[(i%3)*8.,(i//3)*8.,0.]
    assert r['report']['object']['dimensions']==source['report']['object']['dimensions']
    return {'id':name,'exact_local_geometry_match':True,'uv_audit_match':True,'dimensions_match':True,'display_location':r['report']['object']['location'],'fingerprint':r['report']['fingerprint']}

hs=[json.loads(p.read_text(encoding='utf-8')) for p in sorted(DOC.glob('LG_*_FINAL.json'))];assert len(hs)==9
with ThreadPoolExecutor(max_workers=2) as pool:checks=list(pool.map(check,enumerate(hs)))
dest=DOC/'clean_mesh_checks.json';assert not dest.exists();dest.write_text(json.dumps(checks,indent=2),encoding='utf-8');print(json.dumps({'passed_meshes':len(checks)}))
