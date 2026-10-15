import unreal
import json
from pathlib import Path

def build():
    base='/Game/Cindehearth/Tripo/GormHouse_P2_v001'
    texbase=base+'/CH_GormP2_Game_v001/Textures/'
    color=texbase+'Color.Color';normal=texbase+'Normal.Normal'
    assert unreal.load_asset(color) and unreal.load_asset(normal)
    path=base+'/M_CH_P2_Pixel'
    assert not unreal.EditorAssetLibrary.does_asset_exist(path),'Inspect existing material before editing'
    s=unreal.MaterialService;n=unreal.MaterialNodeService
    assert s.create_material('M_CH_P2_Pixel',base).success
    def param(kind,name,value,x,y):
        node=n.create_parameter(path,kind,name,'Cindehearth',value,x,y);assert node.id;return node.id
    def connect(src,dst,pin,out=''):
        assert n.connect_expressions(path,src,out,dst,pin),(src,dst,pin)
    def output(src,pin,out=''):
        assert n.connect_to_output(path,src,out,pin)
    uv=n.create_expression(path,'TextureCoordinate',-1100,0).id
    grid=param('Scalar','PixelGrid','512',-1100,180)
    snap=n.create_custom_expression(path,'return (floor(UV * max(Grid, 1.0)) + 0.5) / max(Grid, 1.0);','CMOT_Float2','Pixel UV centers','UV,Grid',-850,0).id
    connect(uv,snap,'UV');connect(grid,snap,'Grid')
    bc=param('Texture','Albedo',color,-600,0)
    nm=param('Texture','NormalMap',normal,-600,330)
    assert n.set_expression_property(path,nm,'SamplerType','SAMPLERTYPE_Normal')
    connect(snap,bc,'UVs');connect(snap,nm,'UVs')
    tint=param('Vector','Tint','0.90,0.92,0.94,1',-600,-180)
    col=n.create_expression(path,'Multiply',-320,0).id
    connect(bc,col,'A','RGB');connect(tint,col,'B')
    output(col,'BaseColor')
    strength=param('Scalar','NormalStrength','0.12',-600,520)
    flatten=n.create_custom_expression(path,'return normalize(float3(N.xy * Strength, lerp(1.0, N.z, Strength)));','CMOT_Float3','Reduced normal detail','N,Strength',-320,330).id
    connect(nm,flatten,'N','RGB');connect(strength,flatten,'Strength');output(flatten,'Normal')
    rough=param('Scalar','Roughness','0.92',-320,560);output(rough,'Roughness')
    spec=param('Scalar','Specular','0.10',-320,710);output(spec,'Specular')
    lift=param('Scalar','AmbientLift','0.04',-600,-400)
    emiss=n.create_expression(path,'Multiply',-320,-230).id
    connect(col,emiss,'A');connect(lift,emiss,'B');output(emiss,'EmissiveColor')
    assert s.compile_material(path)
    assert unreal.EditorAssetLibrary.save_asset(path)
    mi=base+'/MI_CH_GormP2_Pixel'
    assert s.create_instance(path,'MI_CH_GormP2_Pixel',base).success
    assert s.save_instance(mi)
    graph=n.export_material_graph(path)
    (Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Pipeline/verification/p2_pixel_graph.json').write_text(graph,encoding='utf-8')
    print('CREATED',path,mi)

build()
