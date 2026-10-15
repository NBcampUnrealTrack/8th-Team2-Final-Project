"""Level-only pawn camera setup via supported runtime Blueprint nodes."""
import unreal,json
from pathlib import Path
B=unreal.BlueprintService;A=unreal.EditorAssetLibrary
P='/Game/Libregrove/Level_V8/Gameplay/BP_LG8_FieldCharacter';G='EventGraph'
OUT=Path(unreal.Paths.project_dir()).resolve()/'Docs/Libregrove/Level_V8_20260908'
STAGE=globals().get('STAGE','discover')
def dump(name,data):
    with (OUT/name).open('x',encoding='utf8') as f:json.dump(data,f,indent=2)
def verify_node(n):
    assert n
    found=next(q for q in B.get_nodes_in_graph(P,G) if q.node_id==n)
    pins=B.get_node_pins(P,G,n)
    print(json.dumps({'id':n,'title':found.node_title,'pins':[p.pin_name for p in pins]}));return n
if STAGE=='discover':
    result={}
    for term in ['Set Relative Rotation','Set Absolute','Set Field Of View','Target Arm Length','Camera Lag','BeginPlay']:
        result[term]=[{'title':n.display_name,'key':n.spawner_key} for n in B.discover_nodes(P,term,'',30)]
    result['override']=[{'name':f.function_name,'owner':f.owner_class,'already':f.already_overridden} for f in B.list_overridable_functions(P) if 'BeginPlay' in f.function_name]
    result['nodes']=[{'id':n.node_id,'title':n.node_title} for n in B.get_nodes_in_graph(P,G)]
    dump('camera_discovery.json',result);print(json.dumps(result))
elif STAGE=='variables':
    for name,typ,value in [('LG_CameraRotation','Rotator','(Pitch=-30,Yaw=90,Roll=0)'),('LG_CameraDistance','float','1900'),('LG_CameraFOV','float','38')]:
        assert B.add_variable(P,name,typ,value)
    result=B.compile_blueprint(P);assert result.success,result.errors
    assert A.save_asset(P);print('Three camera controls created')
elif STAGE.startswith('get_'):
    name=STAGE[4:];verify_node(B.add_get_variable_node(P,G,name,globals().get('X',100),globals().get('Y',-250)))
elif STAGE.startswith('fn_'):
    fn=STAGE[3:];owner='SceneComponent' if fn in ['K2_SetRelativeRotation','SetAbsolute'] else 'CameraComponent' if fn=='SetFieldOfView' else 'KismetSystemLibrary'
    verify_node(B.add_function_call_node(P,G,owner,fn,globals().get('X',350),globals().get('Y',0)))
elif STAGE=='snapshot':
    snapshot,err=B.describe_graph(P,G);assert snapshot,err
    result={'nodes':[{'id':n.node_id,'title':n.node_title,'fn':n.function_name,'pins':[{'name':p.pin_name,'in':p.is_input,'default':p.default_value} for p in n.pins]} for n in snapshot.nodes], 'edges':[{'source':e.source_node_id,'out':e.source_pin_name,'target':e.target_node_id,'in':e.target_pin_name} for e in snapshot.edges]}
    dump('camera_nodes_pre_wire.json',result);print(json.dumps(result))
elif STAGE=='wire':
    snapshot,err=B.describe_graph(P,G);assert snapshot and not snapshot.edges,err
    def fn(name):return next(n.node_id for n in snapshot.nodes if n.function_name==name)
    def get(name):return next(n.node_id for n in snapshot.nodes if n.node_title=='Get '+name)
    begin=next(n.node_id for n in snapshot.nodes if 'BeginPlay' in n.node_title)
    absolute=fn('SetAbsolute');rot=fn('K2_SetRelativeRotation');dist=fn('SetDoublePropertyByName');lag=fn('SetBoolPropertyByName');fov=fn('SetFieldOfView')
    assert B.set_node_pin_value(P,G,absolute,'bNewAbsoluteRotation','true')
    assert B.set_node_pin_value(P,G,dist,'PropertyName','TargetArmLength')
    assert B.set_node_pin_value(P,G,lag,'PropertyName','bEnableCameraLag')
    assert B.set_node_pin_value(P,G,lag,'Value','false')
    edges=[(begin,'then',absolute,'execute'),(absolute,'then',rot,'execute'),(rot,'then',dist,'execute'),(dist,'then',lag,'execute'),(lag,'then',fov,'execute'),(get('SpringArm'),'SpringArm',absolute,'self'),(get('SpringArm'),'SpringArm',rot,'self'),(get('SpringArm'),'SpringArm',dist,'Object'),(get('SpringArm'),'SpringArm',lag,'Object'),(get('LG_CameraRotation'),'LG_CameraRotation',rot,'NewRotation'),(get('LG_CameraDistance'),'LG_CameraDistance',dist,'Value'),(get('Camera'),'Camera',fov,'self'),(get('LG_CameraFOV'),'LG_CameraFOV',fov,'InFieldOfView')]
    for src,out,dst,pin in edges:
        assert B.connect_nodes(P,G,src,out,dst,pin)
        check,error=B.describe_graph(P,G);assert check,error
        assert any(e.source_node_id==src and e.source_pin_name==out and e.target_node_id==dst and e.target_pin_name==pin for e in check.edges)
    result=B.compile_blueprint(P);assert result.success,result.errors
    check,error=B.describe_graph(P,G);assert check.status.is_up_to_date and not check.status.has_compile_errors
    B.add_comment_around_nodes(P,G,'Libregrove fixed-camera setup: runtime instance only. Original native templates remain unchanged.',[n.node_id for n in check.nodes if n.node_id not in [x.node_id for x in check.nodes if x.function_name=='PrintString' or 'Overlap' in x.node_title or 'Tick' in x.node_title]],60)
    assert A.save_asset(P)
    cdo=unreal.get_default_object(A.load_blueprint_class(P));values={k:str(cdo.get_editor_property(k)) for k in ['LG_CameraRotation','LG_CameraDistance','LG_CameraFOV']}
    dump('camera_graph_verified.json',{'edges':edges,'compile_success':result.success,'errors':list(result.errors),'defaults':values,'nodes':[{'id':n.node_id,'title':n.node_title} for n in check.nodes]});print(json.dumps(values))
elif STAGE=='gamemode':
    mode='/Game/Libregrove/Level_V8/Gameplay/BP_LG8_GameMode'
    assert B.set_property(mode,'DefaultPawnClass',A.load_blueprint_class(P).get_path_name())
    result=B.compile_blueprint(mode);assert result.success,result.errors;assert A.save_asset(mode)
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world();assert world.get_name()=='L_Libregrove'
    world.get_world_settings().set_editor_property('default_game_mode',A.load_blueprint_class(mode));world.get_world_settings().set_editor_property('kill_z',-20000)
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    print('Level-scoped game mode assigned; live pawn camera awaits PIE validation')
else:raise RuntimeError(STAGE)
