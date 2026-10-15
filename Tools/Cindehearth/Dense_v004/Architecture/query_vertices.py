"""Read-only neighborhood and surface query for inspected exact cap vertices."""
import sys,json,math
from task import ROOT,read
name=sys.argv[1]
t=read(ROOT/'verification'/(name+'_current_topology.json'))['topology']
if len(sys.argv)>2:
    p=list(map(float,sys.argv[2:5]))
    rows=sorted(enumerate(t['vertices']),key=lambda q:math.dist(q[1],p))[:12]
    print([(i,[round(v,4) for v in co]) for i,co in rows])
else:
    print([(c['id'],c['face_count'],{k:[round(x,3) for x in v] if isinstance(v,list) else round(v,3) for k,v in c['bounds'].items()}) for c in t['components'] if c['face_count']>15])
