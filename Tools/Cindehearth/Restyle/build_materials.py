"""Own HD-2D atlas materials; no shared Village material is modified."""
import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Restyle'
base='/Game/Cindehearth/HD2D_v002'
dest=base+'/Materials'
atlas=base+'/Textures/EnvironmentAtlas_v001'
assert unreal.load_asset(atlas)
S=unreal.MaterialService;N=unreal.MaterialNodeService;L=unreal.EditorAssetLibrary
path=dest+'/M_CH_AtlasWorld'
assert not L.does_asset_exist(path)
assert S.create_material('M_CH_AtlasWorld',dest).success
def param(kind,name,value,x,y):
    a=N.create_parameter(path,kind,name,'Cindehearth',str(value),x,y);assert a.id;return a.id
def connect(a,b,pin,out=''):
    assert N.connect_expressions(path,a,out,b,pin)
def output(a,pin):assert N.connect_to_output(path,a,'',pin)
w=N.create_expression(path,'WorldPosition',-1100,0).id
n=N.create_expression(path,'VertexNormalWS',-1100,130).id
tex=param('TextureObject','Atlas',atlas,-1100,260)
tile=param('Scalar','TileCm',200,-1100,400)
cell=param('Vector','Cell','0,0,0,0',-1100,530)
tint=param('Vector','Tint','1.5,1.5,1.5,1',-1100,660)
code=r'''
float3 weights=pow(abs(normalize(Normal)),8.0);weights/=max(dot(weights,1.0),0.0001);
float3 p=World/max(Tile,1.0);
// Mirrored cells remove edge discontinuities without touching the source image.
float2 a=abs(frac(p.yz*.5)*2.0-1.0);
float2 b=abs(frac(p.xz*.5)*2.0-1.0);
float2 c=abs(frac(p.xy*.5)*2.0-1.0);
a=(floor(a*128.0)+.5)/128.0;b=(floor(b*128.0)+.5)/128.0;c=(floor(c*128.0)+.5)/128.0;
float2 size=float2(1.0/3.0,.5), offset=Cell.xy*size;
float3 ca=Texture2DSampleLevel(Atlas,AtlasSampler,offset+(a*.96+.02)*size,0).rgb;
float3 cb=Texture2DSampleLevel(Atlas,AtlasSampler,offset+(b*.96+.02)*size,0).rgb;
float3 cc=Texture2DSampleLevel(Atlas,AtlasSampler,offset+(c*.96+.02)*size,0).rgb;
float3 color=ca*weights.x+cb*weights.y+cc*weights.z;
float macro=sin(World.x*.0023+sin(World.y*.0017)*1.7)*sin(World.y*.0031+World.x*.0006);
return color*Tint.rgb*(1.0+macro*.065);
'''
custom=N.create_custom_expression(path,code,'CMOT_Float3','Pixel atlas in world space','World,Normal,Atlas,Tile,Cell,Tint',-450,0).id
for src,pin in [(w,'World'),(n,'Normal'),(tex,'Atlas'),(tile,'Tile'),(cell,'Cell'),(tint,'Tint')]:connect(src,custom,pin)
output(custom,'BaseColor')
rough=param('Scalar','Roughness',.95,-450,350);output(rough,'Roughness')
spec=param('Scalar','Specular',.08,-450,480);output(spec,'Specular')
ambient=param('Scalar','AmbientLift',.055,-450,-220)
mult=N.create_expression(path,'Multiply',-160,-180).id;connect(custom,mult,'A');connect(ambient,mult,'B');output(mult,'EmissiveColor')
assert S.compile_material(path)
diag=N.get_material_diagnostics(path);assert diag.success and diag.is_compiled_ok,list(diag.compile_errors)
assert len(diag.referenced_texture_paths)==1,list(diag.referenced_texture_paths)
assert L.save_asset(path)
settings={
 'Stone':((0,0),180,(1.55,1.58,1.56)), 'StoneLight':((0,0),220,(1.85,1.79,1.64)),
 'Rock':((2,1),380,(1.60,1.62,1.58)), 'Ash':((1,0),250,(1.28,1.34,1.39)),
 'Soil':((1,0),300,(1.60,1.48,1.31)), 'Path':((2,0),230,(1.55,1.48,1.34)),
 'Roof':((1,1),220,(1.15,1.22,1.25)), 'RoofPatch':((1,1),220,(1.23,1.20,1.04)),
 'Timber':((0,1),180,(1.23,1.18,1.10)), 'Wood':((0,1),160,(1.38,1.28,1.12)),
 'Iron':((1,1),450,(.68,.76,.82)), 'Brass':((0,1),260,(1.18,1.02,.73)),
 'Paper':((1,0),800,(2.1,2.0,1.8)), 'Cloth':((1,0),450,(.8,.91,.95)),
 'Pine':((2,1),260,(.53,.72,.57))}
for key,(cell_value,tile_value,tint_value) in settings.items():
    mi=dest+'/MI_CH_'+key
    assert not L.does_asset_exist(mi)
    assert S.create_instance(path,'MI_CH_'+key,dest).success
    assert S.set_instance_vector_parameter(mi,'Cell',cell_value[0],cell_value[1],0,0)
    assert S.set_instance_vector_parameter(mi,'Tint',*tint_value)
    assert S.set_instance_scalar_parameter(mi,'TileCm',tile_value)
    assert S.save_instance(mi)
(root/'verification/materials_created.json').write_text(json.dumps(dict(master=path,instances=list(settings),compiled=True,textures=list(diag.referenced_texture_paths)),indent=2),encoding='utf-8')
print('ATLAS MASTER COMPILED; INSTANCES',len(settings))
