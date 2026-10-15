"""Read-only cap candidates; skips loops with existing internal diagonals."""
import sys,collections,json
from task import ROOT,PROJECT,read,write
for name in sys.argv[1:]:
 p=ROOT/'verification'/(name+'_current_topology.json')
 d=read(p) if p.exists() else read(PROJECT/'Saved/CindehearthRepair'/('dense_arch_'+name.lower()+'_topology004')/'result.json')['result']
 t=d['topology'];e=set()
 for f in t['faces']:
  for i,a in enumerate(f):e.add(tuple(sorted((a,f[(i+1)%len(f)]))))
 rows=[]
 for x in t['boundary_loops']:
  vs=set(x['vertex_ids']);chords=sum(a in vs and b in vs for a,b in e)-x['edge_count']
  existing=any(set(f)==vs for f in t['faces'])
  rows.append(dict(id=x['id'],simple=x['simple_closed'],edges=x['edge_count'],chords=chords,existing_face=existing,bounds=x['bounds']))
 write(ROOT/'verification'/(name+'_cap_candidates.json'),rows)
 print(name,'safe IDs',','.join(str(x['id']) for x in rows if x['simple'] and x['chords']==0 and not x['existing_face']),'SKIP',[(x['id'],x['simple'],x['chords'],x['existing_face']) for x in rows if not x['simple'] or x['chords'] or x['existing_face']])
