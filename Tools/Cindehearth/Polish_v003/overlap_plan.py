"""Read-only proposal for exact intersecting face IDs; never edits a model."""
import sys,collections,math
from task import ROOT,PROJECT,read,write
def cross(a,b):return [a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]]
for name in sys.argv[1:]:
 p=ROOT/'verification'/(name+'_current_topology.json');d=read(p) if p.exists() else read(PROJECT/'Saved/CindehearthRepair'/('polish_'+name.lower()+'_topology003')/'result.json')['result']
 t=d['topology'];edges=collections.defaultdict(set);faces=t['faces'];verts=t['vertices'];areas=[]
 for j,f in enumerate(faces):
  for i,a in enumerate(f):edges[tuple(sorted((a,f[(i+1)%len(f)])))].add(j)
  v=[verts[k] for k in f];a=0
  for i in range(1,len(v)-1):a+=math.sqrt(sum(x*x for x in cross([v[i][k]-v[0][k] for k in range(3)],[v[i+1][k]-v[0][k] for k in range(3)])))*.5
  areas.append(a)
 selected=[]
 while True:
  bad=[v for v in edges.values() if len(v)>2]
  if not bad:break
  candidates=set().union(*bad)
  # Prefer surplus triangles from P2 fans, retaining large quad surface patches.
  def score(i):return ((8 if len(faces[i])==3 else 1)*sum(i in v for v in bad)/(areas[i]+.003))
  chosen=max(candidates,key=score);selected.append(chosen)
  for v in edges.values():v.discard(chosen)
 rows=[dict(id=i,corners=len(faces[i]),area=round(areas[i],6),center=[round(sum(verts[j][k] for j in faces[i])/len(faces[i]),4) for k in range(3)]) for i in sorted(selected)]
 write(ROOT/'verification'/(name+'_overlap_proposal.json'),dict(source_fingerprint=d['report']['fingerprint'],selection=sorted(selected),faces=rows))
 print(name,'faces',len(rows),'tri',sum(x['corners']==3 for x in rows),'area',round(sum(areas[i] for i in selected),3),'ids',','.join(str(x['id']) for x in rows),'quads',[x for x in rows if x['corners']!=3])
