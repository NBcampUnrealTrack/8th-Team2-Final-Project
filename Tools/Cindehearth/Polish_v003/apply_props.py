"""Replace reviewed prop groups, preserving source gameplay colliders and clue anchors."""
import unreal,json,sys,math,importlib
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Polish_v003';sys.path.insert(0,str(root))
import ue_common as C
importlib.reload(C)
assert not unreal.WidgetService.is_pie_running()
assert not (root/'verification/props_applied.json').exists()
layout=json.loads((root.parent/'layout.json').read_text(encoding='utf-8'));changes=[]
spec={
 'Table':('Table',(225,120,48),0), 'Forge':('Forge',(130,160,205),-90),
 'Anvil':('Anvil',(70,85,62),90), 'Notice':('Noticeboard',(32,144,138),90),
 'ShiftBell':('Bell',(78,174,180),90),'ToolSign':('ToolRack',(19,46,48),-90),
 'Laundry':('Laundry',(52,360,172),90),'AssayDesk':('Desk',(55,112,65),90),
 'Bed':('Bed',(105,58,43),90),'LedgerShelf':('Shelf',(36,150,100),90),
 'Grave':('Grave',(117,83,43),0),'StoneNaia':('StoneNaia',(34,62,54),-90),
 'AshNaia':('AshNaia',(29,33,22),-90),'Barricade':('Barricade',(96,260,155),90)}
with unreal.ScopedEditorTransaction('Cindehearth reviewed P2 props and Naia'):
 for p in layout['placements']:
  if p['module'] not in spec:continue
  prefix='CH_'+p['name'];key,size,rotate=spec[p['module']]
  sources=[C.ACTORS[prefix+'_'+k] for k in layout['modules'][p['module']] if prefix+'_'+k in C.ACTORS]
  assert sources,prefix
  anchor=sources[0];origin=list(anchor.get_actor_location().to_tuple());yaw=anchor.get_actor_rotation().yaw
  size=[size[i]*p['scale'][i] for i in range(3)]
  if p['module']=='ToolSign':origin[2]+=67
  if p['name']=='StoneNaiaColony_4':origin[0]+=105 # keep colony separate from the larger sentinel
  a=C.spawn(prefix+'_P3',key,origin,size,yaw+rotate,tags=p['tags']+['CH.Zone.'+p['zone']])
  hidden=[]
  for old in sources:
   # Unique papers remain at their original interactive anchor, exclusively in Q7 archive.
   if p['module']=='AssayDesk' and old.get_actor_label().endswith('_Paper'):continue
   if p['module']=='Forge' and old.get_actor_label().endswith('_Ember'):continue
   C.hide(old,keep_collision=True);hidden.append(old.get_actor_label())
  changes.append(dict(module=p['module'],source=prefix,created=a.get_actor_label(),hidden=hidden))
 # Replace every visible legacy crate and barrel, preserving the exact existing volume.
 for old in list(C.ACTORS.values()):
  if not isinstance(old,unreal.StaticMeshActor):continue
  comp=old.static_mesh_component;mesh=comp.static_mesh
  if not mesh or not comp.is_visible() or old.get_editor_property('hidden'):continue
  path=mesh.get_path_name();key='Crate' if '/SM_Crate.' in path else 'Barrel' if '/SM_Barrel.' in path else None
  if not key:continue
  box=mesh.get_bounding_box();sc=old.get_actor_scale3d().to_tuple()
  size=[(box.max.to_tuple()[i]-box.min.to_tuple()[i])*sc[i] for i in range(3)]
  center,extent=old.get_actor_bounds(False);origin=[center.x,center.y,center.z-extent.z]
  a=C.spawn(old.get_actor_label()+'_P3',key,origin,size,old.get_actor_rotation().yaw,tags=[str(t) for t in old.tags])
  C.hide(old,keep_collision=True);changes.append(dict(module=key,source=old.get_actor_label(),created=a.get_actor_label()))
 # Remove the former floating opaque ember chunks; the pixel flame and light remain.
 ember=C.ACTORS.get('CH_CommunalHearth_10m_Ember')
 if ember:C.hide(ember)
C.save_result('props_applied',changes)
print('P2 PROP GROUPS',len(changes))
