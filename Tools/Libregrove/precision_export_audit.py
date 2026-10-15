"""Independent GLB byte-level UV/unit/render-vertex audit (outside Blender)."""
import json,sys
import numpy as np
from precision_workflow import DOC
from inspect_glb import read_glb,accessor,inspect
from precision_uv import audit_triangles

def audit(path):
    h=json.loads(path.read_text(encoding='utf-8'));name=h['id'];g,b=read_glb(h['exports']['glb']);triangles=[]
    for mesh in g['meshes']:
        for primitive in mesh['primitives']:
            uv=accessor(g,b,primitive['attributes']['TEXCOORD_0']);index=accessor(g,b,primitive['indices']).reshape(-1,3);triangles.extend(uv[index].tolist())
    q=audit_triangles(triangles,list(range(len(triangles))));export=inspect(h['exports']['glb'])
    width,depth,height=h['expected_width_depth_height_cm'];expected=[depth/100,height/100,width/100]
    error=max(abs(a-b) for a,b in zip(expected,export['dimensions_m_xyz_y_up']));assert error<1e-5,(name,error)
    result={'id':name,'export':export,'uv':q,'units_match_expected':True,'max_dimension_error_m':error}
    suffix='_FINAL' if path.stem.endswith('_FINAL') else ''
    dest=DOC/(name+suffix+'_GLB_QA.json');assert not dest.exists();dest.write_text(json.dumps(result,indent=2),encoding='utf-8')
    (DOC/'uv_layouts').mkdir(exist_ok=True)
    svg=['<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1024 1024"><rect width="1024" height="1024" fill="#17252a"/><g fill="#91bda6" stroke="#e4efdf" stroke-width=".22">']
    for t in triangles:svg.append('<polygon points="'+' '.join(f'{p[0]*1024:.4f},{p[1]*1024:.4f}' for p in t)+'"/>')
    svg.append('</g></svg>');(DOC/'uv_layouts'/(name+suffix+'_UV.svg')).write_text('\n'.join(svg),encoding='utf-8')
    print(json.dumps({'id':name,'uv_passed':q['passed'],'overlaps':q['positive_overlap_pairs'],'degenerate':q['degenerate_face_ids'],'area':q['area_sum'],'render_vertices':export['total_render_vertices'],'dimensions_passed':True}),flush=True)
    return q['passed']

if __name__=='__main__':
    final='final' in sys.argv
    if final:sys.argv.remove('final')
    paths=sorted(DOC.glob('LG_*_FINAL.json' if final else 'LG_*_DELIVERY.json'))
    if sys.argv[1:]:paths=[p for p in paths if int(p.name[3:5]) in [int(s) for s in sys.argv[1:]]]
    assert paths
    if not all([audit(p) for p in paths]):sys.exit(2)
