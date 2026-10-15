import unreal,json
from pathlib import Path
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world();assert w
p=unreal.GameplayStatics.get_player_pawn(w,0);assert p
def path(o):return o.get_path_name() if o else None
rows=[]
for s in p.get_components_by_class(unreal.PaperFlipbookComponent):
 m=s.get_material(0)
 rows.append({'component':s.get_name(),'material':path(m),'flipbook':path(s.get_flipbook()),'color':str(s.get_sprite_color()),'visible':s.is_visible(),'scale':str(s.get_world_scale()),'parent':path(m.get_editor_property('parent')) if isinstance(m,unreal.MaterialInstance) else None,'scalars':str(m.get_editor_property('scalar_parameter_values')) if isinstance(m,unreal.MaterialInstance) else None})
r={'engine':unreal.SystemLibrary.get_engine_version(),'pawn':p.get_name(),'class':path(p.get_class()),'location':str(p.get_actor_location()),'mode':path(unreal.GameplayStatics.get_game_mode(w).get_class()),'sprites':rows,'components':[{'name':c.get_name(),'class':path(c.get_class())} for c in p.get_components_by_class(unreal.ActorComponent)]}
print(json.dumps(r))
