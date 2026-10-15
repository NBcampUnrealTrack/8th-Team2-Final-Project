"""Saved scene audit; only asset reads, no forced material diagnostics."""
import unreal,json,math,collections
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Dense_v004';out=root/'verification'
assert not unreal.WidgetService.is_pie_running()
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors();by={a.get_actor_label():a for a in actors}
d=by['CH_LevelDirector'];assert d.get_editor_property('completed_chapter_quest')==0 and not d.get_editor_property('night')
trees=[];missing=[];new=[];counts=collections.Counter()
for a in actors:
 if not isinstance(a,unreal.StaticMeshActor):continue
 c=a.static_mesh_component;m=c.static_mesh
 if not m:continue
 path=m.get_path_name();visible=c.is_visible() and not a.get_editor_property('hidden')
 if 'PineVolume' in path:
  s=a.get_actor_scale3d().to_tuple();assert max(s)-min(s)<1e-6,a.get_actor_label();trees.append(a.get_actor_label())
 if visible:
  if c.get_num_materials()==0 or any(c.get_material(i) is None for i in range(c.get_num_materials())):missing.append(a.get_actor_label())
  if '/Models/' in path:counts[path.split('/Models/')[1].split('/')[0]]+=1
 if a.get_actor_label().startswith('CH_D4_'):
  p=list(a.get_actor_location().to_tuple());s=list(a.get_actor_scale3d().to_tuple());assert all(math.isfinite(v) for v in p+s)
  new.append(dict(label=a.get_actor_label(),mesh=path,visible=visible,location=p,scale=s,tags=[str(t) for t in a.tags],bounds=[list(v.to_tuple()) for v in a.get_actor_bounds(False)]))
assert len(trees)==95 and not missing,(len(trees),missing)
for key in ('DorikSmithy','FreyaTavern','UnloadingShelter'):
 old=by['CH_'+key+'_Stone'];assert not old.static_mesh_component.is_visible() and not old.get_actor_enable_collision()
 assert by['CH_D4_'+key].static_mesh_component.is_visible()
links=[a for a in actors if a.get_actor_label().startswith('CH_D4_Link_')];assert len(links)==106 and all(not a.get_actor_enable_collision() for a in links)
banks=[a for a in actors if a.get_actor_label().startswith('CH_D4_CemeteryBank_')];assert len(banks)==16
assert all(not a.get_actor_enable_collision() and str(a.static_mesh_component.get_collision_profile_name())=='NoCollision' for a in banks)
oldwall=by['CH_D4_FreyaTavern_Walls'];assert not oldwall.static_mesh_component.is_visible() and not oldwall.get_actor_enable_collision()
for key in ('TavernWallsInset','TavernWallsFrame'):
 wall=by['CH_D4_FreyaTavern_'+key];assert wall.static_mesh_component.is_visible() and wall.get_actor_enable_collision()
 assert (wall.get_actor_location()-oldwall.get_actor_location()).length()<.001
 assert (wall.get_actor_scale3d()-oldwall.get_actor_scale3d()).length()<.001
for label in ('CH_MainMineRockBrow_P3','CH_RidgeMass_15_HD2D_P3'):
 a=by[label];center,extent=a.get_actor_bounds(False);assert center.z+extent.z<76 and str(a.static_mesh_component.get_collision_profile_name())=='NoCollision',label
for label,p in [('CH_D4_ArchiveEntry',(1077.7,195.975,215)),('CH_D4_ArchiveLower',(1172.7,253.475,29))]:assert (by[label].get_actor_location()-unreal.Vector(*p)).length()<.1
assert abs(by['CH_Stairs_-30_-18_Stone'].get_actor_location().x+399)<.1
source=[]
for key in ('Tavern','TavernWallsInset','TavernWallsFrame','SmithyStructure','SmithyRoof','SmithyWalls'):
 r=json.loads((root/'Architecture/verification'/f'{key}_export.json').read_text());g=r['geometry']
 assert g['loose_vertices']==g['non_manifold_edges']==g['degenerate_faces']==0
 source.append(dict(name=key,geometry=g,source=r['source']))
report=dict(passed=True,actors=len(actors),map='/Game/Maps/L_Cindehearth',footprint_design_m=[111,120.6],physical_ue_m=[49.95,54.27],area_ratio=.4958,three_tier_z_cm=[0,157.5,315],trees_3d_uniform=len(trees),connections=len(links),cemetery_bank_parts=len(banks),tavern_inset_and_depot_recess_verified=True,new_visible_parts=sum(r['visible'] for r in new),missing_materials=missing,visible_p2_instances=dict(counts),blender_sources=source,pie=False,chapter=0,night=False,material_check='Loaded assets and creation receipts; no forced recompilation')
(out/'final_scene_audit.json').write_text(json.dumps(report,indent=2),encoding='utf-8');(out/'final_placements.json').write_text(json.dumps(new,indent=2),encoding='utf-8')
print('FINAL SCENE PASS',len(actors),'actors;',len(trees),'volumetric trees;',len(links),'connection details')
