"""Replace the remaining procedural cave walls and conceal exterior terrain gaps."""
import unreal,json,sys,math,random,importlib
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Polish_v003';sys.path.insert(0,str(root))
import ue_common as C
importlib.reload(C)
assert not unreal.WidgetService.is_pie_running();assert not (root/'verification/dungeon_refined.json').exists()
rock=json.loads((root.parent/'Restyle/verification/import_Rock.json').read_text())
C.MESH['Rock']=unreal.load_asset(rock['meshes'][0]['path']);C.MAT['Rock']=unreal.load_asset('/Game/Cindehearth/HD2D_v002/Materials/MI_CH_P2_Rock')
rows=[];rng=random.Random(905031)
def floor(label,position,size,shape='Cube',material='MI_CH_Rock'):
 a=C.AS.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*position));a.set_actor_label(label);a.set_folder_path('Cindehearth/Polish_v003/Terrain')
 a.tags=['Cindehearth','CH.Polish_v003'];c=a.static_mesh_component;c.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/'+shape));a.set_actor_scale3d(unreal.Vector(*(v/100 for v in size)))
 c.set_material(0,unreal.load_asset('/Game/Cindehearth/HD2D_v002/Materials/'+material));c.set_collision_profile_name('NoCollision');rows.append(label)
with unreal.ScopedEditorTransaction('Cindehearth cave and terrain continuity'):
 floor('CH_P3_BackgroundBedrock',(-2500,3000,-700),(40000,40000,40))
 for i,(x,y,sx,sy,h) in enumerate([(-1600,4550,3400,1200,380),(2420,4550,2400,1250,380),(-1000,6350,1700,3600,280),(2060,6350,1700,3600,280)]):
  a=C.spawn('CH_P3_MineBackfill_'+str(i),'Ridge',(x,y,-210),(sx,sy,h),0);rows.append(a.get_actor_label())
 old=C.ACTORS['CH_DeepveinTunnel_Rock'];C.hide(old,True)
 for i,y in enumerate(range(4050,8641,180)):
  design_y=y/45
  z=315 if design_y<106 else 315-(design_y-106)/22*180 if design_y<128 else 135 if design_y<162 else 135-(design_y-162)/26*180 if design_y<188 else -45
  depot=128<=design_y<=150;half=630 if depot else 190
  for side in [-1,1]:
   if design_y<158:
    a=C.spawn('CH_P3_TunnelWall_'+str(i)+'_'+str(side),'Wall',(540+side*half,y,z-12),(65,192,202),0)
   else:
    a=C.spawn('CH_P3_OrganicWall_'+str(i)+'_'+str(side),'Rock',(540+side*(half+22),y,z-35),(240,115,227),90+rng.uniform(-8,8))
   rows.append(a.get_actor_label())
 floor('CH_P3_DepotFloor',(540,6255,121),(1260,1080,22),material='MI_CH_Path')
 old=C.ACTORS['CH_Chamber_25m_Rock'];C.hide(old,True)
 floor('CH_P3_ChamberFloor',(540,9315,-55),(1140,1140,20),'Cylinder')
 for tier in range(2):
  for n in range(18):
   angle=n*math.tau/18+tier*.10
   # The south opening remains clear for the existing six-waypoint dungeon route.
   if math.sin(angle)<-.90:continue
   r=610+tier*32;z=-90+tier*122+rng.uniform(-12,8)
   a=C.spawn('CH_P3_ChamberRock_'+str(tier)+'_'+str(n),'Rock',(540+r*math.cos(angle),9315+r*math.sin(angle),z),(295,205,205),math.degrees(angle)+90)
   rows.append(a.get_actor_label())
 for name,p,col,intensity,radius in [('Cocoon',(540,9210,315),(.31,.47,.57),1200,900),('Depot',(540,6255,380),(.48,.52,.55),900,1000)]:
  a=C.AS.spawn_actor_from_class(unreal.PointLight,unreal.Vector(*p));a.set_actor_label('CH_P3_Readability_'+name);a.tags=['Cindehearth','CH.Polish_v003'];a.set_folder_path('Cindehearth/Polish_v003/Lighting')
  c=a.point_light_component;c.set_intensity(intensity);c.set_light_color(unreal.LinearColor(*col));c.set_attenuation_radius(radius);c.set_editor_property('source_radius',45.);c.set_cast_shadows(False)
C.save_result('dungeon_refined',rows);print('DUNGEON/TERRAIN ELEMENTS',len(rows))
