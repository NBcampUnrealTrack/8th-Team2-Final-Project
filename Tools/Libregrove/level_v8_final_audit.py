"""Read saved scene state without forced shader diagnostics or material compilation."""
import unreal,json,hashlib
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Docs/Libregrove/Level_V8_20260908';RUN=globals().get('RUN','001')
assert not unreal.WidgetService.is_pie_running()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='L_Libregrove'
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors();A=unreal.EditorAssetLibrary;E=unreal.MaterialEditingLibrary
buildings=[a for a in actors if str(a.get_folder_path()).startswith('Libregrove/Buildings')];assert len(buildings)==34,len(buildings)
slots=[];foliage=[];sources={};base='/Game/Libregrove/Level_V8/HD2D/'
for a in actors:
 for c in a.get_components_by_class(unreal.StaticMeshComponent):
  if not c.static_mesh:continue
  for i in range(c.get_num_materials()):
   m=c.get_material(i);assert m,(a.get_actor_label(),i)
   assert m.get_path_name().startswith(base),(a.get_actor_label(),m.get_path_name())
   slots.append({'actor':a.get_actor_label(),'component':c.get_name(),'slot':i,'material':m.get_path_name()})
  if isinstance(c,unreal.InstancedStaticMeshComponent):foliage.append({'component':c.get_name(),'mesh':c.static_mesh.get_path_name(),'count':c.get_instance_count()})
  if '/Textured_V7/' in c.static_mesh.get_path_name():sources[c.static_mesh.get_path_name()]=c.static_mesh
for path,mesh in sources.items():
 for sm in mesh.get_editor_property('static_materials'):assert '/Textured_V7/' in sm.material_interface.get_path_name(),path
land=next(a for a in actors if isinstance(a,unreal.Landscape));terrain=land.get_editor_property('landscape_material');assert terrain.get_path_name().endswith('MI_HD2D_Terrain_Stable.MI_HD2D_Terrain_Stable')
assert terrain.get_editor_property('parent').get_path_name().endswith('M_LG8_HD2D_Landscape_Stable.M_LG8_HD2D_Landscape_Stable')
ft=[]
for path in A.list_assets('/Game/Libregrove/Level_V8/Foliage',True,False):
 t=unreal.load_asset(path)
 if isinstance(t,unreal.FoliageType_InstancedStaticMesh):
  mats=[m.get_path_name() for m in t.get_editor_property('override_materials')];assert all(x.startswith(base) for x in mats) and mats
  ft.append({'path':path,'materials':mats})
assert len(ft)==4
mode=world.get_world_settings().get_editor_property('default_game_mode').get_path_name();assert 'BP_LG8_GameMode' in mode
graph,err=unreal.BlueprintService.describe_graph('/Game/Libregrove/Level_V8/Gameplay/BP_LG8_FieldCharacter','EventGraph');assert graph and not graph.status.has_compile_errors,err
pp=[]
for a in actors:
 if isinstance(a,unreal.PostProcessVolume):
  s=a.get_editor_property('settings');pp.append({'actor':a.get_actor_label(),'weight':a.get_editor_property('blend_weight'),'focal_distance':s.depth_of_field_focal_distance,'fstop':s.depth_of_field_fstop,'sensor_width':s.depth_of_field_sensor_width,'exposure_bias':s.auto_exposure_bias})
assert len(pp)==2
mpc=json.loads(unreal.MaterialParameterCollectionService.list_mpc_parameters(base+'MPC_LG8_HD2D'));default=json.loads((OUT/'HD2D/default_preset.json').read_text(encoding='utf8'))
for r in mpc['scalars']:assert abs(default['scalars'][r['name']]-r['default'])<.0001,r
oldmap_hash=hashlib.sha256((ROOT/'Content/Maps/L_VillageEntrance.umap').read_bytes()).hexdigest();assert oldmap_hash=='01625bb8d6468b7ab8ec4d6e4a69011aed75a43c5b00b3618f85e0785422464a'
routes=[]
for name,run in [('central_shrine','002'),('west_bridge_mill','005'),('east_bridge_workshop','002'),('lower_loop','001'),('west_exit','004'),('southeast_exit','001')]:
 q=json.loads((OUT/'GameplayQA'/(name+'_'+run+'.json')).read_text(encoding='utf8'));assert q['passed'];routes.append({'route':name,'run':run,'waypoints':len(q['reached'])})
result={'scene':world.get_path_name(),'actors':len(actors),'buildings':len(buildings),'material_slots':len(slots),'foliage':foliage,'foliage_total':sum(r['count'] for r in foliage),'foliage_types':ft,'source_mesh_default_materials_preserved':list(sources),'terrain':terrain.get_path_name(),'game_mode':mode,'camera_graph_compile_state':graph.status.compile_status,'camera_edges':len(graph.edges),'postprocess':pp,'mpc_defaults':mpc,'old_village_map_sha256':oldmap_hash,'routes':routes,'waypoints_total':sum(r['waypoints'] for r in routes),'pie_off':True,'no_forced_shader_diagnostics':True}
with (OUT/('final_audit_'+RUN+'.json')).open('x',encoding='utf8') as f:json.dump(result,f,indent=2)
print(json.dumps({k:v for k,v in result.items() if k not in ['mpc_defaults','source_mesh_default_materials_preserved','foliage_types','postprocess','foliage']}))
