"""Analytic frame geometry; artwork never defines the outer border silhouette."""
import unreal,importlib.util,shutil,json
from pathlib import Path
from datetime import datetime
ROOT=Path(unreal.Paths.project_dir()).resolve()
s=importlib.util.spec_from_file_location('key',ROOT/'Tools/UISettingsReference/apply_key_polish.py')
k=importlib.util.module_from_spec(s);s.loader.exec_module(k)
p=k.p;M=p.M;E=p.E
CODE=r'''
float2 v=UV*float2(1536,1024);
float2 center=float2(769,507);
float2 halfsize=float2(734,456);
float radius=47;
float2 q=abs(v-center)-halfsize+radius;
float d=length(max(q,0))+min(max(q.x,q.y),0)-radius;
float aa=max(fwidth(d),.7);
float body=1-smoothstep(-aa*.5,aa*.5,d);
// Four independent rods stop at their end ornaments; only fine arcs join them.
float2 av=abs(v-center);
float horizontal=1-smoothstep(686,688,av.x);
float vertical=1-smoothstep(408,410,av.y);
float hy=abs(av.y-454.7);
float vx=abs(av.x-732.7);
float rodH=(1-smoothstep(1.0,1.0+max(fwidth(hy),.7),hy))*horizontal;
float rodV=(1-smoothstep(1.0,1.0+max(fwidth(vx),.7),vx))*vertical;
float rod=max(rodH,rodV);
float corners=(1-horizontal)*(1-vertical);
float arc=(1-smoothstep(.30,.30+aa,abs(d+1.3)))*corners;
float rim=max(rod,arc*.82);
// Quiet parallel engraving on each rod, ending before the star mounts.
float innerH=(1-smoothstep(.20,.20+aa,abs(av.y-449.5)))*(1-smoothstep(664,674,av.x));
float innerV=(1-smoothstep(.20,.20+aa,abs(av.x-727.5)))*(1-smoothstep(386,396,av.y));
float inner=max(innerH,innerV)*.40;
// Sample calm interior navy in the former banner strip; no ribbon/fold/compass.
float2 artUV=float2(max(UV.x,.155),UV.y);
float4 tex=Texture2DSample(Art,ArtSampler,artUV);
float3 C=tex.rgb;
float l=dot(C,float3(.2126,.7152,.0722));
float warm=smoothstep(-.04,.32,(C.r-C.b)/max(.015,max(C.r,C.b)));
float pale=smoothstep(.06,.18,l)*(1-saturate((C.b-C.r)*8));
float3 art=lerp(Navy*clamp(sqrt(l/.023),.68,1.35),Gold*clamp(sqrt(l/.55),.45,1.25),max(warm,pale));
// Remove the raster outline completely before drawing a constant-width rim.
float artBlend=1-smoothstep(-24,-15,d);
float3 fill=lerp(Navy,art,artBlend);
float3 rgb=lerp(fill,Gold,saturate(rim+inner));
float alpha=body;
// Separate symmetric four-point decorations. No wavy texture silhouettes.
float2 centers[8]={float2(82,961.7),float2(1456,961.7),float2(1456,52.3),float2(82,52.3),float2(36.3,916),float2(1501.7,916),float2(1501.7,98),float2(36.3,98)};
float star=0;
float starLight=1;
for(int i=0;i<8;i++) {
 float2 extent=i<4?float2(23,17):float2(17,23);
 float2 a=abs(v-centers[i])/extent;
 float sd=sqrt(a.x)+sqrt(a.y)-1;
 float sa=max(fwidth(sd),.02);
 float shape=1-smoothstep(-sa*.5,sa*.5,sd);
 if(shape>star)starLight=1.04+.10*clamp((centers[i].x-v.x+centers[i].y-v.y)/18,-1,1);
 star=max(star,shape);
}
rgb=lerp(rgb,Gold*starLight,star);alpha=max(alpha,star);
return float4(rgb,alpha);
'''
def apply():
 assert not p.W.is_pie_running()
 path=k.BASE+'WBP_KeyBindingsWindow'
 out=ROOT/'Saved/UIKeySettingsPolish'/('geometry_'+datetime.now().strftime('%Y%m%d_%H%M%S'));out.mkdir()
 shutil.copy2(ROOT/'Content'/(path[6:]+'.uasset'),out/'WBP_KeyBindingsWindow.uasset')
 name='M_KeySettingsGeometricFrame';mp=p.PRIVATE+'/'+name
 a=unreal.load_asset(mp)
 if a:
  (out/'previous_graph.json').write_text(unreal.MaterialNodeService.export_material_graph(mp))
  M.delete_all_material_expressions(a)
 else:a=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,p.PRIVATE,unreal.Material,unreal.MaterialFactoryNew())
 a.set_editor_property('material_domain',unreal.MaterialDomain.MD_UI);a.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
 uv=M.create_material_expression(a,unreal.MaterialExpressionTextureCoordinate,-600,0)
 tex=M.create_material_expression(a,unreal.MaterialExpressionTextureObject,-600,180);tex.set_editor_property('texture',unreal.load_asset(p.PRIVATE+'/T_KeySettings_FrameRounded'))
 banner=M.create_material_expression(a,unreal.MaterialExpressionTextureObject,-600,260)
 banner.set_editor_property('texture',unreal.load_asset(p.PRIVATE+'/T_Settings_BannerCelestial') or unreal.load_asset(p.PRIVATE+'/T_Settings_BannerLaurel') or unreal.load_asset(p.PRIVATE+'/T_KeySettings_FrameRounded'))
 n=M.create_material_expression(a,unreal.MaterialExpressionCustom,0,0);n.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT4);n.set_editor_property('code',CODE)
 inputs=[]
 for name in ['UV','Art','Banner','Navy','Gold']:
  ci=unreal.CustomInput();ci.set_editor_property('input_name',name);inputs.append(ci)
 n.set_editor_property('inputs',inputs)
 assert M.connect_material_expressions(uv,'',n,'UV');assert M.connect_material_expressions(tex,'',n,'Art')
 assert M.connect_material_expressions(banner,'',n,'Banner')
 for name,key in [('Navy','navy'),('Gold','gold')]:
  c=M.create_material_expression(a,unreal.MaterialExpressionConstant3Vector,-500,350 if key=='navy' else 480);c.set_editor_property('constant',p.col(key));assert M.connect_material_expressions(c,'',n,name)
 for alpha,prop in [(False,unreal.MaterialProperty.MP_EMISSIVE_COLOR),(True,unreal.MaterialProperty.MP_OPACITY)]:
  mask=M.create_material_expression(a,unreal.MaterialExpressionComponentMask,350,180 if alpha else 0)
  for channel in ['r','g','b']:mask.set_editor_property(channel,not alpha)
  mask.set_editor_property('a',alpha);assert M.connect_material_expressions(n,'',mask,'');assert M.connect_material_property(mask,'',prop)
 assert not M.recompile_material(a)
 diag=unreal.MaterialNodeService.get_material_diagnostics(mp);print(diag);assert diag.is_compiled_ok
 assert E.save_loaded_asset(a)
 sk=k.s.Skin('WBP_KeyBindingsWindow');sk.get('WindowBG').set_editor_property('background',k.q.brush(a))
 c=p.B.compile_blueprint(path);assert c.success and not c.num_errors;assert E.save_asset(path,False)
 (out/'compile.txt').write_text(c.export_text());print(c.export_text())
if __name__=='__main__':apply()
