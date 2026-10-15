import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Restyle/verification'
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
rows=[]
for a in actors:
    if not isinstance(a,unreal.StaticMeshActor) or not a.static_mesh_component.static_mesh:continue
    name=a.get_actor_label();m=a.static_mesh_component.static_mesh
    if any(k in name for k in ['Cliff','RidgeMass','OrePile_00','Pine_01','MineEntrance','Fire','Smoke','Tavern','Home_01']):
        o,e=a.get_actor_bounds(False);b=m.get_bounding_box()
        rows.append(dict(label=name,location=list(a.get_actor_location().to_tuple()),bounds=[list(o.to_tuple()),list(e.to_tuple())],mesh_bounds=[list(b.min.to_tuple()),list(b.max.to_tuple())],mesh=m.get_path_name(),visible=a.static_mesh_component.get_editor_property('visible')))
(root/'nature_bounds.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
for name,p,t,f in [('architecture',(1550,-1500,930),(-300,0,160),43),('houses',(550,-3700,650),(-700,-2000,160),43)]:
    v=unreal.Vector(*p)
    print(unreal.ScreenshotService.capture_from_pose(v,unreal.MathLibrary.find_look_at_rotation(v,unreal.Vector(*t)),'CH_Restyle_'+name,f,False,False))
