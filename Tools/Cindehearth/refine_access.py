import unreal
import json
from pathlib import Path
AS=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
LS=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert not unreal.WidgetService.is_pie_running()
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='L_Cindehearth'
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth'
D=json.loads((root/'layout.json').read_text(encoding='utf-8'))
actors=AS.get_all_level_actors()
byname={a.get_actor_label():a for a in actors}
for name in ('GormHouse','MinersHome_01','MinersHome_04'):
    spec=next(p for p in D['placements'] if p['name']==name)
    stone=byname['CH_'+name+'_Stone']
    wanted=unreal.Vector(*[v*45 for v in spec['pos']]);delta=wanted-stone.get_actor_location()
    for a in actors:
        if a.get_actor_label().startswith('CH_'+name+'_'):
            a.set_actor_location(a.get_actor_location()+delta,False,False)
for p in D['markers']:
    a=byname.get('CH_'+p['name'])
    if a:a.set_actor_location(unreal.Vector(*[v*45 for v in p['pos']]),False,False)
for name in ('DorenLetter_Paper','DorenLetter_Seal','Lettering_DorenSeal'):
    a=byname.get('CH_'+name)
    if a:a.set_actor_location(unreal.Vector(-23.8*45,-39.64*45,1.3*45),False,False)
for name in ('Lettering_Gorm','Sign_Gorm'):
    a=byname.get('CH_'+name)
    if a:a.set_actor_location(unreal.Vector(-23.3*45,-39.12*45,1.3*45),False,False)
byname['CH_Messenger'].set_actor_location(unreal.Vector(-25.6*45,-40.8*45,0),False,False)
# Open the mountain rim exactly where the cemetery path exits the settlement.
for a in actors:
    if a.get_actor_label().startswith('CH_SideCliff_1_'):
        loc=a.get_actor_location()
        if 44*45<loc.y<73*45:
            a.set_actor_hidden_in_game(True);a.set_is_temporarily_hidden_in_editor(True);a.set_actor_enable_collision(False)
# Keep background ridge dressing clear of the cemetery trail corridor.
for a in actors:
    if not a.get_actor_label().startswith('CH_RidgeMass_'):continue
    c,e=a.get_actor_bounds(False)
    for i in range(29):
        x=(66+i)*45;y=(48+i*16/28)*45
        if abs(x-c.x)<e.x+100 and abs(y-c.y)<e.y+100:
            a.set_actor_hidden_in_game(True);a.set_is_temporarily_hidden_in_editor(True);a.set_actor_enable_collision(False)
            print('CLEARED TRAIL',a.get_actor_label());break
AS.select_nothing();LS.save_current_level();print('ACCESS REFINED')
