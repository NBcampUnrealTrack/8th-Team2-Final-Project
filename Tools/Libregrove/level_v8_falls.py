"""New waterfall material and its scene ribbons, never edits source water."""
import unreal,json,math
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Docs/Libregrove/Level_V8_20260908'
PATH='/Game/Libregrove/Level_V8/Materials/M_LG8_Waterfall'
lib=unreal.MaterialEditingLibrary
OP=globals().get('OP','inspect')
if OP in ['create','resume']:
    if OP=='create':
        assert not unreal.EditorAssetLibrary.does_asset_exist(PATH)
        mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_LG8_Waterfall','/Game/Libregrove/Level_V8/Materials',unreal.Material,unreal.MaterialFactoryNew())
    else:
        mat=unreal.load_asset(PATH)
        nodes=list(lib.get_material_expressions(mat));assert len(nodes)==4
    mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('two_sided',True)
    def node(cls,x,y):
        return next(n for n in nodes if isinstance(n,cls)) if OP=='resume' else lib.create_material_expression(mat,cls,x,y)
    uv=node(unreal.MaterialExpressionTextureCoordinate,-800,-100)
    time=node(unreal.MaterialExpressionTime,-800,100)
    tex=node(unreal.MaterialExpressionTextureObjectParameter,-800,300)
    tex.set_editor_property('parameter_name','FlowNoise');tex.set_editor_property('texture',unreal.load_asset('/Game/Village/Textures/Pixel/T_Water_Ripple'))
    shader=node(unreal.MaterialExpressionCustom,-400,0)
    shader.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    inputs=[]
    for name in ['UV','Time','NoiseTex']:
        ci=unreal.CustomInput();ci.set_editor_property('input_name',name);inputs.append(ci)
    shader.set_editor_property('inputs',inputs)
    shader.set_editor_property('code','''
float2 q = float2(UV.x*3.0, UV.y*1.8-Time*.50);
float a = Texture2DSample(NoiseTex,NoiseTexSampler,q).r;
float b = Texture2DSample(NoiseTex,NoiseTexSampler,q*float2(2.8,.7)+float2(.17,-Time*.20)).r;
float ribbons = pow(saturate(sin(UV.x*73.0 + a*1.4)*.5+.5),5.0);
float edge = smoothstep(0.,.12,UV.x)*smoothstep(0.,.12,1.-UV.x);
float value = saturate(a*.55 + b*.35+ribbons*.35);
float3 color = lerp(float3(.075,.33,.40),float3(.65,.88,.94),value);
return float4(color,edge*saturate(.48+value*.45));
''')
    for node,name in [(uv,'UV'),(time,'Time'),(tex,'NoiseTex')]:assert lib.connect_material_expressions(node,'',shader,name)
    rgb=lib.create_material_expression(mat,unreal.MaterialExpressionComponentMask,-120,-70)
    for n,v in [('r',True),('g',True),('b',True),('a',False)]:rgb.set_editor_property(n,v)
    alpha=lib.create_material_expression(mat,unreal.MaterialExpressionComponentMask,-120,150)
    for n,v in [('r',False),('g',False),('b',False),('a',True)]:alpha.set_editor_property(n,v)
    assert lib.connect_material_expressions(shader,'',rgb,'Input');assert lib.connect_material_expressions(shader,'',alpha,'Input')
    assert lib.connect_material_property(rgb,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    assert lib.connect_material_property(alpha,'',unreal.MaterialProperty.MP_OPACITY)
    assert unreal.EditorAssetLibrary.save_asset(PATH)
    graph=json.loads(unreal.MaterialNodeService.export_material_graph(PATH))
    with (OUT/'waterfall_graph.json').open('x',encoding='utf8') as f:json.dump(graph,f,indent=2)
    print('Created unassigned waterfall material',len(graph['expressions']))
elif OP=='finish_connections':
    mat=unreal.load_asset(PATH);nodes=list(lib.get_material_expressions(mat));assert len(nodes)==6
    shader=next(n for n in nodes if isinstance(n,unreal.MaterialExpressionCustom))
    masks=[n for n in nodes if isinstance(n,unreal.MaterialExpressionComponentMask)]
    rgb=next(n for n in masks if n.get_editor_property('r'));alpha=next(n for n in masks if n.get_editor_property('a'))
    assert lib.connect_material_expressions(shader,'',rgb,'')
    assert lib.connect_material_expressions(shader,'',alpha,'')
    assert lib.connect_material_property(rgb,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    assert lib.connect_material_property(alpha,'',unreal.MaterialProperty.MP_OPACITY)
    assert unreal.EditorAssetLibrary.save_asset(PATH)
    graph=json.loads(unreal.MaterialNodeService.export_material_graph(PATH))
    assert len(graph['connections'])==5 and len(graph['output_connections'])==2
    with (OUT/'waterfall_graph_final.json').open('x',encoding='utf8') as f:json.dump(graph,f,indent=2)
    print('Completed waterfall graph, six expressions/five wires/two outputs')
elif OP=='compile':
    print(unreal.MaterialService.compile_material(PATH))
elif OP=='place':
    # Set plane width along world X and height along world Z; two-sided ribbons.
    E=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='L_Libregrove'
    labels={a.get_actor_label() for a in E.get_all_level_actors()};mat=unreal.load_asset(PATH)
    for i,(x,y,w,h,z,roll) in enumerate([(2340,2070,150,455,172,90),(2280,2060,50,410,162,93),(2410,2080,44,435,167,87)]):
        name='LG8_WaterfallRibbon_%d'%i;assert name not in labels
        a=E.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(x,y,z),unreal.Rotator(roll=roll,pitch=0,yaw=0))
        a.set_actor_label(name);a.set_folder_path('Libregrove/Water/Waterfall');a.tags=[unreal.Name('Libregrove.V8')]
        c=a.static_mesh_component;assert c.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Plane'))
        a.set_actor_scale3d(unreal.Vector(w/100,h/100,1));c.set_material(0,mat);c.set_collision_profile_name('NoCollision');c.set_cast_shadow(False)
    assert L.save_current_level();print('Placed three waterfall ribbons')
else:raise RuntimeError(OP)
