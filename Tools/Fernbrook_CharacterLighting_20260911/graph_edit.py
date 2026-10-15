import unreal,json
from pathlib import Path
B=unreal.BlueprintService;A=unreal.EditorAssetLibrary
P='/Game/Fernbrook/CharacterLighting/Gameplay/BP_FB_FieldCharacter';G='EventGraph'
MI='/Game/Fernbrook/CharacterLighting/Materials/MI_FB_Player_Lit.MI_FB_Player_Lit'
OUT=Path(unreal.Paths.project_dir()).resolve()/'Docs/Fernbrook_CharacterLighting_20260911'
def snapshot():
 s,e=B.describe_graph(P,G);assert s,e;return s
def check(n):
 assert n
 assert any(x.node_id==n for x in B.get_nodes_in_graph(P,G))
 pins=B.get_node_pins(P,G,n)
 print(json.dumps({'node':n,'pins':[{'name':p.pin_name,'input':p.is_input,'default':p.default_value} for p in pins]}));return n
if STAGE=='first':
 s=snapshot();assert not any(n.function_name=='SetMaterial' for n in s.nodes)
 getter=check(B.add_get_variable_node(P,G,'Sprite',2100,-220))
 setter=check(B.add_function_call_node(P,G,'PrimitiveComponent','SetMaterial',2200,0))
 with (OUT/'first_nodes.json').open('x',encoding='utf8') as f:json.dump({'getter':getter,'setter':setter},f)
elif STAGE=='wire_first':
 s=snapshot();getter=next(n for n in s.nodes if n.node_title=='Get Sprite').node_id
 setter=next(n for n in s.nodes if n.function_name=='SetMaterial').node_id
 prev=next(n for n in s.nodes if n.function_name=='SetFieldOfView').node_id
 assert B.set_node_pin_value(P,G,setter,'ElementIndex','0')
 assert B.set_node_pin_value(P,G,setter,'Material',MI)
 for src,out,dst,pin in [(getter,'Sprite',setter,'self'),(prev,'then',setter,'execute')]:
  assert B.connect_nodes(P,G,src,out,dst,pin)
  assert any(e.source_node_id==src and e.source_pin_name==out and e.target_node_id==dst and e.target_pin_name==pin for e in snapshot().edges)
 print('First material setter connected')
elif STAGE=='layers':
 s=snapshot();assert sum(n.function_name=='SetMaterial' for n in s.nodes)==1
 prev=next(n.node_id for n in s.nodes if n.function_name=='SetMaterial')
 created=[]
 for i,name in enumerate(['EquipSprite','EquipSpriteGloves','EquipSpriteBoots','EquipSpriteLegs','EquipSpriteShoulder','HairSprite','FaceSprite']):
  x=2550+i*350
  getter=check(B.add_get_variable_node(P,G,name,x,-220));created.append(getter)
  setter=check(B.add_function_call_node(P,G,'PrimitiveComponent','SetMaterial',x+70,0));created.append(setter)
  assert B.set_node_pin_value(P,G,setter,'ElementIndex','0')
  assert B.set_node_pin_value(P,G,setter,'Material',MI)
  for src,out,dst,pin in [(getter,name,setter,'self'),(prev,'then',setter,'execute')]:
   assert B.connect_nodes(P,G,src,out,dst,pin)
   assert any(e.source_node_id==src and e.source_pin_name==out and e.target_node_id==dst and e.target_pin_name==pin for e in snapshot().edges)
  prev=setter
 s=snapshot()
 ids=[n.node_id for n in s.nodes if n.function_name=='SetMaterial' or n.node_title in ['Get '+q for q in ['Sprite','EquipSprite','EquipSpriteGloves','EquipSpriteBoots','EquipSpriteLegs','EquipSpriteShoulder','HairSprite','FaceSprite']]]
 assert B.add_comment_around_nodes(P,G,'Fernbrook-only sprite lighting: AmbientLift 0.45 in MI_FB_Player_Lit. Apply once at BeginPlay to body + equipment layers; preserve alpha, normals, original art and camera. No Tick cost.',ids,50)
 r=B.compile_blueprint(P);assert r.success,r.errors
 assert A.save_asset(P)
 s=snapshot();assert s.status.is_up_to_date and not s.status.has_compile_errors
 assert sum(n.function_name=='SetMaterial' for n in s.nodes)==8
 result={'nodes':[{'id':n.node_id,'title':n.node_title,'fn':n.function_name,'pins':[{'name':p.pin_name,'input':p.is_input,'default':p.default_value} for p in n.pins]} for n in s.nodes],'edges':[{'src':e.source_node_id,'src_title':e.source_node_title,'out':e.source_pin_name,'dst':e.target_node_id,'dst_title':e.target_node_title,'in':e.target_pin_name} for e in s.edges],'compiled':r.success,'errors':list(r.errors),'warnings':list(r.warnings)}
 with (OUT/'graph_verified.json').open('x',encoding='utf8') as f:json.dump(result,f,indent=2)
 print(json.dumps({'compiled':r.success,'errors':list(r.errors),'warnings':list(r.warnings),'setters':8,'edges':len(s.edges)}))
elif STAGE=='assign':
 mode='/Game/Fernbrook/CharacterLighting/Gameplay/BP_FB_GameMode'
 assert B.set_property(mode,'DefaultPawnClass',A.load_blueprint_class(P).get_path_name())
 r=B.compile_blueprint(mode);assert r.success,r.errors
 assert A.save_asset(mode)
 cdo=unreal.get_default_object(A.load_blueprint_class(mode))
 assert cdo.get_editor_property('default_pawn_class')==A.load_blueprint_class(P)
 w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
 assert w.get_name()=='L_Fernbrook_Entrance'
 settings=w.get_world_settings()
 assert settings.get_editor_property('default_game_mode').get_path_name()=='/Game/Libregrove/Level_V8/Gameplay/BP_LG8_GameMode.BP_LG8_GameMode_C'
 settings.set_editor_property('default_game_mode',A.load_blueprint_class(mode))
 assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
 print(json.dumps({'mode':settings.get_editor_property('default_game_mode').get_path_name(),'pawn':cdo.get_editor_property('default_pawn_class').get_path_name()}))
else:raise RuntimeError(STAGE)
