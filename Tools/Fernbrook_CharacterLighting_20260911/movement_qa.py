import unreal,json,time
from pathlib import Path
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world();assert w
p=unreal.GameplayStatics.get_player_pawn(w,0);assert p
expected='/Game/Fernbrook/CharacterLighting/Materials/MI_FB_Player_Lit.MI_FB_Player_Lit'
class LightingQA:
 def __init__(self):
  self.start=time.monotonic();self.origin=p.get_actor_location();self.samples=[];self.done=False
  self.handle=unreal.register_slate_post_tick_callback(self.tick)
 def tick(self,dt):
  elapsed=time.monotonic()-self.start
  sprites=p.get_components_by_class(unreal.PaperFlipbookComponent)
  ok=len(sprites)==8 and all(c.get_material(0) and c.get_material(0).get_path_name()==expected for c in sprites)
  body=next(c for c in sprites if c.get_name()=='Sprite')
  if len(self.samples)==0 or elapsed-self.samples[-1]['t']>0.2:
   self.samples.append({'t':elapsed,'material_ok':ok,'flipbook':body.get_flipbook().get_path_name(),'location':p.get_actor_location().to_tuple()})
  if elapsed<2.5 and ok:p.add_movement_input(unreal.Vector(0,1,0),0.35,False)
  if elapsed>3.5 or not ok:
   p.character_movement.stop_movement_immediately();unreal.unregister_slate_post_tick_callback(self.handle);self.done=True
   distance=(p.get_actor_location()-self.origin).length()
   self.result={'passed':all(s['material_ok'] for s in self.samples) and 70<distance<500,'distance_cm':distance,'samples':self.samples,'materials':8}
   out=Path(unreal.Paths.project_dir()).resolve()/('Docs/Fernbrook_CharacterLighting_20260911/movement_qa_'+STAGE+'.json')
   with out.open('x',encoding='utf8') as f:json.dump(self.result,f,indent=2)
assert not hasattr(unreal,'_fb_char_lighting_qa') or unreal._fb_char_lighting_qa.done
unreal._fb_char_lighting_qa=LightingQA();print('Real movement + flipbook/material persistence QA queued')
