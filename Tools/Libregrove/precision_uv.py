"""Deterministic UV triangle intersection diagnostics, also unit-testable outside Blender."""
from collections import defaultdict
import math
import numpy as np

def cross(a,b):return a[0]*b[1]-a[1]*b[0]

def intersection_area(subject,clipper):
    sign=1 if cross(clipper[1]-clipper[0],clipper[2]-clipper[0])>=0 else -1
    poly=[tuple(p) for p in subject]
    for k in range(3):
        a=clipper[k];edge=clipper[(k+1)%3]-a;out=[]
        def side(p):return sign*(edge[0]*(p[1]-a[1])-edge[1]*(p[0]-a[0]))
        for i,end in enumerate(poly):
            begin=poly[i-1];ds=side(begin);de=side(end)
            if (ds>=0)!=(de>=0):
                t=ds/(ds-de);out.append((begin[0]+t*(end[0]-begin[0]),begin[1]+t*(end[1]-begin[1])))
            if de>=0:out.append(end)
        poly=out
        if len(poly)<3:return 0.
    return abs(sum(cross(poly[i],poly[(i+1)%len(poly)]) for i in range(len(poly))))*.5

def audit_triangles(triangles,face_ids):
    t=np.asarray(triangles,dtype=np.float64)
    if not len(t):return {'present':False}
    if not np.isfinite(t).all():return {'present':True,'nonfinite':int((~np.isfinite(t)).sum()),'passed':False}
    low=t.min(axis=1);high=t.max(axis=1)
    area=np.abs((t[:,1,0]-t[:,0,0])*(t[:,2,1]-t[:,0,1])-(t[:,1,1]-t[:,0,1])*(t[:,2,0]-t[:,0,0]))*.5
    grid=defaultdict(list);tested=set();hits=[]
    for i,(lo,hi) in enumerate(zip(low,high)):
        start=np.clip(np.floor(lo*64).astype(int),0,63);end=np.clip(np.floor(hi*64).astype(int),0,63)
        for x in range(start[0],end[0]+1):
            for y in range(start[1],end[1]+1):
                for j in grid[x,y]:
                    if (j,i) in tested:continue
                    tested.add((j,i))
                    if min(np.minimum(high[i],high[j])-np.maximum(low[i],low[j]))<=1e-10:continue
                    overlap=intersection_area(t[i],t[j])
                    if overlap>max(1e-11,min(area[i],area[j])*1e-5):hits.append({'triangles':[j,i],'face_ids':[int(face_ids[j]),int(face_ids[i])],'area':overlap})
                grid[x,y].append(i)
    degenerate=[int(face_ids[i]) for i in np.flatnonzero(area<=1e-12)]
    return {'present':True,'nonfinite':0,'triangle_count':len(t),'positive_overlap_pairs':len(hits),'overlap_details':hits,'degenerate_face_ids':sorted(set(degenerate)),'outside_unit_vertices':int(((t< -1e-6)|(t>1.000001)).any(axis=2).sum()),'area_sum':float(area.sum()),'bounds':[low.min(axis=0).tolist(),high.max(axis=0).tolist()],'passed':not hits and not degenerate and bool(((t>=-1e-6)&(t<=1.000001)).all())}

def mesh_uv_audit(mesh,include_overlap=True):
    if not mesh.uv_layers.active:return {'present':False,'passed':False}
    mesh.calc_loop_triangles();uv=mesh.uv_layers.active.data
    triangles=[[list(uv[li].uv) for li in tri.loops] for tri in mesh.loop_triangles]
    faces=[tri.polygon_index for tri in mesh.loop_triangles]
    if include_overlap:return audit_triangles(triangles,faces)
    return {'present':True,'triangle_count':len(triangles),'overlap_not_tested':True}

def export_precision_risks(mesh):
    """Fixed read-only float32 V-flip sliver audit; stable Blender polygon IDs."""
    if not mesh.uv_layers.active:return {'present':False}
    mesh.calc_loop_triangles();uv=mesh.uv_layers.active.data;items=[]
    for tri in mesh.loop_triangles:
        raw=np.array([list(uv[i].uv) for i in tri.loops],dtype=np.float32)
        flipped=raw.copy();flipped[:,1]=np.float32(1)-flipped[:,1]
        def area(t):
            t=t.astype(np.float64);return abs(cross(t[1]-t[0],t[2]-t[0]))*.5
        a=area(raw);b=area(flipped)
        if min(a,b)<=1e-10:items.append({'face_id':tri.polygon_index,'uv_area':a,'glb_float32_uv_area':b,'geometry_area_m2':tri.area})
    return {'present':True,'threshold_area':1e-10,'risk_faces':items}

def self_test():
    a=[[0,0],[.4,0],[0,.4]];b=[[.1,.1],[.4,.1],[.1,.4]];c=[[.6,.6],[1,.6],[.6,1]]
    q=audit_triangles([a,b,c],[4,5,6]);assert q['positive_overlap_pairs']==1 and q['overlap_details'][0]['face_ids']==[4,5]
    q=audit_triangles([a,[[.4,0],[.4,.4],[0,.4]]],[0,1]);assert q['positive_overlap_pairs']==0 and q['passed']
    q=audit_triangles([[[0,0],[0,0],[0,0]]],[7]);assert q['degenerate_face_ids']==[7]
    return {'uv_intersection_unit_tests':3}
