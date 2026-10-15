import unreal,json,sys,math
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Polish_v003';sys.path.insert(0,str(root))
import ue_common as C
assert not unreal.WidgetService.is_pie_running();assert not (root/'verification/architecture_applied.json').exists()
layout=json.loads((root.parent/'layout.json').read_text(encoding='utf-8'));changes=[]
dims={'Tavern':(14,10,6.6),'Smithy':(12,9,4.2),'Assay':(8,6,3.5),'Clinic':(6,5,3.3),'Office':(5,4,3.2)}
with unreal.ScopedEditorTransaction('Cindehearth P2 stone architecture and communal hearth'):
 for p in layout['placements']:
  key=p['module'];prefix='CH_'+p['name']
  if key in dims:
   source=C.ACTORS[prefix+'_Stone'];origin=source.get_actor_location().to_tuple();yaw=source.get_actor_rotation().yaw;ang=math.radians(yaw);w,d,h=dims[key];made=[]
   def point(x,y,z):return [origin[0]+45*(x*math.cos(ang)-y*math.sin(ang)),origin[1]+45*(x*math.sin(ang)+y*math.cos(ang)),origin[2]+45*z]
   def panel(x,y,z,width,height,rot=0,thick=.52):
    if width<=.01 or height<=.01:return
    label=prefix+'_P3Wall_'+str(len(made));C.spawn(label,'Wall',point(x,y,z),(thick*45,width*45,height*45),yaw+90+rot);made.append(label)
   # Actual wall panels retain the central door openings and open smithy front.
   for side in [-1,1]:
    n=math.ceil(d/3.8);rows=math.ceil(h/2)
    for i in range(n):
     for row in range(rows):panel(side*(w/2-.24),-d/2+(i+.5)*d/n,row*h/rows,d/n+.025,h/rows+.01,90)
   n=math.ceil(w/3.8);rows=math.ceil(h/2)
   for i in range(n):
    for row in range(rows):panel(-w/2+(i+.5)*w/n,d/2-.24,row*h/rows,w/n+.025,h/rows+.01)
   if key!='Smithy':
    for side in [-1,1]:
     width=(w-1.6)/2;n=math.ceil(width/3.8)
     for i in range(n):
      for row in range(rows):panel(side*(.8+(i+.5)*width/n),-d/2+.24,row*h/rows,width/n+.02,h/rows+.01)
    panel(0,-d/2+.24,2.6,1.64,h-2.6)
   # Gable courses follow the steep closed roof; the roof remains independently hideable.
   rise=w*.4;layers=8
   for side in [-1,1]:
    for i in range(layers):panel(0,side*(d/2-.16),h+i*rise/layers,w*(1-i/layers),rise/layers+.012,0,.4)
   # Plain stone chimney, aligned with the existing soot anchor.
   for side in [-1,1]:
    panel(.3*w+side*.34,.2*d,h-.1,1.05,3.45,90,.26)
    panel(.3*w,.2*d+side*.4,h-.1,.9,3.45,0,.26)
   hidden=[]
   for suffix in ['Stone','StoneLight','Iron']:
    a=C.ACTORS.get(prefix+'_'+suffix)
    if a:C.hide(a,keep_collision=True);hidden.append(a.get_actor_label())
   changes.append(dict(kind='shell',source=prefix,created=made,hidden=hidden))
  elif key=='Hearth':
   origin=C.ACTORS[prefix+'_Stone'].get_actor_location().to_tuple()
   a=C.spawn(prefix+'_P3','Hearth',origin,(450,450,38),0);hidden=[]
   for suffix in ['Dark','Stone','StoneLight','Timber']:
    old=C.ACTORS.get(prefix+'_'+suffix)
    if old:C.hide(old,keep_collision=suffix=='Stone');hidden.append(old.get_actor_label())
   changes.append(dict(kind='hearth',created=[a.get_actor_label()],hidden=hidden))
  elif key=='MinePortal':
   old=C.ACTORS[prefix+'_Stone'];origin=old.get_actor_location().to_tuple();yaw=old.get_actor_rotation().yaw
   a=C.spawn(prefix+'_P3','Portal',origin,(100,450,250),yaw+90);hidden=[]
   for suffix in ['Stone','StoneLight','Timber']:
    old=C.ACTORS.get(prefix+'_'+suffix)
    if old:C.hide(old,keep_collision=True);hidden.append(old.get_actor_label())
   changes.append(dict(kind='portal',created=[a.get_actor_label()],hidden=hidden))
C.save_result('architecture_applied',changes)
print('REBUILT',len(changes),'architecture groups;',sum(len(x['created']) for x in changes),'P2 modules')
