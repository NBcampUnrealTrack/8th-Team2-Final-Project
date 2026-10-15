"""Read-only numerical analysis of vertex/face arrays returned by typed MCP."""
import json
from collections import Counter,defaultdict
import numpy as np
from scipy.spatial import cKDTree
from blender_mcp_client import EVIDENCE

def payload(path):
    data=json.loads(path.read_text(encoding='utf-8'))[-1]
    return data.get('structuredContent') or json.loads(next(x['text'] for x in data['content'] if x['type']=='text'))

summaries=[]
for path in sorted(EVIDENCE.glob('topology_LG*.json')):
    result=payload(path)['result']
    top=result['topology']; report=result['report']; name=report['object']['name']
    vertices=np.array(top['vertices']); faces=top['faces']; tree=cKDTree(vertices)
    sym={}
    for axis in (0,1):
        mirrored=vertices.copy(); mirrored[:,axis]*=-1
        distances,_=tree.query(mirrored)
        sym['XY'[axis]]={'mean_distance':float(np.mean(distances)),'p95_distance':float(np.quantile(distances,.95)),'within_0_005':float(np.mean(distances<=.005)),'within_0_01':float(np.mean(distances<=.01))}
    duplicate_faces=[]; seen={}; area_zero=[]
    edges=defaultdict(list)
    for i,face in enumerate(faces):
        key=tuple(sorted(face))
        if key in seen:duplicate_faces.append(i)
        else:seen[key]=i
        points=vertices[face]
        area=sum(np.linalg.norm(np.cross(points[k]-points[0],points[k+1]-points[0]))*.5 for k in range(1,len(face)-1))
        if area<1e-10:area_zero.append(i)
        for a,b in zip(face,face[1:]+face[:1]):edges[tuple(sorted((a,b)))].append(i)
    nonmanifold=[{'edge':list(e),'faces':fs,'bounds':[vertices[list(e)].min(axis=0).tolist(),vertices[list(e)].max(axis=0).tolist()]} for e,fs in edges.items() if len(fs)>2]
    summary={'name':name,'geometry':report['geometry'],'components':len(top['components']),'symmetry_nearest_vertex_only':sym,'coincident_vertex_pairs_1e_5':len(tree.query_pairs(1e-5)),'duplicate_face_ids':duplicate_faces,'zero_area_face_ids':area_zero,'nonmanifold':nonmanifold,'small_loops':[{'id':x['id'],'edges':x['edge_count'],'simple':x['simple_closed'],'bounds':x['bounds']} for x in top['boundary_loops'] if x['bounds']['diameter']<.035]}
    summaries.append(summary)
    print(json.dumps({k:v for k,v in summary.items() if k not in ('nonmanifold','small_loops')}))
(EVIDENCE/'topology_analysis.json').write_text(json.dumps(summaries,indent=2),encoding='utf-8')
