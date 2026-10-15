"""Source-preserving, adjustable HD-2D material family. Run stages separately."""
import unreal,json,hashlib
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Docs/Libregrove/Level_V8_20260908/HD2D'
OUT.mkdir(exist_ok=True)
BASE='/Game/Libregrove/Level_V8/HD2D';MPC=BASE+'/MPC_LG8_HD2D';FUNC=BASE+'/MF_LG8_HD2D_Color'
SURFACE=BASE+'/M_LG8_HD2D_Surface'
E=unreal.MaterialEditingLibrary;A=unreal.EditorAssetLibrary;N=unreal.MaterialNodeService;C=unreal.MaterialParameterCollectionService
STAGE=globals().get('STAGE','inspect')
DEFAULTS={'StyleStrength':1.0,'PixelBlend':.90,'TextureDensity':1.0,'PaletteSteps':24.,'PosterizeStrength':.55,'Saturation':.93,'Contrast':1.07,'ShadowLift':.025,'RoughnessBias':.02,'NormalStrength':.55,'EmissiveMultiplier':1.,'WorldPixelSizeCm':3.}
def save(name,obj):
    with (OUT/name).open('x',encoding='utf8') as f:json.dump(obj,f,indent=2)
def node(mat,cls,x=0,y=0):return E.create_material_expression(mat,cls,x,y)
def link(src,dst,pin='',output=''):assert E.connect_material_expressions(src,output,dst,pin),(src.get_name(),dst.get_name(),pin)
def scalar(mat,name,value,group='Per Material',x=-600,y=0):
    n=node(mat,unreal.MaterialExpressionScalarParameter,x,y)
    n.set_editor_property('parameter_name',name);n.set_editor_property('default_value',value);n.set_editor_property('group',group);return n
def vector(mat,name,value,group='Per Material',x=-600,y=0):
    n=node(mat,unreal.MaterialExpressionVectorParameter,x,y)
    n.set_editor_property('parameter_name',name);n.set_editor_property('default_value',unreal.LinearColor(*value));n.set_editor_property('group',group);return n
def custom(mat,inputs,code,output=unreal.CustomMaterialOutputType.CMOT_FLOAT3,x=0,y=0):
    n=node(mat,unreal.MaterialExpressionCustom,x,y);n.set_editor_property('output_type',output)
    ins=[]
    for name,source in inputs.items():
        ci=unreal.CustomInput();ci.set_editor_property('input_name',name);ins.append(ci)
    n.set_editor_property('inputs',ins);n.set_editor_property('code',code)
    for name,source in inputs.items():link(source,n,name)
    return n
def collection(mat,name,x=-1400,y=0):
    before={n.get_path_name() for n in E.get_material_expressions(mat)}
    N.create_collection_parameter(mat.get_path_name(),MPC,name,x,y)
    nodes=[n for n in E.get_material_expressions(mat) if n.get_path_name() not in before];assert len(nodes)==1
    return nodes[0]
def style(mat,color,influence=1,x=700,y=0):
    call=node(mat,unreal.MaterialExpressionMaterialFunctionCall,x,y);assert call.set_material_function(unreal.load_asset(FUNC))
    link(color,call,'SourceColor')
    for i,name in enumerate(['Saturation','Contrast','PaletteSteps','PosterizeStrength','ShadowLift','GlobalTint']):
        link(collection(mat,name,x-950,y+250+i*120),call,name)
    strength=custom(mat,{'Global':collection(mat,'StyleStrength',x-900,y-200),'Local':scalar(mat,'StyleInfluence',influence,x=x-900,y=y-340)},'return saturate(Global*Local);',unreal.CustomMaterialOutputType.CMOT_FLOAT1,x-350,y-180)
    link(strength,call,'Strength');return call
def output(mat,n,prop,out=''):assert E.connect_material_property(n,out,prop)

