"""Natural overlapping P2 rock silhouettes; existing route collision remains unchanged."""
import unreal,json,sys,math,random,importlib
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Polish_v003';sys.path.insert(0,str(root))
import ue_common as C
importlib.reload(C)
assert not unreal.WidgetService.is_pie_running();assert not (root/'verification/ridges_applied.json').exists()
rows=[];rng=random.Random(905003)
with unreal.ScopedEditorTransaction('Cindehearth P2 mountain silhouettes'):
 for old in list(C.ACTORS.values()):
  if not isinstance(old,unreal.StaticMeshActor):continue
  label=old.get_actor_label();comp=old.static_mesh_component
  if not comp.static_mesh or not comp.is_visible() or old.get_editor_property('hidden'):continue
  far=label.startswith('CH_HD2D_DistantRidge_')
  close=label.endswith('_HD2D') and any(k in label for k in ['RidgeMass','NorthCliff','SideCliff'])
  if not (far or close):continue
  center,extent=old.get_actor_bounds(False);bottom=center.z-extent.z
  if far:
   h=extent.z*2;size=(h*2.65,h*2.1,h*1.03);yaw=rng.uniform(-15,15);bottom-=h*.11
  else:
   # Rotate the broad ridge along the mountain wall. Keep its base embedded to hide the underside.
   h=max(180,extent.z*1.7);side='SideCliff' in label or ('RidgeMass' in label and abs(center.x)>3000)
   width=max(extent.x,extent.y)*2.25;depth=min(extent.x,extent.y)*1.5
   size=(width,depth,h);yaw=(90 if side else 0)+rng.uniform(-9,9);bottom-=h*.16
  a=C.spawn(label+'_P3','Ridge',(center.x,center.y,bottom),size,yaw)
  C.hide(old,keep_collision=True)
  rows.append(dict(source=label,created=a.get_actor_label(),far=far))
C.save_result('ridges_applied',rows);print('P2 RIDGES',len(rows))
