import unreal
import json
import time
from pathlib import Path

class GormWalk:
    def __init__(self):
        w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        self.pawn=unreal.GameplayStatics.get_player_pawn(w,0)
        assert self.pawn
        self.route=[(-810,-1950),(-1030,-1900),(-1160,-1830),(-1160,-1790),(-1000,-1850),(-810,-1850),(-810,-1440),(-810,-1215)]
        self.index=1;self.samples=[];self.start=time.monotonic();self.next_sample=0
        self.pawn.set_actor_location(unreal.Vector(*self.route[0],52),False,True)
        self.handle=unreal.register_slate_post_tick_callback(self.tick)
    def tick(self,dt):
        elapsed=time.monotonic()-self.start
        if elapsed<.6:return
        p=self.pawn.get_actor_location();target=self.route[self.index]
        delta=unreal.Vector(target[0]-p.x,target[1]-p.y,0);dist=delta.length()
        if elapsed>=self.next_sample:
            self.samples.append(dict(t=round(elapsed,2),pos=list(p.to_tuple()),waypoint=self.index,mode=str(self.pawn.character_movement.movement_mode)))
            self.next_sample=elapsed+.5
        if dist<28:
            self.index+=1
            if self.index==len(self.route):self.finish(True);return
        if elapsed>45:self.finish(False);return
        if dist>1:self.pawn.add_movement_input(delta/dist,min(1,dist/max(self.pawn.character_movement.max_walk_speed*dt*1.5,1)),False)
    def finish(self,passed):
        unreal.unregister_slate_post_tick_callback(self.handle)
        self.pawn.character_movement.stop_movement_immediately()
        data=dict(passed=passed,reached=self.index,total=len(self.route),samples=self.samples,final_position=list(self.pawn.get_actor_location().to_tuple()))
        out=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Restyle/verification/gorm_walk.json'
        out.write_text(json.dumps(data,indent=2),encoding='utf-8')
        unreal.log('REPAIRED GORM WALK '+str(passed))

p2_walk=GormWalk()
print('P2 WALK STARTED')
