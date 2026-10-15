"""Pixel flame and rising soot cards. No particle-system or shared-asset edits."""
import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Restyle'
dest='/Game/Cindehearth/HD2D_v002/Materials';S=unreal.MaterialService;N=unreal.MaterialNodeService;L=unreal.EditorAssetLibrary
results=[]
for key in ['Fire','Smoke']:
    p=dest+'/M_CH_Pixel'+key
    assert not L.does_asset_exist(p)
    assert S.create_material('M_CH_Pixel'+key,dest).success
    m=unreal.load_asset(p)
    m.set_editor_property('two_sided',True)
    m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_MASKED if key=='Fire' else unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    uv=N.create_expression(p,'TextureCoordinate',-900,0).id
    time=N.create_expression(p,'Time',-900,160).id
    phase=N.create_parameter(p,'Scalar','Phase','Motion','0',-900,320).id
    if key=='Fire':
        code=r'''
float2 q=(floor(UV*float2(44,52))+.5)/float2(44,52);
float t=floor(Time*10.0)/10.0+Phase*7.0;
float h=1.0-q.y;
float sway=sin(h*8.0-t*2.5)*.07*h+sin(h*19.0-t*3.3)*.024;
float x=abs(q.x-.5-sway);
float width=.34*pow(saturate(1.0-h),.75);
float scallop=sin(h*27.0-t*4.1)*.027+sin(q.x*47.0+h*13.0+t*3.0)*.018;
float mask=step(x,width+scallop)*step(.018,h)*step(h,.97);
float core=saturate(1.0-x/max(width,.01))*saturate((.8-h)*1.8);
float3 c=lerp(float3(1.65,.14,.008),float3(3.0,1.14,.10),floor(core*4.0)/4.0);
return float4(c,mask);
'''
    else:
        code=r'''
float2 q=(floor(UV*24.0)+.5)/24.0;
float age=frac(Time*.14+Phase);
float2 c=q-.5;
float wobble=sin(q.x*19.0+q.y*14.0+Phase*16.0)*.075+sin(q.y*31.0-Phase*9.0)*.03;
float radial=saturate((.43-length(c)+wobble)*8.0);
float fade=smoothstep(0.0,.18,age)*(1.0-smoothstep(.62,1.0,age));
return float4(float3(.24,.265,.265),radial*fade*.24);
'''
    shape=N.create_custom_expression(p,code,'CMOT_Float4','Cindehearth pixel '+key,'UV,Time,Phase',-400,0).id
    for src,pin in [(uv,'UV'),(time,'Time'),(phase,'Phase')]:assert N.connect_expressions(p,src,'',shape,pin)
    rgb=N.create_custom_expression(p,'return Shape.rgb;','CMOT_Float3','Color','Shape',-100,-80).id
    alpha=N.create_custom_expression(p,'return Shape.a;','CMOT_Float1','Cutout / fade','Shape',-100,80).id
    for n in [rgb,alpha]:assert N.connect_expressions(p,shape,'',n,'Shape')
    assert N.connect_to_output(p,rgb,'','EmissiveColor')
    assert N.connect_to_output(p,alpha,'','OpacityMask' if key=='Fire' else 'Opacity')
    if key=='Smoke':
        drift=N.create_custom_expression(p,'float a=frac(Time*.14+Phase); return float3(a*a*80.0,sin(a*2.4+Phase*5.0)*a*18.0,a*240.0);','CMOT_Float3','Slow shared upward drift','Time,Phase',-200,330).id
        for src,pin in [(time,'Time'),(phase,'Phase')]:assert N.connect_expressions(p,src,'',drift,pin)
        assert N.connect_to_output(p,drift,'','WorldPositionOffset')
    assert S.compile_material(p)
    diag=N.get_material_diagnostics(p);assert diag.is_compiled_ok,list(diag.compile_errors)
    assert L.save_asset(p)
    for i in range(4):
        name='MI_CH_Pixel'+key+str(i);mi=dest+'/'+name
        assert S.create_instance(p,name,dest).success
        assert S.set_instance_scalar_parameter(mi,'Phase',i*.25);assert S.save_instance(mi)
    results.append(dict(material=p,compiled=diag.is_compiled_ok,geometry='crossed quads',particle_system=None))
(root/'verification/effect_materials.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
print('PIXEL FIRE / SOOT MATERIALS COMPILED')
