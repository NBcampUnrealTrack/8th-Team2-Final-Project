import unreal,time,json
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Restyle/verification'
class Stability:
    def __init__(self):
        self.started=time.monotonic();self.ticks=0;self.samples=[];self.captured=False
        self.handle=unreal.register_slate_post_tick_callback(self.tick)
    def tick(self,dt):
        self.ticks+=1;t=time.monotonic()-self.started
        if not self.samples or t-self.samples[-1]['elapsed']>15:
            self.samples.append(dict(elapsed=round(t,2),ticks=self.ticks))
        if t>30 and not self.captured:
            self.captured=True
            self.capture=json.loads(unreal.ScreenshotService.capture_from_pose(unreal.Vector(),unreal.Rotator(),'CH_Restyle_reload_stable',0,False,True))
        if t<100:return
        unreal.unregister_slate_post_tick_callback(self.handle)
        a=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
        trees=[x for x in a if isinstance(x,unreal.StaticMeshActor) and x.static_mesh_component.static_mesh and 'CH_PineVolume_Closed_v005.' in x.static_mesh_component.static_mesh.get_path_name()]
        assert len(trees)==95 and self.capture['success']
        report=dict(passed=True,editor_ticks=self.ticks,elapsed_seconds=round(t,2),tree_count=len(trees),capture=self.capture,samples=self.samples,forced_material_diagnostics=False)
        (root/'editor_stability.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
        unreal.log('CINDEHEARTH RELOAD STABILITY PASS')
ch_stability=Stability()
print('100 SECOND RELOAD OBSERVATION STARTED; NO MATERIAL RECOMPILE')
