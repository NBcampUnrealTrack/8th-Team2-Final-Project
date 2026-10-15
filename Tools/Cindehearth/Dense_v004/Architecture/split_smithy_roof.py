"""Separate inspected upper roof assembly; preserves vertex coordinates."""
from task import ROOT,EX,read,write,repair,export
src=read(ROOT/'verification/Smithy_source.json')
d=repair('Smithy','RoofSelectionInspect017','blender_topology_inspect',dict(blend_path=src['blend'],object_name=src['object']))
t=d['topology'];assert src['fingerprint']==d['report']['fingerprint']
# Inspected profile: the continuous eave assembly begins at Z=1.15m;
# every selected face is wholly above it, including gables, straps and chimney.
roof=[i for i,f in enumerate(t['faces']) if min(t['vertices'][j][2] for j in f)>=1.15]
write(ROOT/'verification/Smithy_roof_selection.json',dict(source=src,roof_faces=roof,threshold_z=1.15,roof_count=len(roof),body_count=len(t['faces'])-len(roof)))
assert 300<len(roof)<1100
for key,ids in [('SmithyStructure',roof),('SmithyRoof',[i for i in range(len(t['faces'])) if i not in roof])]:
    obj='CH_Dense_'+key+'_Final018'
    result=repair(key,'Split018','blender_repair_mesh_copy',dict(blend_path=src['blend'],object_name=src['object'],expected_fingerprint=src['fingerprint'],output_object_name=obj,output_blend=str(EX/(obj+'.blend')),operation='delete_faces',selection_ids=ids,distance=.00001,max_faces=3000,max_diameter=4.5))
    r=result['output'];write(ROOT/'verification'/(key+'_source.json'),dict(blend=result['output_blend'],object=obj,fingerprint=r['fingerprint'],report=r))
    export(key)
    print(key,r['geometry'],r['bounds'],flush=True)
