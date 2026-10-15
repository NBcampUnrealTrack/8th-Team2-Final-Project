"""Explicit inspection-selected edits through bounded Blender MCP only."""
import sys,json
from task import ROOT,EX,read,write,repair
name,stage,operation=sys.argv[1:4]
src=read(ROOT/'verification'/(name+'_source.json'))
ids=[int(x) for x in sys.argv[4].split(',')] if len(sys.argv)>4 and sys.argv[4] else []
obj='CH_Polish_'+name+'_'+stage
d=repair(name,stage,'blender_repair_mesh_copy',dict(blend_path=src['blend'],object_name=src['object'],expected_fingerprint=src['fingerprint'],output_object_name=obj,output_blend=str(EX/(obj+'.blend')),operation=operation,selection_ids=ids,distance=.00001,max_faces=1500,max_diameter=4.5))
report=d['output'];write(ROOT/'verification'/(name+'_source.json'),dict(blend=d['output_blend'],object=obj,fingerprint=report['fingerprint'],report=report))
print(name,stage,report['geometry'],flush=True)
d=repair(name,stage+'_inspect','blender_topology_inspect',dict(blend_path=d['output_blend'],object_name=obj))
write(ROOT/'verification'/(name+'_current_topology.json'),d)
print('loops',[(x['id'],x['edge_count'],x['simple_closed'],round(x['bounds']['diameter'],4)) for x in d['topology']['boundary_loops']],flush=True)
