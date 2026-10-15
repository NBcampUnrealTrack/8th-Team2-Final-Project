"""Split exact reviewed wall repairs for separate stone material assignment."""
from task import ROOT,EX,read,write,repair
import sys
name=sys.argv[1]
src=read(ROOT/'verification'/(name+'_source.json'))
write(ROOT/'verification'/(name+'_unified_repaired.json'),src)
faces={'Tavern':[1899,1900,1901,1902],'Smithy':[1905,1906]}[name]
assert src['report']['geometry']['polygons']==max(faces)+1
for suffix,ids in [('Body',faces),('Walls',[i for i in range(src['report']['geometry']['polygons']) if i not in faces])]:
    obj='CH_Dense_'+name+'_'+suffix+'013'
    d=repair(name,'Split'+suffix+'013','blender_repair_mesh_copy',dict(blend_path=src['blend'],object_name=src['object'],expected_fingerprint=src['fingerprint'],output_object_name=obj,output_blend=str(EX/(obj+'.blend')),operation='delete_faces',selection_ids=ids,distance=.00001,max_faces=3000,max_diameter=4.5))
    r=d['output'];key=name if suffix=='Body' else name+'Walls'
    write(ROOT/'verification'/(key+'_source.json'),dict(blend=d['output_blend'],object=obj,fingerprint=r['fingerprint'],report=r))
    print(key,r['geometry'],flush=True)
