import unreal,json
from pathlib import Path
from collections import Counter
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Restyle/verification'
assert not unreal.WidgetService.is_pie_running()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='L_Cindehearth'
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
rows=[]
for a in actors:
    row=dict(label=a.get_actor_label(),path=a.get_path_name(),class_name=a.get_class().get_name(),
             location=list(a.get_actor_location().to_tuple()),rotation=list(a.get_actor_rotation().to_tuple()),
             scale=list(a.get_actor_scale3d().to_tuple()),tags=[str(t) for t in a.tags],
             hidden=a.get_editor_property('hidden'),editor_hidden=a.is_temporarily_hidden_in_editor())
    if isinstance(a,unreal.StaticMeshActor):
        c=a.static_mesh_component;mesh=c.static_mesh
        row.update(mesh=mesh.get_path_name() if mesh else None,materials=[c.get_material(i).get_path_name() if c.get_material(i) else None for i in range(c.get_num_materials())],
                   visible=c.get_editor_property('visible'),collision=str(c.get_collision_profile_name()))
    rows.append(row)
(root/'level_before.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
print('ACTORS',len(rows),'CLASSES',dict(Counter(r['class_name'] for r in rows)))
print('MATERIALS',Counter(m for r in rows for m in r.get('materials',[])).most_common(22))
poses=[('overview',(0,-8600,5400),(0,400,0),53),('arrival',(250,-5500,770),(250,-2900,220),40),
       ('square',(1550,-1500,930),(-300,0,160),43),('mine',(1750,800,970),(600,3500,400),45),
       ('ash',(-6100,-700,2400),(-5900,2400,500),47),('pine',(2600,-8000,2200),(0,-6100,-150),50)]
for name,p,t,f in poses:
    pos=unreal.Vector(*p);rot=unreal.MathLibrary.find_look_at_rotation(pos,unreal.Vector(*t))
    print(unreal.ScreenshotService.capture_from_pose(pos,rot,'CH_Restyle_before_'+name,f,False,False))
