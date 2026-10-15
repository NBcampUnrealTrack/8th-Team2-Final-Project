import unreal,json,sys,importlib,math
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Polish_v003';sys.path.insert(0,str(root))
import ue_common as C
importlib.reload(C)
assert not unreal.WidgetService.is_pie_running();assert not (root/'verification/final_seams.json').exists()
rows=[]
with unreal.ScopedEditorTransaction('Cindehearth portal seams and cave light balance'):
 for label,a in list(C.ACTORS.items()):
  if not isinstance(a,unreal.StaticMeshActor) or not label.endswith('_P3'):continue
  if '/Models/Portal/' not in a.static_mesh_component.static_mesh.get_path_name():continue
  center,extent=a.get_actor_bounds(False);bottom=center.z-extent.z;yaw=a.get_actor_rotation().yaw-90;theta=math.radians(yaw)
  def point(x,y,z):return (center.x+x*math.cos(theta)-y*math.sin(theta),center.y+x*math.sin(theta)+y*math.cos(theta),bottom+z)
  # Recessed stone jambs close daylight seams between separately watertight P2 frame components.
  for side in [-1,1]:
   n=C.spawn(label+'_InnerJamb_'+str(side),'Wall',point(side*157,23,0),(70,47,219),yaw+90)
   rows.append(n.get_actor_label())
  n=C.spawn(label+'_InnerLintel','Wall',point(0,23,217),(70,330,30),yaw+90);rows.append(n.get_actor_label())
 for name,value in [('Cocoon',95.),('Depot',70.)]:C.ACTORS['CH_P3_Readability_'+name].point_light_component.set_intensity(value)
C.save_result('final_seams',rows);print('PORTAL SEAM SUPPORTS',len(rows))
