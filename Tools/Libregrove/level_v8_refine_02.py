"""Visual fixes evidenced by actual entry/square gameplay captures."""
import unreal,json,shutil,hashlib
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Docs/Libregrove/Level_V8_20260908'
assert not unreal.WidgetService.is_pie_running()
src=ROOT/'Content/Libregrove/Level_V8/Maps/L_Libregrove.umap';backup=ROOT/'Saved/LibregroveV8Backup/BeforeEntryClear_20260909.umap'
backup.parent.mkdir(parents=True,exist_ok=True);assert not backup.exists();shutil.copy2(src,backup)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);rows=[];tree=None
for a in actors.get_all_level_actors():
    for comp in a.get_components_by_class(unreal.InstancedStaticMeshComponent):
        if not comp.static_mesh or 'BroadleafTree' not in comp.static_mesh.get_path_name():continue
        tree=comp.static_mesh.get_path_name()
        for i in range(comp.get_instance_count()):
            t=comp.get_instance_transform(i,True);p=t.translation
            if abs(p.x)<500 and p.y<-3700:rows.append({'component':comp.get_path_name(),'index':i,'location':p.to_tuple(),'rotation':t.rotation.to_tuple(),'scale':t.scale3d.to_tuple()})
assert len(rows)==3,rows
with (OUT/'entry_tree_clear_backup.json').open('x',encoding='utf8') as f:json.dump({'level_backup':str(backup),'sha256':hashlib.sha256(backup.read_bytes()).hexdigest(),'instances':rows},f,indent=2)
for row in rows:
    x,y,z=row['location'];r=unreal.FoliageService.remove_foliage_in_radius('/Game/Libregrove/Level_V8/Foliage/FT_LG8_Canopy',x,y,5.)
    assert r.success,r.error_message;print(r)
mapping=json.loads((OUT/'HD2D/instance_mapping.json').read_text(encoding='utf8'))
terrain=unreal.load_asset(next(r['instance'] for r in mapping if r['kind']=='Landscape'))
E=unreal.MaterialEditingLibrary;before={};changes={'TileCm_Cobblestone':720.,'TileCm_Grass':300.,'TileCm_Dirt':360.,'TileCm_Moss':330.}
for k,v in changes.items():
    before[k]=E.get_material_instance_scalar_parameter_value(terrain,k);E.set_material_instance_scalar_parameter_value(terrain,k,v);assert abs(E.get_material_instance_scalar_parameter_value(terrain,k)-v)<.001
assert unreal.EditorAssetLibrary.save_loaded_asset(terrain)
dof=next(a for a in actors.get_all_level_actors() if a.get_actor_label()=='LG8_PP_DOF_Optional_Windows')
settings=dof.get_editor_property('settings');settings.set_editor_property('depth_of_field_sensor_width',120.);settings.set_editor_property('depth_of_field_fstop',2.4);dof.set_editor_property('settings',settings)
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
with (OUT/'refinement_02.json').open('x',encoding='utf8') as f:json.dump({'trees_removed':3,'recoverable_level_backup':str(backup),'terrain_before':before,'terrain_after':changes,'dof_sensor_width':120.,'dof_fstop':2.4},f,indent=2)
print('Three foreground trees cleared; larger cobbles; optional DOF refined')
