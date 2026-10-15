"""One dedicated fire material; retains all source assets unchanged."""
import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Fire_v005';out=root/'verification'
assert not unreal.WidgetService.is_pie_running()
dest='/Game/Cindehearth/Fire_v005/Materials';p=dest+'/M_CH_FrontFire'
S=unreal.MaterialService;N=unreal.MaterialNodeService;L=unreal.EditorAssetLibrary
assert not L.does_asset_exist(p)
assert S.create_material('M_CH_FrontFire',dest).success
m=unreal.load_asset(p);m.set_editor_property('two_sided',True);m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT);m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
nodes={}
nodes['UV']=N.create_expression(p,'TextureCoordinate',-900,0).id
nodes['Time']=N.create_expression(p,'Time',-900,160).id
nodes['Phase']=N.create_parameter(p,'Scalar','Phase','Fire','0',-900,320).id
nodes['Intensity']=N.create_parameter(p,'Scalar','Intensity','Fire','1',-900,480).id
shape=N.create_custom_expression(p,(root/'fire.hlsl').read_text(),'CMOT_Float4','Cindehearth fixed front fire','UV,Time,Phase,Intensity',-450,0).id
for pin,node in nodes.items():assert N.connect_expressions(p,node,'',shape,pin)
rgb=N.create_custom_expression(p,'return Shape.rgb;','CMOT_Float3','Warm core and orange rim','Shape',-100,-80).id
alpha=N.create_custom_expression(p,'return Shape.a;','CMOT_Float1','Rooted silhouette and fading motes','Shape',-100,80).id
for node in (rgb,alpha):assert N.connect_expressions(p,shape,'',node,'Shape')
assert N.connect_to_output(p,rgb,'','EmissiveColor') and N.connect_to_output(p,alpha,'','Opacity')
compiled=S.compile_material(p);assert compiled
assert L.save_asset(p)
instances=[]
for name,phase,intensity in [('Hearth',0,1),('Forge',.37,.82)]:
    mi=dest+'/MI_CH_FrontFire_'+name;assert S.create_instance(p,'MI_CH_FrontFire_'+name,dest).success
    assert S.set_instance_scalar_parameter(mi,'Phase',phase) and S.set_instance_scalar_parameter(mi,'Intensity',intensity)
    assert S.save_instance(mi);instances.append(mi)
graph=json.loads(N.export_material_graph(p));assert len(graph['output_connections'])==2
(out/'material_graph.json').write_text(json.dumps(graph,indent=2),encoding='utf-8')
(out/'material_created.json').write_text(json.dumps(dict(material=p,instances=instances,compile_requested=compiled,expressions=len(graph['expressions']),render_mode='Unlit translucent, depth-tested; two static quads; no Niagara'),indent=2),encoding='utf-8')
print('FRONT FIRE MATERIAL CREATED',p)
