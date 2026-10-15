import unreal,json,sys,importlib
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Dense_v004';sys.path.insert(0,str(root))
import layout_transform as lt
importlib.reload(lt)
out=root/'verification';receipt=out/'layout_applied.json'
assert not receipt.exists(),'Already applied; use baseline-relative targeted revision.'
assert not unreal.WidgetService.is_pie_running()
sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);actors={a.get_actor_label():a for a in sub.get_all_level_actors()}
assert len(actors)==len(lt.ROWS)==3195
for row in lt.ROWS:
 a=actors[row['label']];assert max(abs(x-y) for x,y in zip(a.get_actor_location().to_tuple(),row['location']))<.05,row['label']
plan=[lt.transform(r) for r in lt.ROWS]
with unreal.ScopedEditorTransaction('Cindehearth v004 compact village and connected routes'):
 for r in plan:
  if r['mode']=='fixed':continue
  a=actors[r['label']];a.modify();a.set_actor_location(unreal.Vector(*r['location']),False,True)
  a.set_actor_rotation(unreal.Rotator(roll=r['rotation'][0],pitch=r['rotation'][1],yaw=r['rotation'][2]),True)
  a.set_actor_scale3d(unreal.Vector(*r['scale']))
 d=actors['CH_LevelDirector'];d.modify()
 route=[unreal.Transform(location=unreal.Vector(*t['location']),rotation=unreal.Rotator(roll=t['rotation'][0],pitch=t['rotation'][1],yaw=t['rotation'][2]),scale=unreal.Vector(*t['scale'])) for t in lt.camera_route()]
 d.set_editor_property('epilogue_camera_route',route)
 for key,pos in [('Entry',[1077.7,195.975,215]),('Lower',[1172.7,253.475,29])]:
  a=sub.spawn_actor_from_class(unreal.TargetPoint,unreal.Vector(*pos));a.set_actor_label('CH_D4_Archive'+key);a.tags=['Cindehearth','CH.Archive.'+key];a.set_folder_path('Cindehearth/Runtime');a.set_actor_hidden_in_game(True)
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
for r in plan:
 a=actors[r['label']];assert max(abs(x-y) for x,y in zip(a.get_actor_location().to_tuple(),r['location']))<.05,r['label']
receipt.write_text(json.dumps(dict(actors=len(plan),footprint_design_m=[111,120.6],area_ratio=.74*.67,plan=plan,epilogue=lt.camera_route()),indent=2),encoding='utf-8')
print('LAYOUT APPLIED',len(plan),'archive anchors2; tier heights preserved')
