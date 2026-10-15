"""Align west-side paths to the traversable bank/bridge axis, preserving other areas."""
import unreal,json,math,runpy,shutil,hashlib
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Docs/Libregrove/Level_V8_20260908'
STAGE=globals().get('STAGE','prepare');D=runpy.run_path(str(ROOT/'Tools/Libregrove/level_v8_design.py'))
NEW={'LowerWest':[(0,-1890),(-780,-1890),(-1620,-1760),(-2100,-1400),(-2180,-1165),(-2138,-1099),(-2020,-905)],'WestExit':[(-2020,-905),(-2220,-1160),(-2600,-1200),(-3200,-1100),(-3650,-920),(-4000,-720)],'MillFootpath':[(-1390,-55),(-1650,-170),(-1620,120),(-1660,420),(-1660,1000),(-1710,1350)]}
def save(name,value):
 with (OUT/name).open('x',encoding='utf8') as f:json.dump(value,f,indent=2)
if STAGE=='prepare':
 assert not unreal.WidgetService.is_pie_running()
 src=ROOT/'Content/Libregrove/Level_V8/Maps/L_Libregrove.umap';backup=ROOT/'Saved/LibregroveV8Backup/BeforeWestRoads_20260909.umap';assert not backup.exists();shutil.copy2(src,backup)
 old={r['id']:r['points'] for r in D['ROADS'] if r['id'] in NEW}
 for r in D['ROADS']:
  if r['id'] in NEW:r['points']=NEW[r['id']]
 n=253;o=-4536;s=36;x0=math.floor((1300-o)/s);x1=math.ceil((4200-o)/s);y0=math.floor((-1950-o)/s);y1=math.ceil((1400-o)/s);w=x1-x0+1;h=y1-y0+1
 before={k:list(unreal.LandscapeService.get_weights_in_region('LG8_Terrain',k,x0,y0,w,h)) for k in ['Grass','Dirt','Cobblestone','Moss']};assert all(len(a)==w*h for a in before.values())
 after={k:[] for k in before}
 for j in range(h):
  y=o+(y0+j)*s
  for i in range(w):
   x=-(o+(x0+i)*s);c,d=D['roadweight'](x,y);rd=D['distance'](x,y,D['RIVER']);d=max(d,(1-D['smooth'](170,330,rd))*.85)
   for b in D['BUILDINGS']:d=max(d,(1-D['smooth'](145*b['scale'],260*b['scale'],math.hypot(x-b['x'],y-b['y'])))*.92)
   m=max(D['smooth'](2300,3600,abs(x)),D['smooth'](2800,3600,abs(y)))*.75;d*=1-c;m*=(1-c)*(1-d);g=max(0,1-c-d-m);total=g+d+c+m
   for k,v in zip(after,[g,d,c,m]):after[k].append(v/total)
 save('west_roads_patch.json',{'rect':[x0,y0,w,h],'old_routes':old,'new_routes':NEW,'before':before,'after':after,'backup':str(backup),'backup_sha256':hashlib.sha256(backup.read_bytes()).hexdigest()});print('Prepared west path patch',w,h)
elif STAGE.startswith('paint_'):
 assert not unreal.WidgetService.is_pie_running()
 k=STAGE[6:];data=json.loads((OUT/'west_roads_patch.json').read_text(encoding='utf8'));x,y,w,h=data['rect']
 assert unreal.LandscapeService.set_weights_in_region('LG8_Terrain',k,x,y,w,h,data['after'][k]);assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level();print('Painted west route',k)
elif STAGE=='clear':
 assert not unreal.WidgetService.is_pie_running();targets=[];actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
 for a in actors:
  for comp in a.get_components_by_class(unreal.InstancedStaticMeshComponent):
   mesh=comp.static_mesh
   if not mesh:continue
   for i in range(comp.get_instance_count()):
    t=comp.get_instance_transform(i,True);p=t.translation
    if p.x<1400:continue
    is_tree='BroadleafTree' in mesh.get_path_name();radius=270 if is_tree and t.scale3d.x>.7 else 180 if is_tree else 115
    dist=min(D['distance'](-p.x,p.y,pts) for pts in NEW.values())
    if dist<radius:targets.append({'mesh':mesh.get_path_name(),'loc':p.to_tuple(),'rotation':t.rotation.to_tuple(),'scale':t.scale3d.to_tuple()})
 save('west_roads_foliage_backup.json',targets)
 count=0
 for row in targets:
  x,y,z=row['loc'];r=unreal.FoliageService.remove_foliage_in_radius(row['mesh'],x,y,4.);assert r.success,r.error_message;count+=r.instances_removed
 assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
 save('west_roads_foliage_result.json',{'targets':len(targets),'removed':count,'recoverable_map':'Saved/LibregroveV8Backup/BeforeWestRoads_20260909.umap'});print('Cleared west path foliage',count)
else:raise RuntimeError(STAGE)
