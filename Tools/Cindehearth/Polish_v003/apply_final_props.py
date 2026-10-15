import unreal,json,sys,math,importlib
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Polish_v003';sys.path.insert(0,str(root))
import ue_common as C
importlib.reload(C)
assert not unreal.WidgetService.is_pie_running();assert not (root/'verification/final_props_applied.json').exists()
rows=[]
with unreal.ScopedEditorTransaction('Cindehearth logging machinery, archives and ring stockpile'):
 # Replace the shelf front-cap candidate with the visually corrected source in a new asset namespace.
 for label,a in list(C.ACTORS.items()):
  if label.startswith('CH_LedgerShelf_') and label.endswith('_P3'):
   mesh,mat=C.resources('ShelfOpen');a.static_mesh_component.set_static_mesh(mesh);a.static_mesh_component.set_material(0,mat);rows.append(dict(shelf=label))
 p=(-900,-6525,0)
 gear=C.spawn('CH_AbandonedLoggingGear_P3','Pulley',p,(245,252,247),-90,tags=['CH.Q5','CH.Zone.Pinehollow'])
 for label,a in list(C.ACTORS.items()):
  if label.startswith('CH_AbandonedLoggingGear_') and label!='CH_AbandonedLoggingGear_P3' and isinstance(a,unreal.StaticMeshActor):C.hide(a,True)
 bound=C.ACTORS['CH_BoundCogNaia_P3'];bound.set_actor_location(unreal.Vector(-900,-6610,33),False,True)
 C.spawn('CH_BoundCogNaia_Rings_P3','RingLinks',(-900,-6610,38),(64,61,39),10,tags=['CH.Q5.BoundNaia'])
 rows.append(dict(pulley=gear.get_actor_label()))
 for i,(x,y) in enumerate([(225,5985),(855,5985),(225,6435),(855,6435)]):
  prefix='CH_RestRingStockpile_'+str(i)
  for label,a in list(C.ACTORS.items()):
   if label.startswith(prefix+'_') and isinstance(a,unreal.StaticMeshActor):C.hide(a,True)
  made=[]
  for tier in range(3):
   for n in range(6-tier*2):
    theta=n*math.tau/(6-tier*2)+tier*.8;radius=73-tier*24
    a=C.spawn(prefix+'_P3_'+str(tier)+'_'+str(n),'RingLinks',(x+math.cos(theta)*radius,y+math.sin(theta)*radius,135+tier*29),(100,94,57),n*67+tier*33,tags=['CH.Q10.ScaleOnly'])
    made.append(a.get_actor_label())
  rows.append(dict(stockpile=prefix,clusters=made,document_clues=False))
 old=C.ACTORS['CH_HollowcoreCocoon_Iron'];center,ext=old.get_actor_bounds(False);bottom=center.z-ext.z
 cocoon=C.spawn('CH_HollowcoreCocoon_P3','Hollowcore',(center.x,center.y,bottom),(194,194,267),-90,tags=['CH.Q11.Cocoon'])
 C.hide(old,True)
 # Linked restraint clusters follow the cocoon surface and share its liberation state.
 for tier in range(3):
  for n in range(6):
   theta=n*math.tau/6+tier*.3;radius=82 if tier<2 else 57;z=bottom+35+tier*76
   target=unreal.Vector(center.x+radius*math.cos(theta),center.y+radius*math.sin(theta),z)
   a=C.spawn('CH_HollowcoreRingWrap_P3_'+str(tier)+'_'+str(n),'RingLinks',target.to_tuple(),(63,59,36),0,tags=['CH.Q11.Cocoon'])
   a.set_actor_rotation(unreal.Rotator(pitch=70,yaw=math.degrees(theta)),False)
   c,e=a.get_actor_bounds(False);a.set_actor_location(a.get_actor_location()+target-c,False,True)
 rows.append(dict(cocoon=cocoon.get_actor_label(),ring_clusters=18))
C.ACTORS['CH_LevelDirector'].set_chapter_progress(0)
C.save_result('final_props_applied',rows);print('FINAL PROPS APPLIED',len(rows))
