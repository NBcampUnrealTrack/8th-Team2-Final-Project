import unreal
import json
from pathlib import Path
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
records=[]
for a in actors:
    if a.get_actor_label() in ['CH_GormHouse_Firewood0','CH_GormHouse_Firewood1','CH_GormHouse_Firewood2','CH_Lettering_Gorm']:
        p=a.get_actor_location()
        records.append(dict(label=a.get_actor_label(),location=list(p.to_tuple())))
assert len(records)==4
out=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Pipeline/verification/p2_surroundings_before.json'
assert not out.exists()
out.write_text(json.dumps(records,indent=2),encoding='utf-8')
for a in actors:
    if a.get_actor_label().startswith('CH_GormHouse_Firewood'):
        p=a.get_actor_location();p.x+=220
        a.set_actor_location(p,False,False)
    elif a.get_actor_label()=='CH_Lettering_Gorm':
        p=a.get_actor_location();p.y+=38
        a.set_actor_location(p,False,False)
mi='/Game/Cindehearth/Tripo/GormHouse_P2_v001/MI_CH_GormP2_Pixel'
unreal.MaterialService.set_instance_scalar_parameter(mi,'AmbientLift',.1)
unreal.MaterialService.set_instance_vector_parameter(mi,'Tint',1.05,1.05,1.05)
unreal.MaterialService.save_instance(mi)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
print('SAVED: house, entrance clearance, plaque, pixel material')
