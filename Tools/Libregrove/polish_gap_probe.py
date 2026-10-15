"""Read-only ray/face diagnostics for observed clay-render gaps, from fresh MCP topology."""
import json
import numpy as np
from polish_v3 import ASSETS,DOC,record

def probe(n,view,pixels,axis,plane,stage='BASE_INSPECT'):
    t=record(f'V3_{ASSETS[n]["id"]}_{stage}')['topology'];v=np.array(t['vertices']);center=(v.min(axis=0)+v.max(axis=0))/2;scale=np.ptp(v,axis=0).max()*1.32
    d=np.array({'front_right':[1.,-1,.68],'back_left':[-1.,1,.68]}[view]);d/=np.linalg.norm(d);forward=-d;right=np.cross(forward,[0,0,1]);right/=np.linalg.norm(right);up=np.cross(right,forward)
    out=[]
    for px,py in pixels:
        origin=center+right*((px-384)/768*scale)+up*((384-py)/768*scale)
        point=origin+forward*((plane-origin[axis])/forward[axis]);distance=np.linalg.norm(v-point,axis=1);ids=np.argsort(distance)[:12]
        faces=[i for i,f in enumerate(t['faces']) if set(f)&set(ids[:4])]
        out.append({'pixel':[px,py],'hit':point.tolist(),'vertices':[{'id':int(i),'co':v[i].tolist()} for i in ids],'nearby_faces':[{'id':i,'vertices':t['faces'][i],'normal':t['face_normals'][i]} for i in faces]})
    return {'id':ASSETS[n]['id'],'view':view,'axis':axis,'plane':plane,'probes':out}

results=[probe(4,'front_right',[(350,506),(675,370)],0,1.06),probe(5,'front_right',[(173,348),(194,357)],1,-1.49),probe(5,'back_left',[(446,547),(446,451)],0,-1.065)]
p=DOC/'gap_probe_v3.json';assert not p.exists();p.write_text(json.dumps(results,indent=2),encoding='utf-8')
for r in results:
    print(json.dumps({'id':r['id'],'view':r['view'],'probes':[{k:v for k,v in p.items() if k!='nearby_faces'} for p in r['probes']]}))
