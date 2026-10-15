import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Polish_v003/verification'
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world();assert w
d=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.NUCindehearthDirector)[0]
actors=unreal.GameplayStatics.get_all_actors_with_tag(w,'Cindehearth');rows=[]
def visible(a):
 c=a.get_component_by_class(unreal.StaticMeshComponent)
 return c and c.is_visible() and not c.get_editor_property('hidden_in_game') and not a.get_editor_property('hidden')
for q in [0,7,8,9,11,12]:
 d.set_editor_property('auto_play_epilogue',False);d.set_chapter_progress(q)
 gates=[a.get_name() for a in actors if a.actor_has_tag('CH.Q8.Blocker') and visible(a)]
 cocoons=[a.get_name() for a in actors if a.actor_has_tag('CH.Q11.Cocoon') and visible(a)]
 assert bool(gates)==(q<8),(q,gates)
 assert bool(cocoons)==(q<11),(q,cocoons)
 rows.append(dict(quest=q,visible_barriers=len(gates),visible_cocoon_parts=len(cocoons),mood=str(d.get_square_mood())))
clues=[a for a in actors if a.actor_has_tag('Shadowvale.Export') or a.actor_has_tag('Doren.DoubleRing')]
assert clues and all(a.get_actor_location().y<500 and a.get_actor_location().z<0 for a in clues)
d.set_chapter_progress(0);d.set_night(False)
(root/'presented_state_qa.json').write_text(json.dumps(dict(passed=True,states=rows,q7_unique_clue_actors=len(clues)),indent=2));print('VISIBLE COMPONENT STATE QA PASS',rows)
