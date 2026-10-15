"""Read MCP-returned topology to plan bounded selections; no Blender mutations."""
import json
import numpy as np
from precision_workflow import record,DOC,EVIDENCE

rows=[]
for p in sorted(EVIDENCE.glob('precision_base_LG_*.json')):
    name=p.stem.removeprefix('precision_base_');r=record(p.stem);t=r['topology'];v=np.array(t['vertices']);norm=np.array(t['face_normals']);area=np.array(t['face_areas'])
    centers=np.array([v[f].mean(axis=0) for f in t['faces']]);out={'id':name,'bounds':[v.min(axis=0).tolist(),v.max(axis=0).tolist()],'components':len(t['components']),'large_components':sorted(t['components'],key=lambda c:-c['face_count'])[:8]}
    # Roof slope surfaces: high, upward and primarily Y/Z normals after +X-front normalization.
    mask=(centers[:,2]>v[:,2].min()+.52*np.ptp(v[:,2]))&(np.abs(norm[:,0])<.35)&(norm[:,2]>.3)&(np.abs(norm[:,1])>.25)
    candidates=[]
    for side in (-1,1):
        chosen=np.flatnonzero(mask&(norm[:,1]*side>0));points=centers[chosen];weights=area[chosen]
        if len(points)>6:
            # Weighted robust roof regression z = a*y + b, excluding ridge/beam noise.
            fit=np.polyfit(points[:,1],points[:,2],1,w=np.sqrt(weights))
            for _ in range(4):
                err=np.abs(points[:,2]-(points[:,1]*fit[0]+fit[1]));keep=err<max(.025,float(np.quantile(err,.65)))
                fit=np.polyfit(points[keep,1],points[keep,2],1,w=np.sqrt(weights[keep]))
            candidates.append({'side':side,'face_count':len(chosen),'slope':fit[0],'ridge_intercept':fit[1],'y_range':np.quantile(points[:,1],[0,.1,.9,1]).tolist(),'x_range':np.quantile(points[:,0],[0,.1,.9,1]).tolist()})
    out['roof_regressions']=candidates;rows.append(out)
    print(json.dumps({k:x for k,x in out.items() if k!='large_components'}),flush=True)
(DOC/'topology_plan_analysis.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
