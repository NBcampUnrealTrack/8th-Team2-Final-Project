"""Diagnose exact coincident-contact components; no Blender mutations."""
import json
from polish_ray_hits import hits
from polish_v3 import ASSETS,DOC,record
out=[]
for n,pixels,view in [(4,[(350,506),(675,370)],'front_right'),(5,[(173,348),(194,357)],'front_right'),(5,[(446,547),(446,451)],'back_left'),(1,[(277,260),(286,253)],'front_right')]:
    t=record(f'V3_{ASSETS[n]["id"]}_Reduce02_AFTER')['topology'];r=hits(n,pixels,view,'Reduce02_AFTER')
    for p in r['probes']:
        for h in p['hits']:
            comps=[c for c in t['components'] if h['face'] in c['face_ids']]
            h['components']=[{k:v for k,v in c.items() if k!='face_ids'} for c in comps]
    out.append(r)
dest=DOC/'contact_plan01.json';assert not dest.exists();dest.write_text(json.dumps(out,indent=2),encoding='utf-8')
for r in out:
    print(json.dumps({'n':r['n'],'view':r['view'],'probes':[{'pixel':p['pixel'],'hits':[{'face':h['face'],'point':h['point'],'normal':h['normal'],'components':h['components']} for h in p['hits'][:3]]} for p in r['probes']]}))
