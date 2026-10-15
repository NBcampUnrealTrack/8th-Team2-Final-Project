"""Independent GLB content/scale audit; extract embedded image bytes without editing."""
import json,hashlib,math
from pathlib import Path
import numpy as np
from inspect_glb import inspect,read_glb,accessor
from textured_handoff import DEST,save

def main():
    rows=json.loads((DEST/'export_manifest.json').read_text(encoding='utf-8'));output=[]
    for row in rows:
        p=Path(row['glb_path']);assert hashlib.sha256(p.read_bytes()).hexdigest()==row['sha256']
        g,b=read_glb(p);info=inspect(p)
        assert len(g['meshes'])==1 and len(g['materials'])==1 and not g.get('animations')
        assert info['total_triangles']==row['output_report']['geometry']['triangles']
        target=row['dimensions_cm'];assert np.allclose(np.array(info['dimensions_m_xyz_y_up'])*100,[target[0],target[2],target[1]],atol=.001)
        images=[]
        for i,im in enumerate(g.get('images',[])):
            view=g['bufferViews'][im['bufferView']];start=view.get('byteOffset',0);raw=b[start:start+view['byteLength']]
            ext={'image/jpeg':'.jpg','image/png':'.png'}[im['mimeType']]
            dest=DEST/'SourceTextures'/row['id']/(im.get('name','Image'+str(i))+ext);dest.parent.mkdir(parents=True,exist_ok=True)
            with dest.open('xb') as f:f.write(raw)
            images.append({'path':str(dest),'sha256':hashlib.sha256(raw).hexdigest(),'bytes':len(raw)})
        assert images
        for mesh in g['meshes']:
            for primitive in mesh['primitives']:
                for name,index in primitive['attributes'].items():assert np.isfinite(accessor(g,b,index)).all()
        output.append({'id':row['id'],'glb':info,'images':images,'texture_preserved_as_embedded_bytes':True,'geometry_and_scale_passed':True})
    save('glb_content_audit.json',output)
    print(json.dumps([{'id':r['id'],'triangles':r['glb']['total_triangles'],'render_vertices':r['glb']['total_render_vertices'],'image_count':len(r['images']),'passed':True} for r in output],indent=2))

if __name__=='__main__':main()
