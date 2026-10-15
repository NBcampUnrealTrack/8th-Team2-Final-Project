import unreal
from pathlib import Path
import json
ROOT=Path(unreal.Paths.project_dir())/'Tools/Cindehearth'
AS=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
LS=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
lib=unreal.EditorAssetLibrary
p='/Game/Cindehearth/Signs/M_CH_Sign'
if not lib.does_asset_exist(p):
    assert unreal.MaterialService.create_material('M_CH_Sign','/Game/Cindehearth/Signs').success
    m=unreal.load_asset(p);m.set_editor_property('two_sided',True)
    node=unreal.MaterialNodeService.create_parameter(p,'Texture','Sign','','/Game/Cindehearth/Signs/T_CH_VillageName',-300,0)
    assert unreal.MaterialNodeService.connect_to_output(p,node.id,'RGB','BaseColor')
    assert unreal.MaterialService.compile_material(p);unreal.MaterialService.save_material(p)
index={a.get_actor_label():a for a in AS.get_all_level_actors()}
for name,lines,pos,size,yaw in json.loads((ROOT/'signs.json').read_text(encoding='utf-8')):
    tex=unreal.load_asset('/Game/Cindehearth/Signs/T_CH_'+name);assert tex
    tex.set_editor_property('filter',unreal.TextureFilter.TF_BILINEAR)
    tex.set_editor_property('never_stream',True)
    lib.save_loaded_asset(tex)
    mi='/Game/Cindehearth/Signs/MI_CH_'+name
    if not lib.does_asset_exist(mi):
        assert unreal.MaterialService.create_instance(p,'MI_CH_'+name,'/Game/Cindehearth/Signs').success
        assert unreal.MaterialService.set_instance_texture_parameter(mi,'Sign',tex.get_path_name())
        unreal.MaterialService.save_instance(mi)
    old=index.get('CH_Sign_'+name)
    if old:old.set_actor_hidden_in_game(True);old.set_is_temporarily_hidden_in_editor(True)
    a=index.get('CH_Lettering_'+name)
    if not a:
        a=AS.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*[v*45 for v in pos]),unreal.Rotator(roll=-90,yaw=yaw))
        a.set_actor_label('CH_Lettering_'+name);a.set_folder_path('Cindehearth/Signs')
        a.tags=[unreal.Name('Cindehearth')]
        a.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Plane'))
    # BasicShapes/Plane UVs require a half turn in the sign plane for upright text.
    a.set_actor_rotation(unreal.Rotator(roll=90,yaw=yaw+180),False)
    a.static_mesh_component.set_material(0,unreal.load_asset(mi))
    a.static_mesh_component.set_collision_profile_name('NoCollision')
    a.set_actor_scale3d(unreal.Vector(size[0]*.45,size[1]*.45,.45))
    if name=='DorenSeal':
        a.tags=list(a.tags)+[unreal.Name('CH.EpilogueOnly')]
        a.set_actor_hidden_in_game(True);a.set_is_temporarily_hidden_in_editor(True)
    if name=='Rockfall':a.tags=list(a.tags)+[unreal.Name('CH.Q8.Blocker')]
LS.save_current_level();AS.select_nothing()
print('SIGNAGE INSTALLED')
