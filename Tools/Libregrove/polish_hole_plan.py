"""Select small planar interior boundary loops; exclude existing coplanar faces."""
import json
import numpy as np
from polish_v3 import ASSETS,DOC,record

plans=[]
for n in range(1,10):
    name=ASSETS[n]['id'];r=record(f'V3_{name}_BASE_INSPECT');t=r['topology'];v=np.array(t['vertices']);f=t['faces'];centers=np.array([v[face].mean(axis=0) for face in f]);normals=np.array(t['face_normals']);edges=t['edges'];results=[]
    for loop in t['boundary_loops']:
        if not loop['simple_closed'] or not 3<=loop['edge_count']<=24 or not .015<=loop['bounds']['diameter']<=.38:continue
        links={i:[] for i in loop['vertex_ids']}
        for eid in loop['edge_ids']:
            a,b=edges[eid]['vertices'];links[a].append(b);links[b].append(a)
        ordered=[min(links)];prev=None;cur=ordered[0]
        while True:
            nxt=next(i for i in links[cur] if i!=prev)
            if nxt==ordered[0]:break
            ordered.append(nxt);prev,cur=cur,nxt
            assert len(ordered)<=len(links)
        pts=v[ordered];center=pts.mean(axis=0);_,_,basis=np.linalg.svd(pts-center,full_matrices=False);normal=basis[-1];plane_error=np.abs((pts-center)@normal).max()
        if plane_error>.003:continue
        p=(pts-center)@basis[:2].T;area=abs(np.sum(p[:,0]*np.roll(p[:,1],-1)-p[:,1]*np.roll(p[:,0],-1)))*.5
        if not .00002<area<.055:continue
        # If the loop encloses an existing coplanar triangle, it is an outer
        # border of a sheet, not an empty hole. Do not duplicate that face.
        candidates=np.flatnonzero((np.abs((centers-center)@normal)<.00003)&(np.abs(normals@normal)>.9)&(np.linalg.norm(centers-center,axis=1)<loop['bounds']['diameter']))
        def inside(point):
            hit=False
            for a,b in zip(p,np.roll(p,-1,axis=0)):
                if (a[1]>point[1])!=(b[1]>point[1]) and point[0]<(b[0]-a[0])*(point[1]-a[1])/(b[1]-a[1])+a[0]:hit=not hit
            return hit
        if any(inside((centers[i]-center)@basis[:2].T) for i in candidates):continue
        neighbor_ids={i for eid in loop['edge_ids'] for i in edges[eid]['face_ids']}
        avg=np.mean(normals[list(neighbor_ids)],axis=0)
        if np.linalg.norm(avg)<.2 or abs(avg@normal)/np.linalg.norm(avg)<.6:continue
        screen={};bound_center=(v.min(axis=0)+v.max(axis=0))/2;scale=np.ptp(v,axis=0).max()*1.32
        for view,d in {'front_right':[1,-1,.68],'back_left':[-1,1,.68]}.items():
            d=np.array(d);d/=np.linalg.norm(d);right=np.cross(-d,[0,0,1]);right/=np.linalg.norm(right);up=np.cross(right,-d)
            screen[view]=[round(384+(center-bound_center)@right/scale*768,1),round(384-(center-bound_center)@up/scale*768,1)]
        results.append({'loop_id':loop['id'],'vertex_ids':ordered,'center':center.tolist(),'diameter':loop['bounds']['diameter'],'plane_error':float(plane_error),'area':float(area),'screens':screen})
    out={'id':name,'loops':results};plans.append(out);print(json.dumps({'id':name,'count':len(results),'large':sorted(results,key=lambda x:-x['area'])[:5]}))
dest=DOC/'small_hole_plan.json';assert not dest.exists();dest.write_text(json.dumps(plans,indent=2),encoding='utf-8')
