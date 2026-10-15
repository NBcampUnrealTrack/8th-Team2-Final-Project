import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Restyle'
dest='/Game/Cindehearth/HD2D_v002/Materials'
assert not unreal.WidgetService.is_pie_running()
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='L_Cindehearth'
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
before=json.loads((root/'verification/level_before.json').read_text(encoding='utf-8'))
index={a.get_actor_label():a for a in actors.get_all_level_actors()}
keys=['Stone','StoneLight','Rock','Ash','Soil','Path','Roof','RoofPatch','Timber','Wood','Iron','Brass','Paper','Cloth','Pine']
new={key:unreal.load_asset(dest+'/MI_CH_'+key) for key in keys};assert all(new.values())
changes=[]
assert not (root/'verification/palette_applied.json').exists()
with unreal.ScopedEditorTransaction('Cindehearth HD2D environment palette'):
    for row in before:
        if not row.get('mesh'):continue
        a=index[row['label']];c=a.static_mesh_component
        for slot,old in enumerate(row['materials']):
            if not old:continue
            short=old.rsplit('/',1)[-1].split('.')[0]
            key=short.replace('MI_CH_','') if short.startswith('MI_CH_') else short.replace('MI_Prop_','') if short.startswith('MI_Prop_') else None
            if key=='Metal':key='Iron'
            if key not in new:continue
            assert c.get_material(slot).get_path_name()==old
            a.modify();c.modify();c.set_material(slot,new[key])
            assert c.get_material(slot)==new[key]
            changes.append(dict(label=row['label'],slot=slot,before=old,after=new[key].get_path_name()))
    for a in index.values():
        if a.actor_has_tag('CH.Resident') and isinstance(a,unreal.PaperSpriteActor):
            a.get_component_by_class(unreal.PaperSpriteComponent).set_sprite_color(unreal.LinearColor(.95,.95,.91,1))
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
(root/'verification/palette_applied.json').write_text(json.dumps(changes,indent=2),encoding='utf-8')
print('MATERIAL SLOTS REPLACED',len(changes))
