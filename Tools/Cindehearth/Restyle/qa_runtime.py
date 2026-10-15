import unreal
import json
import time
from pathlib import Path
STAGE=globals().get('STAGE','state')
OUT=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Restyle/verification'
OUT.mkdir(exist_ok=True,parents=True)
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert w,'Start PIE first'
ds=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.NUCindehearthDirector)
assert len(ds)==1
d=ds[0]
pawn=unreal.GameplayStatics.get_player_pawn(w,0)
assert pawn

if STAGE=='states':
    report=[]
    auto=d.get_editor_property('auto_play_epilogue');d.set_editor_property('auto_play_epilogue',False)
    for stage in (0,7,8,9,11,12):
        d.set_night(True);d.set_chapter_progress(stage)
        a=unreal.GameplayStatics.get_all_actors_with_tag(w,'Cindehearth')
        count=lambda tag:sum(1 for x in a if x.actor_has_tag(tag) and not x.get_editor_property('hidden'))
        blockers=lambda tag:sum(1 for x in a if x.actor_has_tag(tag) and x.get_actor_enable_collision())
        row=dict(stage=stage,mood=str(d.get_square_mood()),crowd=count('CH.Crowd'),gate=count('CH.Q8.Blocker'),
                 gate_collision=blockers('CH.Q8.Blocker'),hatch=count('CH.Q7.Blocker'),
                 hatch_collision=blockers('CH.Q7.Blocker'),cocoon=count('CH.Q11.Cocoon'),freed=count('CH.Q11.Freed'))
        report.append(row)
        assert row['crowd']==24,row
        assert (row['gate_collision']>0)==(stage<8),row
        assert (row['hatch_collision']>0)==(stage<7),row
        assert (row['freed']>0)==(stage>=11),row
        for resident in (x for x in a if x.actor_has_tag('CH.Resident')):
            c=resident.get_component_by_class(unreal.PaperSpriteComponent)
            assert c and c.get_sprite() and c.get_material(0) and resident.get_actor_bounds(False)[1].z>30,resident.get_name()
    d.set_chapter_progress(0);d.set_night(False);d.set_editor_property('auto_play_epilogue',auto)
    (OUT/'state_qa.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print('STATE QA',json.dumps(report))

elif STAGE.startswith('walk:'):
    mode=STAGE.split(':')[1]
    # Real movement input on every engine tick, not swept teleports across ramps.
    class CindehearthWalkTest:
        def __init__(self,pawn,mode):
            self.pawn=pawn;self.mode=mode;self.start=time.monotonic();self.samples=[];self.next_sample=0
            self.route={'rail':[(15.5,-78,0),(15.5,-42,0),(15.5,-18,3.5),(15.5,18,3.5),(15.5,42,7),(15.5,83,7)],
                        'pinehollow':[(12,-102,0),(12,-125,0),(12,-148,0),(12,-170,0),(12,-190,0)],
                        'stairs':[(-18,-42,0),(-18,-31,3.2),(-18,-27,3.5)],
                        'cemetery':[(65,48,7),(78,55,10.2),(95,65,14),(103,65,14)],
                        'deepvein':[(12,92,7),(12,103,7),(12,127,3),(12,151,3),(12,178,-1),(12,199,-1)],
                        'ashslope':[(-72,12,3.5),(-96,12,3.5),(-102,30,3.5),(-117,30,6),(-126,-8,6),(-142,-8,8.5),(-151,30,8.5),(-167,30,11),(-176,-8,11),(-192,-8,13.5)]}[mode]
            x,y,z=self.route[0];self.index=1
            self.pawn.set_actor_location(unreal.Vector(x*45,y*45,z*45+52),False,True)
            self.handle=unreal.register_slate_post_tick_callback(self.tick)
        def tick(self,dt):
            elapsed=time.monotonic()-self.start
            if elapsed<.8:return
            pos=self.pawn.get_actor_location();target=self.route[self.index]
            delta=unreal.Vector(target[0]*45-pos.x,target[1]*45-pos.y,0)
            dist=delta.length()
            if elapsed>self.next_sample:
                self.samples.append(dict(seconds=round(elapsed,2),pos=[round(pos.x,1),round(pos.y,1),round(pos.z,1)],mode=str(self.pawn.character_movement.movement_mode)))
                self.next_sample=elapsed+2
            if dist<100:
                self.index+=1
                if self.index>=len(self.route):self.finish(True);return
            if elapsed>80:self.finish(False);return
            if dist>1:self.pawn.add_movement_input(delta/dist,min(1,dist/max(self.pawn.character_movement.max_walk_speed*dt*1.5,1)),False)
        def finish(self,ok):
            unreal.unregister_slate_post_tick_callback(self.handle)
            self.pawn.character_movement.stop_movement_immediately()
            pos=self.pawn.get_actor_location()
            report=dict(passed=ok,route=self.mode,waypoints_reached=self.index,total_waypoints=len(self.route),final_pos=[pos.x,pos.y,pos.z],samples=self.samples)
            (OUT/('walk_'+self.mode+'.json')).write_text(json.dumps(report,indent=2),encoding='utf-8')
            unreal.log('CINDEHEARTH WALK '+self.mode+' '+str(ok))
    if mode=='deepvein':d.set_editor_property('auto_play_epilogue',False);d.set_chapter_progress(8)
    ch_walk_test=CindehearthWalkTest(pawn,mode)
    print('WALK STARTED',mode)

elif STAGE=='archive_enter':
    d.set_editor_property('auto_play_epilogue',False);d.set_chapter_progress(7)
    pawn.set_actor_location(unreal.Vector(1300,292.5,215),False,True)
    d.use_archive_ladder()
    print('ARCHIVE ENTER',pawn.get_actor_location())
elif STAGE=='archive_exit':
    print('ARCHIVE FLOOR',pawn.get_actor_location(),pawn.character_movement.current_floor)
    assert pawn.get_actor_location().z<70
    assert pawn.character_movement.current_floor.walkable_floor
    d.use_archive_ladder();print('ARCHIVE EXIT',pawn.get_actor_location())
elif STAGE=='epilogue':
    d.set_editor_property('auto_play_epilogue',False);d.set_chapter_progress(12)
    assert d.play_epilogue()
    print('EPILOGUE START',unreal.GameplayStatics.get_player_controller(w,0).get_view_target())
elif STAGE=='epilogue_verify':
    pc=unreal.GameplayStatics.get_player_controller(w,0)
    print('EPILOGUE END',d.epilogue_played,pc.get_view_target(),pc.is_move_input_ignored(),pc.is_look_input_ignored())
    assert d.epilogue_played
    assert not pc.is_move_input_ignored() and not pc.is_look_input_ignored()
    assert pc.get_view_target()==pawn