if STAGE=='inspect':
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors();mats={}
    for a in actors:
        for comp in a.get_components_by_class(unreal.StaticMeshComponent):
            for i in range(comp.get_num_materials()):
                mat=comp.get_material(i)
                if mat:mats[mat.get_path_name()]=mat
        if isinstance(a,unreal.Landscape):
            m=a.get_editor_property('landscape_material');mats[m.get_path_name()]=m
    info=[]
    for path,m in mats.items():
        root=m
        while isinstance(root,unreal.MaterialInstance):root=root.get_editor_property('parent')
        rec={'path':path,'root':root.get_path_name(),'scalars':{},'vectors':{},'textures':{}}
        if isinstance(m,unreal.MaterialInstanceConstant):
            for name in E.get_scalar_parameter_names(root):rec['scalars'][str(name)]=E.get_material_instance_scalar_parameter_value(m,name)
            for name in E.get_vector_parameter_names(root):
                v=E.get_material_instance_vector_parameter_value(m,name);rec['vectors'][str(name)]=[v.r,v.g,v.b,v.a]
            for name in E.get_texture_parameter_names(root):
                t=E.get_material_instance_texture_parameter_value(m,name);rec['textures'][str(name)]=t.get_path_name() if t else None
        else:
            for n in E.get_material_expressions(root):
                if isinstance(n,(unreal.MaterialExpressionTextureObjectParameter,unreal.MaterialExpressionTextureSampleParameter2D)):
                    tex=n.get_editor_property('texture');rec['textures'][str(n.get_editor_property('parameter_name'))]=tex.get_path_name() if tex else None
        info.append(rec)
    save('source_materials.json',info)
    print(json.dumps([{'path':r['path'],'root':r['root'],'textures':r['textures']} for r in info],indent=2))
elif STAGE=='shared':
    assert not A.does_asset_exist(MPC) and not A.does_asset_exist(FUNC)
    assert C.create_mpc(MPC)
    for name,value in DEFAULTS.items():assert C.add_scalar_parameter(MPC,name,value)
    assert C.add_vector_parameter(MPC,'GlobalTint',unreal.LinearColor(1.02,1.0,.95,1))
    assert A.save_asset(MPC)
    r=N.create_material_function('MF_LG8_HD2D_Color',BASE,'Shared gamma-aware HD-2D palette; non-destructive global control',True);assert r.success
    fn=unreal.load_asset(FUNC);sources={}
    names=['SourceColor','Saturation','Contrast','PaletteSteps','PosterizeStrength','ShadowLift','GlobalTint','Strength']
    for i,name in enumerate(names):
        n=E.create_material_expression_in_function(fn,unreal.MaterialExpressionFunctionInput,-650,i*140)
        n.set_editor_property('input_name',name);n.set_editor_property('sort_priority',i)
        n.set_editor_property('input_type',unreal.FunctionInputType.FUNCTION_INPUT_VECTOR3 if name in ['SourceColor','GlobalTint'] else unreal.FunctionInputType.FUNCTION_INPUT_SCALAR)
        sources[name]=n
    shader=E.create_material_expression_in_function(fn,unreal.MaterialExpressionCustom,-150,0)
    ins=[]
    for name in names:
        ci=unreal.CustomInput();ci.set_editor_property('input_name',name);ins.append(ci)
    shader.set_editor_property('inputs',ins);shader.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    shader.set_editor_property('code','''
float peak=max(1.0,max(SourceColor.r,max(SourceColor.g,SourceColor.b)));
float3 c=pow(saturate(SourceColor/peak),1.0/2.2);
float lum=dot(c,float3(.2126,.7152,.0722));
c=lerp(lum.xxx,c,max(0.,Saturation));
c=saturate((c-.5)*max(.1,Contrast)+.5+ShadowLift);
float levels=max(4.0,PaletteSteps);
c=lerp(c,floor(c*levels+.5)/levels,saturate(PosterizeStrength));
c=pow(saturate(c),2.2)*GlobalTint*peak;
return lerp(SourceColor,c,saturate(Strength));
''')
    for name,n in sources.items():link(n,shader,name)
    out=E.create_material_expression_in_function(fn,unreal.MaterialExpressionFunctionOutput,300,0);out.set_editor_property('output_name','StyledColor');out.set_editor_property('sort_priority',0)
    link(shader,out,'')
    assert A.save_asset(FUNC);save('function_graph.json',json.loads(N.export_function_graph(FUNC)))
    save('default_preset.json',{'scalars':DEFAULTS,'vectors':{'GlobalTint':[1.02,1.0,.95,1]}})
    print('Created shared MPC and eight-input reusable color function')
