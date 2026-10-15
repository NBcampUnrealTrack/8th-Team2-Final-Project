import unreal,json,time
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Restyle/verification'
poses=[('overview',(0,-8600,5400),(0,400,0),53),('arrival',(250,-5500,770),(250,-2900,220),40),('square',(1550,-1500,930),(-300,0,160),43),('houses',(550,-3700,650),(-700,-2000,160),43),('mine',(1750,800,970),(600,3500,400),45),('pine',(2600,-8000,2200),(0,-6100,-150),50)]
state={'i':0,'start':0,'results':[],'phase':0}
def tick(dt):
    if state['i']>=len(poses):
        unreal.unregister_slate_post_tick_callback(state['handle'])
        (root/'review_captures.json').write_text(json.dumps(state['results'],indent=2),encoding='utf-8');return
    n,p,t,f=poses[state['i']];p=unreal.Vector(*p);rot=unreal.MathLibrary.find_look_at_rotation(p,unreal.Vector(*t))
    if state['phase']==0:
        unreal.EditorLevelLibrary.set_level_viewport_camera_info(p,rot)
        state['start']=time.monotonic();state['phase']=1
    elif time.monotonic()-state['start']>1.1:
        # First draw warms the view's occlusion history after FOV changes.
        unreal.ScreenshotService.capture_from_pose(p,rot,'CH_Restyle_warmup',f,False,False)
        state['phase']=2;state['start']=time.monotonic()
    elif state['phase']==2 and time.monotonic()-state['start']>.4:
        state['results'].append(json.loads(unreal.ScreenshotService.capture_from_pose(p,rot,'CH_Restyle_review_'+n,f,False,False)))
        state['i']+=1;state['phase']=0
state['handle']=unreal.register_slate_post_tick_callback(tick)
print('REVIEW CAPTURE QUEUED')
