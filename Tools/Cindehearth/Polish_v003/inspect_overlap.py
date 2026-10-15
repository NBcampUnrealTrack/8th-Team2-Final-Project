import json,collections,sys,math
from pathlib import Path
for name in sys.argv[1:]:
 p=Path('Tools/Cindehearth/Polish_v003/verification')/(name+'_current_topology.json')
 d=json.loads(p.read_text()) if p.exists() else json.loads(Path('Saved/CindehearthRepair', 'polish_'+name.lower()+'_topology003','result.json').read_text())['result']
 t=d['topology'];e=collections.defaultdict(list)
 for j,f in enumerate(t['faces']):
  for i,a in enumerate(f):e[tuple(sorted((a,f[(i+1)%len(f)])))].append(j)
 print(name)
 for k,v in e.items():
  if len(v)>2:
   print('EDGE',k,'FACES',v)
   for j in v:print(j,[[round(x,4) for x in t['vertices'][i]] for i in t['faces'][j]])
