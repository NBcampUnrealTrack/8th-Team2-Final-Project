"""Real CharacterMovement route tests; no teleporting between route waypoints."""
import unreal,json,time,math,runpy,traceback
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Docs/Libregrove/Level_V8_20260908/GameplayQA'
OUT.mkdir(exist_ok=True);RUN=globals().get('RUN','001');SELECT=globals().get('SELECT',[])
D=runpy.run_path(str(ROOT/'Tools/Libregrove/level_v8_design.py'));base=D['base']
roads={r['id']:r['points'] for r in D['ROADS']}
def route(points):return [[-x,y,base(x,y)] for x,y in points]
routes={
 'central_shrine':route([(0,-3170),(0,-1890),(0,-1120),(0,-1050),(0,-720),(0,-385),(0,-270),(0,220),(-190,440),(-250,720),(-190,1020),(0,1280),(0,1800),(0,1920),(0,2000),(0,2130),(0,2280),(0,2445),(0,2500)]),
 'west_bridge_mill':route([(0,-1890),(-780,-1890),(-1620,-1760),(-2100,-1400),(-2180,-1165),(-2138,-1099),(-2020,-905),(-1820,-590),(-1620,-270),(-1390,-55),(-1650,-170),(-1620,120),(-1660,420),(-1660,1000),(-1710,1350)]),
 'east_bridge_workshop':route(roads['LowerEast']+[(1900,-680)]+roads['EastBank'][:3]+roads['WorkshopLane'][1:]),
 'lower_loop':route(roads['LowerSouth']+[(1900,-1700),(930,-1910),(0,-1890)]),
 'west_exit':route([(-2020,-905),(-2220,-1160),(-2600,-1200),(-3200,-1100),(-3650,-920),(-4000,-720)]),
 'southeast_exit':route(roads['SouthEastExit']),
}
for name,points in routes.items():
    for p in points:
        for b in D['BRIDGES']:
            dx=-p[0]-b['x'];dy=p[1]-b['y'];ang=math.radians(b['yaw'])
            lx=dx*math.cos(ang)+dy*math.sin(ang);ly=-dx*math.sin(ang)+dy*math.cos(ang)
            if abs(lx)<b['width']/2+40 and abs(ly)<b['length']/2+75:p[2]=55
        if name=='central_shrine' and -1100<p[1]<-370:p[2]=60
        if name=='west_bridge_mill' and abs(p[0]-1820)<10 and abs(p[1]+590)<10:p[2]=66
        if name=='east_bridge_workshop' and abs(p[1]+680)<10:p[2]=66
        if name=='central_shrine' and 1950<p[1]<2426:p[2]=30+(min(13,int((p[1]-1950)/34))+1)*195/14
if SELECT:routes={n:routes[n] for n in SELECT}
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world();assert world
pawn=unreal.GameplayStatics.get_player_pawn(world,0);assert pawn
class Walk:
 def __init__(self):
  self.names=list(routes);self.r=-1;self.rows=[];self.handle=unreal.register_slate_post_tick_callback(self.tick);self.next_route()
 def next_route(self):
  self.r+=1
  if self.r==len(self.names):
   unreal.unregister_slate_post_tick_callback(self.handle);pawn.character_movement.stop_movement_immediately()
   with (OUT/('summary_'+RUN+'.json')).open('x',encoding='utf8') as f:json.dump(self.rows,f,indent=2)
   unreal.log('LG8 ROUTE QA FINISHED '+RUN);return
  self.name=self.names[self.r];self.points=routes[self.name];self.i=0;self.samples=[];self.reached=[];self.start=time.monotonic();self.progress=self.start;self.best=1e20;self.last_sample=0
  q=self.points[0];pawn.character_movement.stop_movement_immediately();pawn.set_actor_location(unreal.Vector(q[0],q[1],q[2]+53),False,True)
 def finish(self,passed,reason=''):
  pawn.character_movement.stop_movement_immediately()
  row={'route':self.name,'passed':passed,'reason':reason,'reached':self.reached,'points':self.points,'samples':self.samples,'movement':'add_movement_input; only route start teleports','tolerance_horizontal_cm':32,'floor_tolerance_cm':45}
  with (OUT/(self.name+'_'+RUN+'.json')).open('x',encoding='utf8') as f:json.dump(row,f,indent=2)
  self.rows.append({'route':self.name,'passed':passed,'reason':reason,'reached':len(self.reached),'total':len(self.points)})
  unreal.log('LG8 WALK '+self.name+' '+str(passed));self.next_route()
 def tick(self,dt):
  try:
   now=time.monotonic()
   if now-self.start<1:return
   pos=pawn.get_actor_location();q=self.points[self.i];delta=unreal.Vector(q[0]-pos.x,q[1]-pos.y,0);distance=delta.length();grounded=bool(pawn.character_movement.current_floor.walkable_floor);floor_error=pos.z-48-q[2]
   row={'time':round(now-self.start,2),'waypoint':self.i,'pos':pos.to_tuple(),'distance':distance,'walkable':grounded,'floor_error':floor_error}
   if now-self.last_sample>.5:self.samples.append(row);self.last_sample=now
   if distance<self.best-5:self.best=distance;self.progress=now
   if distance<32 and grounded and abs(floor_error)<45:
    self.reached.append(row);self.i+=1;self.best=1e20;self.progress=now
    if self.i==len(self.points):self.finish(True)
    return
   if now-self.progress>12:self.finish(False,'no_progress_or_wrong_floor_12s');return
   if now-self.start>110 or pos.z<-800:self.finish(False,'timeout_or_fall');return
   if distance>1:pawn.add_movement_input(delta/distance,min(1.,distance/max(pawn.character_movement.max_walk_speed*dt*1.5,1.)),False)
  except Exception:self.finish(False,traceback.format_exc())
assert not hasattr(unreal,'_lg8_walk') or len(unreal._lg8_walk.rows)==len(unreal._lg8_walk.names)
unreal._lg8_walk=Walk();print('Queued',len(routes),'real movement routes',RUN)
