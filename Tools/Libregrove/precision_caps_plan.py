"""Select only small simple, planar, non-duplicate cap loops from MCP topology."""
import json
import numpy as np
from precision_workflow import record,DOC,EVIDENCE

plans=[]
for p in sorted(DOC.glob('LG_*_Split22.json')):
    h=json.loads(p.read_text(encoding='utf-8'));name=h['id'];actual=h['stage'];t=record(name+'_'+actual+'_after')['topology'];v=np.array(t['vertices']);face_sets={frozenset(f) for f in t['faces']};accepted=[]
    for loop in t['boundary_loops']:
        if not loop['simple_closed'] or loop['edge_count']>12 or loop['bounds']['diameter']>.22 or loop['edge_count']<3:continue
        if frozenset(loop['vertex_ids']) in face_sets:continue
        pts=v[loop['vertex_ids']];_,s,_=np.linalg.svd(pts-pts.mean(axis=0),full_matrices=False)
        if s[1]<1e-5 or s[-1]/max(s[0],1e-8)>.04:continue
        accepted.append({'id':loop['id'],'edges':loop['edge_count'],'diameter':loop['bounds']['diameter'],'center':loop['bounds']['center']})
    plans.append({'id':name,'source_stage':actual,'source_fingerprint':h['after']['fingerprint'],'cap_candidates':accepted})
    print(json.dumps({'id':name,'candidate_count':len(accepted),'examples':accepted[:15]}))
(DOC/'small_caps_candidates.json').write_text(json.dumps(plans,indent=2),encoding='utf-8')
