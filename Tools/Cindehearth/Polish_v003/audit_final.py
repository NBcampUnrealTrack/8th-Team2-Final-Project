"""Read-only saved scene/asset verification. Never force material recompilation."""
import unreal,json,collections
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Polish_v003';out=root/'verification'
assert not unreal.WidgetService.is_pie_running()
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors();d=next(a for a in actors if isinstance(a,unreal.NUCindehearthDirector))
assert d.get_editor_property('completed_chapter_quest')==0 and not d.get_editor_property('night')
counts=collections.Counter();new=[];nulls=[];old_props=[];trees=[];structural=[]
for a in actors:
 if not isinstance(a,unreal.StaticMeshActor):continue
 c=a.static_mesh_component;m=c.static_mesh
 if not m:continue
 path=m.get_path_name();visible=c.is_visible() and not a.get_editor_property('hidden')
 if 'PineVolume' in path:
  s=a.get_actor_scale3d().to_tuple();assert max(s)-min(s)<1e-6;trees.append(a.get_actor_label())
 if visible and ('/SM_Crate.' in path or '/SM_Barrel.' in path):old_props.append(a.get_actor_label())
 if visible and a.actor_has_tag('CH.Polish_v003'):
  if c.get_num_materials()==0 or any(c.get_material(i) is None for i in range(c.get_num_materials())):nulls.append(a.get_actor_label())
  if '/Polish_v003/Models/' in path:
   key=path.split('/Models/')[1].split('/')[0];counts[key]+=1
  elif '/Models/Rock/' in path:counts['Rock_v002_reused']+=1
  else:structural.append(a.get_actor_label())
  new.append(dict(label=a.get_actor_label(),mesh=path,tags=[str(t) for t in a.tags],location=list(a.get_actor_location().to_tuple())))
assert len(trees)==95 and not nulls and not old_props,(len(trees),nulls,old_props)
expected={x['name'] for x in json.loads((root/'catalog.json').read_text())}-{'RingPile','Shelf'}|{'ShelfOpen'}
assert expected<=set(counts),(expected-set(counts))
for key in expected:
 r=json.loads((out/('import_'+key+'.json')).read_text());assert r['status']=='finished'
 m=unreal.load_asset(r['meshes'][0]['path']);assert m
 assert not unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).get_nanite_settings(m).enabled
 mi=unreal.load_asset('/Game/Cindehearth/Polish_v003/Materials/MI_CH_'+key);assert mi
assert not any(a.get_actor_label().startswith('CH_TEMP_') for a in actors)
report=dict(passed=True,map='/Game/Maps/L_Cindehearth',actors=len(actors),visible_polish_mesh_actors=len(new),instances=dict(counts),accepted_p2_bases=len(expected),trees_3d_uniform=len(trees),old_crate_barrel_visible=old_props,missing_materials=nulls,structural_ground=structural,chapter=0,night=False,pie=False,compile_check='Persisted asset load and creation receipts; no hot forced recompilation')
(out/'final_scene_audit.json').write_text(json.dumps(report,indent=2),encoding='utf-8');(out/'final_placements.json').write_text(json.dumps(new,indent=2),encoding='utf-8')
print(json.dumps(report))
