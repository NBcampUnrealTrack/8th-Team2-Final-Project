"""Read-only dominant wall plane analysis for inspected architectural meshes."""
import json
import numpy as np
from precision_workflow import record,DOC

results=[]
for name in ['LG_01_Cottage_A','LG_02_Inn','LG_03_VillageHall','LG_04_Workshop','LG_05_WatermillHouse']:
    t=record('precision_base_'+name)['topology'];v=np.array(t['vertices']);n=np.array(t['face_normals']);area=np.array(t['face_areas']);c=np.array([v[f].mean(axis=0) for f in t['faces']]);planes=[]
    for axis in (0,1):
        for side in (-1,1):
            sel=np.flatnonzero((n[:,axis]*side>.97)&(area>.025)&(c[:,axis]*side>np.ptp(v[:,axis])*.22)&(c[:,2]>.2)&(c[:,2]<v[:,2].max()*.7))
            if not len(sel):continue
            largest=sorted(sel,key=lambda i:-area[i])[:12]
            planes.append({'axis':axis,'side':side,'faces':[{'id':int(i),'center':c[i].tolist(),'area':float(area[i]),'bounds':[v[t['faces'][i]].min(axis=0).tolist(),v[t['faces'][i]].max(axis=0).tolist()]} for i in largest]})
    results.append({'id':name,'planes':planes})
    print(json.dumps({'id':name,'planes':[{'axis':p['axis'],'side':p['side'],'top3':p['faces'][:3]} for p in planes]}))
(DOC/'wall_plane_analysis.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
