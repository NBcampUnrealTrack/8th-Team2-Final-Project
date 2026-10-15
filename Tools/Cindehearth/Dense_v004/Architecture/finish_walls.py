"""Typed solidify + UV projection on isolated inspected wall repair only."""
from task import ROOT,EX,read,write,repair,blender,export
import sys
name=sys.argv[1]+'Walls';src=read(ROOT/'verification'/(name+'_source.json'))
glb=EX/(src['object']+'_ForUV.glb')
repair(name,'ForUVExport014','blender_repair_export',dict(blend_path=src['blend'],object_name=src['object'],expected_fingerprint=src['fingerprint'],output_path=str(glb)))
r=blender(name,'Import014','blender_model_import',dict(path=str(glb),name_prefix='CH_Dense_'+name+'_UVREF__'))['mesh_reports'][0]
obj='CH_Dense_'+name+'_Solid014'
d=blender(name,'Solid014','blender_modifier_apply',dict(source_object_name=r['object']['name'],output_object_name=obj,modifier=dict(type='solidify',thickness=.035,offset=0)))
obj2='CH_Dense_'+name+'_UV015'
blender(name,'UV015','blender_uv_edit',dict(source_object_name=obj,output_object_name=obj2,operation='cube_project',cube_size=.55))
final='CH_Dense_'+name+'_Final016'
blender(name,'Material016','blender_material_assign',dict(source_object_name=obj2,output_object_name=final,material_name='CH_Dense_'+name+'_Stone',create_material=True,base_color=[.20,.18,.15,1],metallic=0,roughness=.96))
path=EX/(final+'.blend')
blender(name,'Save016','blender_save_copy',dict(path=str(path),overwrite=False))
d=repair(name,'FinalInspect016','blender_topology_inspect',dict(blend_path=str(path),object_name=final))
r=d['report'];write(ROOT/'verification'/(name+'_source.json'),dict(blend=str(path),object=final,fingerprint=r['fingerprint'],report=r))
write(ROOT/'verification'/(name+'_current_topology.json'),d)
export(name)
print(name,r['geometry'],r['bounds'],flush=True)
