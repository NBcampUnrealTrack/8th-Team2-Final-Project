import unreal,json,hashlib
from pathlib import Path
R=Path(unreal.Paths.project_dir()).resolve();D=R/'Docs/Fernbrook_CharacterLighting_20260911'
W=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);assert not W.get_game_world()
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
base=json.loads((R/'Docs/Fernbrook_TreePolish_V6_20260911/baseline.json').read_text())
before={r['label']:r for r in base['actors']};assert len(actors)==len(before)==241
for a in actors:
 b=before[a.get_actor_label()]
 for old,new in [(b['loc'],a.get_actor_location().to_tuple()),(b['rot'],a.get_actor_rotation().to_tuple()),(b['scale'],a.get_actor_scale3d().to_tuple())]:assert max(abs(x-y) for x,y in zip(old,new))<.00001
mi=unreal.EditorAssetLibrary.load_asset('/Game/Fernbrook/CharacterLighting/Materials/MI_FB_Player_Lit')
L=unreal.MaterialEditingLibrary
ambient=L.get_material_instance_scalar_parameter_value(mi,'AmbientLift')
t=L.get_material_instance_vector_parameter_value(mi,'Tint')
assert abs(ambient-.45)<.0001 and abs(t.r-1.3)<.0001 and t.r==t.g==t.b
mode=W.get_editor_world().get_world_settings().get_editor_property('default_game_mode')
assert mode.get_path_name()=='/Game/Fernbrook/CharacterLighting/Gameplay/BP_FB_GameMode.BP_FB_GameMode_C'
changed=[];baseline=json.loads((D/'baseline_hashes.json').read_text())
for row in baseline['files']:
 f=R/row['path']
 if not f.exists() or hashlib.sha256(f.read_bytes()).hexdigest()!=row['sha256']:changed.append(row['path'])
assert changed==['Content/Fernbrook/Entrance_V1/Maps/L_Fernbrook_Entrance.umap'],changed
newfiles=[str(f.relative_to(R)).replace('\\','/') for f in (R/'Content/Fernbrook/CharacterLighting').rglob('*.uasset')]
assert len(newfiles)==3
qa=json.loads((D/'movement_qa_002.json').read_text());assert qa['passed']
assert {'idle_front','walk_down'}=={q['flipbook'].split('.')[-1] for q in qa['samples']}
dirty={'content':len(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()),'maps':len(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())};assert dirty=={'content':0,'maps':0}
r={'map':W.get_editor_world().get_path_name(),'mode':mode.get_path_name(),'ambient_lift':ambient,'tint':[t.r,t.g,t.b,t.a],'unchanged_existing_assets':len(baseline['files'])-1,'modified_existing_assets':changed,'new_assets':newfiles,'actor_transforms_preserved':241,'movement_pass':qa['passed'],'movement_distance_cm':qa['distance_cm'],'dirty':dirty,'pie':'stopped','backup':baseline['map_backup']}
with (D/'final_signoff.json').open('x',encoding='utf8') as f:json.dump(r,f,indent=2)
print(json.dumps(r))
