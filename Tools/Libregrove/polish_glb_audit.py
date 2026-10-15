"""Independent byte-level GLB units, triangles, finite normals and UV checks."""
import json,sys
import numpy as np
from polish_v3 import ASSETS,DOC
from inspect_glb import read_glb,accessor,inspect
from precision_uv import audit_triangles

def audit(n):
    name=ASSETS[n]['id'];h=json.loads((DOC/(name+'_FINAL.json')).read_text(encoding='utf-8'));g,b=read_glb(h['exports']['glb']);triangles=[]
    for mesh in g['meshes']:
        for p in mesh['primitives']:
            for attr in ('POSITION','NORMAL','TEXCOORD_0'):assert np.isfinite(accessor(g,b,p['attributes'][attr])).all()
            uv=accessor(g,b,p['attributes']['TEXCOORD_0']);index=accessor(g,b,p['indices']).reshape(-1,3);triangles.extend(uv[index].tolist())
    q=audit_triangles(triangles,list(range(len(triangles))));export=inspect(h['exports']['glb'])
    w,d,z=h['expected_width_depth_height_cm'];error=max(abs(a-b) for a,b in zip(export['dimensions_m_xyz_y_up'],[d/100,z/100,w/100]));assert error<1e-5
    assert export['total_triangles']==h['geometry']['triangles']
    assert q['passed'],(name,q)
    result={'id':name,'export':export,'uv':q,'triangle_count_matches':True,'units_match_expected':True,'max_dimension_error_m':error,'finite_positions_normals_uv':True}
    dest=DOC/(name+'_FINAL_GLB_QA.json');assert not dest.exists();dest.write_text(json.dumps(result,indent=2),encoding='utf-8')
    (DOC/'uv_layouts').mkdir(exist_ok=True)
    svg=['<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 2048 2048"><g fill="none" stroke="#222" stroke-width=".65">']
    for t in triangles:svg.append('<polygon points="'+' '.join(f'{p[0]*2048:.5f},{p[1]*2048:.5f}' for p in t)+'"/>')
    svg.append('</g></svg>');(DOC/'uv_layouts'/(name+'_UV.svg')).write_text('\n'.join(svg),encoding='utf-8')
    print(json.dumps({'id':name,'glb_passed':True,'render_vertices':export['total_render_vertices'],'uv_area_percent':q['area_sum']*100}),flush=True)

if __name__=='__main__':
    for n in sys.argv[1:]:audit(int(n))