elif STAGE=='surface':
    assert not A.does_asset_exist(SURFACE)
    mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_LG8_HD2D_Surface',BASE,unreal.Material,unreal.MaterialFactoryNew())
    mat.set_editor_property('two_sided',True)
    uv=node(mat,unreal.MaterialExpressionTextureCoordinate,-1800,-400)
    tex=node(mat,unreal.MaterialExpressionTextureObjectParameter,-1800,-150);tex.set_editor_property('parameter_name','BaseColorTexture');tex.set_editor_property('group','Textures');tex.set_editor_property('texture',unreal.load_asset('/InterchangeAssets/gltf/Textures/T_White_srgb'))
    grid=custom(mat,{'Grid':scalar(mat,'PixelGrid',256,'Pixel Sampling',-1800,200),'Density':collection(mat,'TextureDensity',-1800,350)},'return max(16.,Grid*max(.0625,Density));',unreal.CustomMaterialOutputType.CMOT_FLOAT1,-1450,200)
    blend=custom(mat,{'Global':collection(mat,'PixelBlend',-1800,600),'Strength':collection(mat,'StyleStrength',-1800,750),'Local':scalar(mat,'PixelInfluence',1,'Pixel Sampling',-1800,900)},'return saturate(Global*Strength*Local);',unreal.CustomMaterialOutputType.CMOT_FLOAT1,-1300,550)
    sample=custom(mat,{'UV':uv,'Tex':tex,'Grid':grid,'Blend':blend},'''float2 snapped=(floor(UV*Grid)+.5)/Grid;
float2 q=lerp(UV,snapped,Blend);
return Texture2DSampleGrad(Tex,TexSampler,q,ddx(UV),ddy(UV)).rgb;''',x=-900,y=-200)
    tinted=custom(mat,{'Color':sample,'Tint':vector(mat,'MaterialTint',[1,1,1,1],x=-1150,y=-600)},'return Color*Tint;',x=-350,y=-200)
    styled=style(mat,tinted,x=150,y=-200);output(mat,styled,unreal.MaterialProperty.MP_BASE_COLOR,'StyledColor')
    rough=custom(mat,{'Roughness':scalar(mat,'Roughness',.88,x=-400,y=1150),'Bias':collection(mat,'RoughnessBias',-400,1320)},'return clamp(Roughness+Bias,.04,1.);',unreal.CustomMaterialOutputType.CMOT_FLOAT1,200,1150)
    output(mat,rough,unreal.MaterialProperty.MP_ROUGHNESS)
    output(mat,scalar(mat,'Metallic',0,x=500,y=1320),unreal.MaterialProperty.MP_METALLIC)
    normal=node(mat,unreal.MaterialExpressionTextureSampleParameter2D,-500,1650);normal.set_editor_property('parameter_name','NormalTexture');normal.set_editor_property('group','Textures');normal.set_editor_property('texture',unreal.load_asset('/InterchangeAssets/gltf/Textures/T_Generic_N'));normal.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    normalstyled=custom(mat,{'N':normal,'Strength':collection(mat,'NormalStrength',-500,1900)},'return normalize(float3(N.xy*max(0.,Strength),max(.001,N.z)));',x=100,y=1700)
    switch=node(mat,unreal.MaterialExpressionStaticSwitchParameter,500,1700);switch.set_editor_property('parameter_name','UseNormalMap');switch.set_editor_property('default_value',False);switch.set_editor_property('group','Optional Features')
    flat=node(mat,unreal.MaterialExpressionConstant3Vector,100,1900);flat.set_editor_property('constant',unreal.LinearColor(0,0,1))
    link(normalstyled,switch,'True');link(flat,switch,'False');output(mat,switch,unreal.MaterialProperty.MP_NORMAL)
    emission=custom(mat,{'Color':tinted,'Gain':scalar(mat,'EmissiveGain',0,x=-400,y=2200),'Global':collection(mat,'EmissiveMultiplier',-400,2350)},'return Color*max(0.,Gain)*max(0.,Global);',x=200,y=2200)
    output(mat,emission,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    assert A.save_asset(SURFACE);save('surface_graph.json',json.loads(N.export_material_graph(SURFACE)))
    print('Created one-texture default surface master; optional normal map static switch')
elif STAGE=='surface_finish':
    mat=unreal.load_asset(SURFACE);nodes=E.get_material_expressions(mat)
    normalstyled=next(n for n in nodes if isinstance(n,unreal.MaterialExpressionCustom) and str(n.get_editor_property('code')).startswith('return normalize'))
    tinted=next(n for n in nodes if isinstance(n,unreal.MaterialExpressionCustom) and str(n.get_editor_property('code'))=='return Color*Tint;')
    switch=next(n for n in nodes if isinstance(n,unreal.MaterialExpressionStaticSwitchParameter))
    flat=next(n for n in nodes if isinstance(n,unreal.MaterialExpressionConstant3Vector))
    link(normalstyled,switch,'True');link(flat,switch,'False');output(mat,switch,unreal.MaterialProperty.MP_NORMAL)
    emission=custom(mat,{'Color':tinted,'Gain':scalar(mat,'EmissiveGain',0,x=-400,y=2200),'Global':collection(mat,'EmissiveMultiplier',-400,2350)},'return Color*max(0.,Gain)*max(0.,Global);',x=200,y=2200)
    output(mat,emission,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    assert A.save_asset(SURFACE);save('surface_graph.json',json.loads(N.export_material_graph(SURFACE)))
    print('Surface repaired and saved; awaiting isolated compile')
elif STAGE.startswith('clone_'):
    kind=STAGE[6:]
    source={'Landscape':'/Game/Village/Materials/M_VillagePixelLandscape2','WorldProps':'/Game/Village/Materials/M_PixelPropWA2','Water':'/Game/Village/Materials/M_HD2D_Water','Sky':'/Game/Village/Materials/M_HD2D_Sky','Waterfall':'/Game/Libregrove/Level_V8/Materials/M_LG8_Waterfall'}[kind]
    path=BASE+'/M_LG8_HD2D_'+kind;assert not A.does_asset_exist(path)
    # Source graph has been inspected; record an immutable fresh copy before cloning.
    save('source_graph_'+kind+'.json',json.loads(N.export_material_graph(source)))
    mat=A.duplicate_asset(source,path);assert mat
    property_=unreal.MaterialProperty.MP_EMISSIVE_COLOR if kind in ['Sky','Waterfall'] else unreal.MaterialProperty.MP_BASE_COLOR
    src=E.get_material_property_input_node(mat,property_);srcout=E.get_material_property_input_node_output_name(mat,property_);assert src,kind
    # Function accepts a full color; if the original output selects a mask channel, preserve it.
    bridge=node(mat,unreal.MaterialExpressionMultiply,800,-250);link(src,bridge,'A',srcout)
    one=node(mat,unreal.MaterialExpressionConstant,650,-450);one.set_editor_property('r',1);link(one,bridge,'B')
    styled=style(mat,bridge,.30 if kind=='Sky' else .65 if kind in ['Water','Waterfall'] else 1.,x=1350,y=-200)
    output(mat,styled,property_,'StyledColor')
    if kind in ['Landscape','WorldProps']:
        normal=E.get_material_property_input_node(mat,unreal.MaterialProperty.MP_NORMAL)
        if normal:
            n=custom(mat,{'N':normal,'Strength':collection(mat,'NormalStrength',800,1650)},'return normalize(float3(N.xy*max(0.,Strength),max(.001,N.z)));',x=1300,y=1650);output(mat,n,unreal.MaterialProperty.MP_NORMAL)
        rough=E.get_material_property_input_node(mat,unreal.MaterialProperty.MP_ROUGHNESS)
        if rough:
            q=custom(mat,{'R':rough,'Bias':collection(mat,'RoughnessBias',800,1900)},'return clamp(R+Bias,.04,1.);',unreal.CustomMaterialOutputType.CMOT_FLOAT1,1300,1900);output(mat,q,unreal.MaterialProperty.MP_ROUGHNESS)
    assert A.save_asset(path);save('master_graph_'+kind+'.json',json.loads(N.export_material_graph(path)))
    print('Created unassigned style master',path)
elif STAGE.startswith('sampling_'):
    kind=STAGE[9:];path=BASE+'/M_LG8_HD2D_'+kind;mat=unreal.load_asset(path)
    nodes=list(E.get_material_expressions(mat));cache={};changed=[]
    pixel=collection(mat,'WorldPixelSizeCm',-3100,2500);density=collection(mat,'TextureDensity',-3100,2700)
    blend=custom(mat,{'Global':collection(mat,'PixelBlend',-3100,2900),'Strength':collection(mat,'StyleStrength',-3100,3100)},'return saturate(Global*Strength);',unreal.CustomMaterialOutputType.CMOT_FLOAT1,-2800,2900)
    for n in nodes:
        if isinstance(n,unreal.MaterialExpressionTextureObjectParameter):
            n.set_editor_property('mip_value_mode',unreal.TextureMipValueMode.TMVM_NONE);continue
        if not isinstance(n,unreal.MaterialExpressionTextureSample):continue
        if kind=='Landscape':
            pname=str(n.get_editor_property('parameter_name'));layer=pname.split('_')[1]
            if layer not in ['Grass','Dirt','Cobblestone','Moss','Cliff']:continue
            tile_name='TileCm_'+layer
        else:tile_name='TileCm'
        tile=next(t for t in nodes if isinstance(t,unreal.MaterialExpressionScalarParameter) and str(t.get_editor_property('parameter_name'))==tile_name)
        uv=E.get_inputs_for_material_expression(mat,n)[0];assert uv
        key=uv.get_path_name()
        if key not in cache:
            q=custom(mat,{'UV':uv,'TileCm':tile,'PixelCm':pixel,'Density':density,'Blend':blend},'float grid=max(8.,TileCm/max(.25,PixelCm)*max(.0625,Density)); return lerp(UV,(floor(UV*grid)+.5)/grid,Blend);',unreal.CustomMaterialOutputType.CMOT_FLOAT2,-2400,2500+len(cache)*300)
            dx=node(mat,unreal.MaterialExpressionDDX,-2300,2650+len(cache)*300);dy=node(mat,unreal.MaterialExpressionDDY,-2100,2650+len(cache)*300)
            link(uv,dx,'');link(uv,dy,'');cache[key]=(q,dx,dy)
        q,dx,dy=cache[key]
        n.set_editor_property('mip_value_mode',unreal.TextureMipValueMode.TMVM_DERIVATIVE)
        pins=list(E.get_material_expression_input_names(n));assert 'DDX(UVs)' in pins and 'DDY(UVs)' in pins
        link(q,n,str(pins[0]));link(dx,n,'DDX(UVs)');link(dy,n,'DDY(UVs)');changed.append(n.get_name())
    assert changed
    assert A.save_asset(path);save('sampling_'+kind+'.json',{'samples':changed,'unique_uvs':len(cache),'original_derivatives':True})
    save('sampling_graph_'+kind+'.json',json.loads(N.export_material_graph(path)))
    print(kind,'stable world pixel samples:',len(changed))
elif STAGE.startswith('emission_'):
    kind=STAGE[9:];path=BASE+'/M_LG8_HD2D_'+kind;mat=unreal.load_asset(path)
    prop=unreal.MaterialProperty.MP_EMISSIVE_COLOR;src=E.get_material_property_input_node(mat,prop)
    if src:
        srcout=E.get_material_property_input_node_output_name(mat,prop)
        mul=node(mat,unreal.MaterialExpressionMultiply,1900,2300);link(src,mul,'A',srcout);link(collection(mat,'EmissiveMultiplier',1600,2500),mul,'B');output(mat,mul,prop)
    assert A.save_asset(path);save('emission_'+kind+'.json',{'connected':bool(src)})
    print(kind,'global emissive linked:',bool(src))
elif STAGE.startswith('compile_'):
    kind=STAGE[8:];path=BASE+'/M_LG8_HD2D_'+kind
    assert unreal.MaterialService.compile_material(path)
    save('compile_'+kind+'.json',{'path':path,'compile_result':True})
    print('Compiled unassigned master',path)
else:raise RuntimeError(STAGE)
