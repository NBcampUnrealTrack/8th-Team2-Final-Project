"""Apply the visually reviewed v006 repair; preserve the previous actor state."""
import unreal
import json
import shutil
from pathlib import Path

root = Path(unreal.Paths.project_dir())
evidence = root / 'Tools/Cindehearth/Pipeline/verification'
base = '/Game/Cindehearth/Tripo/GormHouse_P2_Repair_v006'
asset = base + '/CH_GormP2_Clean_v006'
mesh = unreal.load_asset(asset + '/StaticMeshes/CH_GormP2_Clean_v006')
old_base = '/Game/Cindehearth/Tripo/GormHouse_P2_v001'
body_material = unreal.load_asset(old_base + '/MI_CH_GormP2_Pixel')
assert mesh and body_material
assert not unreal.WidgetService.is_pie_running()
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name() == 'L_Cindehearth'
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
matches = [a for a in actors.get_all_level_actors() if a.get_actor_label() == 'CH_GormHouse_P2']
assert len(matches) == 1
actor = matches[0]
component = actor.static_mesh_component
backup_path = evidence / 'repair_unreal_replacement_before.json'
backup = dict(actor=actor.get_path_name(), mesh=component.static_mesh.get_path_name(),
              materials=[component.get_material(i).get_path_name() for i in range(component.get_num_materials())],
              location=list(actor.get_actor_location().to_tuple()),
              rotation=list(actor.get_actor_rotation().to_tuple()),
              scale=list(actor.get_actor_scale3d().to_tuple()), tags=[str(t) for t in actor.tags])
assert backup['mesh'].startswith(old_base + '/')
map_backup = root / 'Saved/CindehearthAutomation/L_Cindehearth_before_mesh_repair.umap'
if STAGE == 'resume':
    assert json.loads(backup_path.read_text(encoding='utf-8')) == backup
    assert map_backup.is_file()
else:
    assert not backup_path.exists(), 'Replacement already started; inspect its evidence before retrying'
    assert not map_backup.exists()
    shutil.copy2(root / 'Content/Maps/L_Cindehearth.umap', map_backup)
    backup_path.write_text(json.dumps(backup, indent=2), encoding='utf-8')

mesh_editor = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
settings = mesh_editor.get_nanite_settings(mesh)
settings.enabled = False
mesh_editor.set_nanite_settings(mesh, settings, True)
assert mesh.get_num_triangles(0) == 1994
assert not mesh_editor.get_nanite_settings(mesh).enabled
assert len(mesh.static_materials) == 2

roof_mi = base + '/MI_CH_RepairedRoof_Pixel'
s = unreal.MaterialService
if STAGE == 'resume':
    assert unreal.EditorAssetLibrary.does_asset_exist(roof_mi)
else:
    assert not unreal.EditorAssetLibrary.does_asset_exist(roof_mi)
    assert s.create_instance(old_base + '/M_CH_P2_Pixel', 'MI_CH_RepairedRoof_Pixel', base).success
assert s.set_instance_texture_parameter(roof_mi, 'Albedo', asset + '/Textures/CH_GormRoof_Color_v002')
for key, value in dict(PixelGrid=512., NormalStrength=0., Roughness=.92, Specular=.10, AmbientLift=.10).items():
    assert s.set_instance_scalar_parameter(roof_mi, key, value)
assert s.set_instance_vector_parameter(roof_mi, 'Tint', 1.05, 1.05, 1.05, 1.)
assert s.save_instance(roof_mi)
roof_material = unreal.load_asset(roof_mi)
assert roof_material
diag = unreal.MaterialNodeService.get_material_diagnostics(old_base + '/M_CH_P2_Pixel')
assert diag.success and diag.is_compiled_ok, list(diag.compile_errors)

with unreal.ScopedEditorTransaction('Repair Gorm house roof and detached mesh pieces'):
    actor.modify()
    component.modify()
    mesh.modify()
    mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    mesh.set_material(0, body_material)
    mesh.set_material(1, roof_material)
    assert component.set_static_mesh(mesh)
    component.set_material(0, body_material)
    component.set_material(1, roof_material)
    component.set_collision_profile_name('BlockAll')
assert list(actor.get_actor_location().to_tuple()) == backup['location']
assert list(actor.get_actor_rotation().to_tuple()) == backup['rotation']
assert list(actor.get_actor_scale3d().to_tuple()) == backup['scale']
assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
result = dict(status='applied', actor=actor.get_path_name(), mesh=mesh.get_path_name(),
              triangles=mesh.get_num_triangles(0), nanite=mesh_editor.get_nanite_settings(mesh).enabled,
              materials=[component.get_material(i).get_path_name() for i in range(component.get_num_materials())],
              collision=str(mesh.get_editor_property('body_setup').get_editor_property('collision_trace_flag')),
              location=list(actor.get_actor_location().to_tuple()), rotation=list(actor.get_actor_rotation().to_tuple()),
              bounds=str(actor.get_actor_bounds(False)), map_backup=str(map_backup))
(evidence / 'repair_unreal_applied.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
print(json.dumps(result))
