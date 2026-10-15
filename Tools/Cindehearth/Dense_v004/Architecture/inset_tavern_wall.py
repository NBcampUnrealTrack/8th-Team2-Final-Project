"""Fix door occlusion by translating only the inspected front repair solid.

Only existing typed Blender MCP operations are called. No input file is changed.
"""
from task import ROOT,EX,read,write,repair,blender,export

src=read(ROOT/'verification/TavernWalls_source.json')
d=repair('TavernWalls','InsetBaseline025','blender_topology_inspect',dict(blend_path=src['blend'],object_name=src['object']))
assert d['report']['fingerprint']==src['fingerprint']
t=d['topology'];assert len(t['components'])==4
front=t['components'][0]
assert front['face_ids']==[0,1,8,9,16,17,18,19]
assert front['bounds']['min'][0]>.59
for key,remove in [('TavernWallsFront',[1,2,3]),('TavernWallsFrame',[0])]:
    obj='CH_Dense_'+key+'_025'
    result=repair(key,'Split025','blender_repair_mesh_copy',dict(blend_path=src['blend'],object_name=src['object'],expected_fingerprint=src['fingerprint'],output_object_name=obj,output_blend=str(EX/(obj+'.blend')),operation='delete_components',selection_ids=remove,distance=.00001,max_faces=100,max_diameter=4.5))
    r=result['output'];write(ROOT/'verification'/(key+'_source.json'),dict(blend=result['output_blend'],object=obj,fingerprint=r['fingerprint'],report=r))
    export(key)

front=read(ROOT/'verification/TavernWallsFront_source.json')
file=read(ROOT/'verification/TavernWallsFront_export.json')['source']
baseline=blender('TavernWallsInset','Import026','blender_model_import',dict(path=file,name_prefix='CH_Dense_TavernWallsInset_REF__'))['mesh_reports'][0]
obj='CH_Dense_TavernWallsInset_Final026'
blender('TavernWallsInset','Translate026','blender_mesh_edit',dict(source_object_name=baseline['object']['name'],output_object_name=obj,operations=[dict(type='transform_vertices',translation=[-.14,0,0])]))
path=EX/(obj+'.blend')
blender('TavernWallsInset','Save026','blender_save_copy',dict(path=str(path),overwrite=False))
d=repair('TavernWallsInset','FinalInspect026','blender_topology_inspect',dict(blend_path=str(path),object_name=obj))
r=d['report']
# GLB round-trip split the eight geometric corners by UV/normal seams. The
# inspected 24 vertices occupy exactly eight locations; weld only these exact
# coincidences on another preserved copy, retaining every UV loop.
positions={tuple(round(v,6) for v in p) for p in d['topology']['vertices']}
assert len(positions)==8 and len(d['topology']['vertices'])==24
fixed='CH_Dense_TavernWallsInset_Weld027'
result=repair('TavernWallsInset','Weld027','blender_repair_mesh_copy',dict(blend_path=str(path),object_name=obj,expected_fingerprint=r['fingerprint'],output_object_name=fixed,output_blend=str(EX/(fixed+'.blend')),operation='weld',selection_ids=[],distance=.000001,max_faces=100,max_diameter=4.5))
path=EX/(fixed+'.blend');obj=fixed
d=repair('TavernWallsInset','FinalInspect027','blender_topology_inspect',dict(blend_path=str(path),object_name=obj))
r=d['report']
assert all(r['geometry'][k]==0 for k in ['boundary_edges','non_manifold_edges','loose_vertices','degenerate_faces','non_finite_vertices'])
assert abs(r['bounds']['local_max'][0]-(src['report']['bounds']['local_max'][0]-.14))<1e-6
write(ROOT/'verification/TavernWallsInset_source.json',dict(blend=str(path),object=obj,fingerprint=r['fingerprint'],report=r))
write(ROOT/'verification/TavernWallsInset_current_topology.json',d)
export('TavernWallsInset')
write(ROOT/'verification/TavernWallsInset_fix.json',dict(reason='Original front repair solid covered wood door and first floor window frames.',door_panel_x_m=.6381546258926392,lowest_window_frame_x_m=.5484,front_translation_m=[-.14,0,0],new_front_bounds=r['bounds'],
    replace_previous='TavernWalls',assembly=['TavernWallsFrame','TavernWallsInset'],copy_transform=True,source_other_three_walls_unchanged=True,
    note='Hide previous TavernWalls actor. Spawn Frame and Inset with the exact previous actor transform/material. Do not fit individual bounds. Main Tavern body remains unchanged.'))
print('DONE TavernWallsFrame + TavernWallsInset',r['geometry'],r['bounds'],flush=True)
