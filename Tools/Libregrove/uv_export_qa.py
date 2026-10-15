"""Exact triangle clipping audit of exported UVs, not an AABB-overlap estimate."""
import json,math,sys
from collections import defaultdict
from pathlib import Path
import numpy as np
from inspect_glb import read_glb,accessor,inspect
from blender_mcp_client import EVIDENCE,ROOT

def cross(a,b):return a[0]*b[1]-a[1]*b[0]
def poly_area(p):
    return abs(sum(cross(p[i],p[(i+1)%len(p)]) for i in range(len(p))))*.5 if len(p)>2 else 0
def clip(subject,clipper):
    orient=1 if cross(clipper[1]-clipper[0],clipper[2]-clipper[0])>=0 else -1
    polygon=[tuple(p) for p in subject]
    for k in range(3):
        a=clipper[k];b=clipper[(k+1)%3];edge=b-a
        def side(p):return orient*cross(edge,np.array(p)-a)
        out=[]
        for i,end in enumerate(polygon):
            start=polygon[i-1];ds=side(start);de=side(end)
            if (ds>=0)!=(de>=0):
                alpha=ds/(ds-de)
                out.append((start[0]+alpha*(end[0]-start[0]),start[1]+alpha*(end[1]-start[1])))
            if de>=0:out.append(end)
        polygon=out
        if len(polygon)<3:return 0.
    return poly_area(polygon)

def audit(handoff):
    path=handoff['exports']['glb'];model,binary=read_glb(path);triangles=[];areas3=[]
    for mesh in model['meshes']:
        for primitive in mesh['primitives']:
            attrs=primitive['attributes'];uv=accessor(model,binary,attrs['TEXCOORD_0']);pos=accessor(model,binary,attrs['POSITION'])
            index=accessor(model,binary,primitive['indices']).reshape(-1,3)
            triangles.extend(uv[index].tolist())
            pts=pos[index];areas3.extend((np.linalg.norm(np.cross(pts[:,1]-pts[:,0],pts[:,2]-pts[:,0]),axis=1)*.5).tolist())
    tris=np.array(triangles);low=tris.min(axis=1);high=tris.max(axis=1)
    area=np.abs((tris[:,1,0]-tris[:,0,0])*(tris[:,2,1]-tris[:,0,1])-(tris[:,1,1]-tris[:,0,1])*(tris[:,2,0]-tris[:,0,0]))*.5
    grid=defaultdict(list);resolution=64;pairs=set();hits=[];overlap_area=0
    for i,(lo,hi) in enumerate(zip(low,high)):
        a=np.maximum(0,np.minimum(resolution-1,np.floor(lo*resolution).astype(int)));b=np.maximum(0,np.minimum(resolution-1,np.floor(hi*resolution).astype(int)))
        for x in range(a[0],b[0]+1):
            for y in range(a[1],b[1]+1):
                key=(x,y)
                for j in grid[key]:
                    pair=(j,i)
                    if pair in pairs:continue
                    pairs.add(pair)
                    span=np.minimum(high[i],high[j])-np.maximum(low[i],low[j])
                    if min(span)<=1e-10:continue
                    overlap=clip(tris[i],tris[j])
                    # Suppress float32/shared-edge numerical slivers.
                    if overlap>max(1e-11,min(area[i],area[j])*1e-5):
                        hits.append({'triangles':[j,i],'area':overlap});overlap_area+=overlap
                grid[key].append(i)
    glb_report=inspect(path)
    density=1024*np.sqrt(np.divide(area,np.maximum(areas3,1e-20)))/100
    result={'id':handoff['id'],'export':glb_report,'triangle_count':len(tris),'nonfinite_values':int((~np.isfinite(tris)).sum()),'outside_unit_square_vertices':int(((tris< -1e-6)|(tris>1.000001)).any(axis=2).sum()),'zero_area_uv_triangles_1e_12':int((area<=1e-12).sum()),'tested_pairs':len(pairs),'positive_overlap_pair_count':len(hits),'sum_pair_overlap_area':overlap_area,'uv_triangle_area_sum':float(area.sum()),'density_1024_px_per_cm_median':float(np.median(density)),'density_1024_px_per_cm_p10_p90':[float(x) for x in np.quantile(density,[.1,.9])],'overlap_details':hits}
    out=ROOT/'Docs/Libregrove/BlenderQA_20260908/uv_layouts';out.mkdir(parents=True,exist_ok=True)
    lines=['<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1024 1024"><rect width="1024" height="1024" fill="#162124"/><g fill="#97bfbc" fill-opacity=".65" stroke="#edf6ed" stroke-width=".35">']
    for tri in tris:
        pts=' '.join(f'{p[0]*1024:.3f},{p[1]*1024:.3f}' for p in tri)
        lines.append(f'<polygon points="{pts}"/>')
    lines.append('</g></svg>')
    (out/(handoff['id']+'.svg')).write_text('\n'.join(lines),encoding='utf-8')
    (EVIDENCE/('export_qa_'+handoff['id']+'.json')).write_text(json.dumps(result,indent=2),encoding='utf-8')
    print(json.dumps({k:v for k,v in result.items() if k not in ('export','overlap_details')}) ,flush=True)

if __name__=='__main__':
    files=sorted(EVIDENCE.glob('final_handoff_*.json'))
    for file in files:
        h=json.loads(file.read_text(encoding='utf-8'))
        if sys.argv[1:] and h['id'] not in sys.argv[1:]:continue
        audit(h)
