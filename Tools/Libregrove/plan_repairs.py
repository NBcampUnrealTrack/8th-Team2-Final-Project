"""Read-only boundary classification from typed topology snapshots."""
import json
from collections import defaultdict
import numpy as np
from blender_mcp_client import EVIDENCE

plans=[]
paths=[EVIDENCE/('topology_'+a+'.json') for a in ['LG_01_Cottage_A','LG_02_Inn','LG_03_VillageHall','LG_04_Workshop','LG_05_WatermillHouse','LG_06_NaiaShelter_Reduced02','LG_07_Sanctuary','LG_08_BroadleafTree_Balanced03','LG_09_MarketStall']]
for path in paths:
    raw=json.loads(path.read_text(encoding='utf-8'))[-1]
    data=raw.get('structuredContent') or json.loads(raw['content'][0]['text'])
    result=data['result'];top=result['topology']; report=result['report'];name=report['object']['name']
    vs=np.array(top['vertices']);fs=top['faces'];edges=defaultdict(list)
    areas=[];zero=[]
    for i,face in enumerate(fs):
        pts=vs[face]
        area=sum(np.linalg.norm(np.cross(pts[k]-pts[0],pts[k+1]-pts[0]))*.5 for k in range(1,len(face)-1))
        areas.append(area)
        if area<=1e-10:zero.append(i)
        for a,b in zip(face,face[1:]+face[:1]):edges[tuple(sorted((a,b)))].append(i)
    components={i:c for c in top['components'] for i in c['face_ids']}
    face_sets={frozenset(face) for face in fs}
    caps=[]; candidates=[]
    for loop in top['boundary_loops']:
        if not loop['simple_closed'] or loop['bounds']['diameter']>.2:continue
        ids=set(loop['vertex_ids']); adjacency=defaultdict(list);neighbor_faces=set()
        if frozenset(ids) in face_sets:continue
        for (a,b),faceids in edges.items():
            if len(faceids)==1 and a in ids and b in ids:
                adjacency[a].append(b);adjacency[b].append(a);neighbor_faces.update(faceids)
        if not adjacency or any(len(v)!=2 for v in adjacency.values()):continue
        start=min(adjacency);ordered=[start];previous=None;current=start
        while True:
            following=next(v for v in adjacency[current] if v!=previous)
            if following==start:break
            ordered.append(following);previous,current=current,following
            if len(ordered)>len(ids):raise RuntimeError('Invalid loop')
        pts=vs[ordered];center=pts.mean(axis=0);_,_,axes=np.linalg.svd(pts-center,full_matrices=False)
        normal=axes[-1];plane_error=float(np.max(np.abs((pts-center)@normal)))
        area=float(np.linalg.norm(sum(np.cross(pts[k]-center,pts[(k+1)%len(pts)]-center) for k in range(len(pts))))*.5)
        parent_area=max(components[i]['area'] for i in neighbor_faces)
        item={'loop_id':loop['id'],'edges':loop['edge_count'],'bounds':loop['bounds'],'cap_area':area,'parent_area':parent_area,'area_ratio':area/max(parent_area,1e-12),'plane_error':plane_error}
        candidates.append(item)
        if 1e-9<area and area/parent_area<.08 and plane_error<.003 and loop['edge_count']<=20:caps.append(item)
    plan={'name':name,'fingerprint':report['fingerprint'],'zero_area_face_ids':zero,'cap_candidates':caps,'all_small_loops':candidates}
    plans.append(plan)
    print(json.dumps({'name':name,'zero':zero,'caps':[x['loop_id'] for x in caps],'added_triangles':sum(x['edges']-2 for x in caps)}))
(EVIDENCE/'repair_selected_plan.json').write_text(json.dumps(plans,indent=2),encoding='utf-8')
