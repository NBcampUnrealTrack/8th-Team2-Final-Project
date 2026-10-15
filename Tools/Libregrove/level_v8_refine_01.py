"""Visual fixes found in unedited native Waterfront_001 and Square_001."""
import unreal,json,math
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Docs/Libregrove/Level_V8_20260908'
E=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='L_Libregrove'
actors={a.get_actor_label():a for a in E.get_all_level_actors()};before=[]
inventory=json.loads((OUT/'reuse_inventory.json').read_text(encoding='utf8'))
rock=next(r for r in inventory if '/HD2D_v002/Models/Rock/' in r['path'] and r['type']=='StaticMesh')
m=unreal.load_asset(rock['path']);ex=rock['extent'];origin=rock['origin']
for i in range(15):
    a=actors['LG8_WaterfallCliff_%02d'%i]
    before.append({'actor':a.get_actor_label(),'mesh':a.static_mesh_component.static_mesh.get_path_name(),'location':a.get_actor_location().to_tuple(),'scale':a.get_actor_scale3d().to_tuple()})
    if i<5:x=2590+(i%2)*130;y=2200+i*65;w=330;d=260;h=450+i*16;z=-35
    elif i<10:x=2040+(i%2)*85;y=2200+(i-5)*65;w=280;d=280;h=430+(i-5)*22;z=-35
    elif i<13:x=2220+(i-10)*130;y=2350;w=250;d=270;h=560;z=-30
    else:x=2610 if i==13 else 2050;y=1910;w=220;d=210;h=120;z=-55
    sc=[w/(2*ex[0]),d/(2*ex[1]),h/(2*ex[2])]
    assert a.static_mesh_component.set_static_mesh(m)
    a.set_actor_scale3d(unreal.Vector(*sc));a.set_actor_rotation(unreal.Rotator(yaw=i*47),False)
    a.set_actor_location(unreal.Vector(x,y,z-(origin[2]-ex[2])*sc[2]),False,False)
    a.static_mesh_component.set_collision_profile_name('NoCollision')
# Remove harsh bands and thick pixel foam from the river, using only its new instance.
mp='/Game/Libregrove/Level_V8/Materials/MI_LG8_River'
for name,value in {'DepthSteps':48,'FoamWidth':3,'ShallowDistance':80,'HighlightStrength':.44,'BandStrength':.10,'OpacityBase':.66,'RoughnessBase':.19}.items():assert unreal.MaterialService.set_instance_scalar_parameter(mp,name,value)
assert unreal.EditorAssetLibrary.save_asset(mp)
# Day lamps should read as warm accents, not floodlights.
for a in actors.values():
    if a.get_actor_label().startswith('LG8_LanternLight_'):a.light_component.set_intensity(18)
# Shrine tread terrain must stay below every real step, preventing intermittent buried risers.
p=json.loads((OUT/'design_v001.json').read_text(encoding='utf8'));n=p['N'];s=p['spacing'];o=p['origin']
x0=int((-144-o)/s);x1=int((144-o)/s);y0=int((1940-o)/s);y1=int((2426-o)/s)
vals=list(unreal.LandscapeService.get_height_in_region('LG8_Terrain',x0,y0,x1-x0+1,y1-y0+1));patched=[]
for j in range(y1-y0+1):
    y=o+(y0+j)*s;k=max(0,min(13,int((y-1950)/34)));top=30+(k+1)*195/14
    for i in range(x1-x0+1):patched.append(min(vals[j*(x1-x0+1)+i],top-18))
assert unreal.LandscapeService.set_height_in_region('LG8_Terrain',x0,y0,x1-x0+1,y1-y0+1,patched)
assert L.save_current_level()
with (OUT/'refinement_01.json').open('x',encoding='utf8') as f:json.dump({'replaced_cliffs':before,'river':mp,'terrain_patch':[x0,y0,x1,y1]},f,indent=2)
print('Natural waterfall cliffs, continuous fall opening, softer river and daytime lamps; shrine treads unburied')
