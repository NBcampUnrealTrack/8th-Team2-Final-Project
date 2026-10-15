import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Restyle'
assert not unreal.WidgetService.is_pie_running()
before=json.loads((root/'verification/level_before.json').read_text(encoding='utf-8'))
actors={a.get_actor_label():a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()}
fixed=[]
for row in before:
    if not row.get('hidden'):continue
    a=actors.get(row['label']+'_HD2D')
    if not a:continue
    a.modify();a.static_mesh_component.set_visibility(False,True);a.set_actor_hidden_in_game(True);a.set_is_temporarily_hidden_in_editor(True);a.set_actor_enable_collision(False)
    fixed.append(a.get_actor_label())
unreal.ViewportService.set_game_view(True)
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
(root/'verification/preserved_hidden_nature.json').write_text(json.dumps(fixed,indent=2),encoding='utf-8')
print('PRESERVED PRIOR ROUTE CLEARANCE',fixed)
