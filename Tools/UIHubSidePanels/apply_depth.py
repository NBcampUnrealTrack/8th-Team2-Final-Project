"""Retain neutral polish; add soft depth, foreground bokeh and a noisy rail fade."""
import importlib.util,json
from pathlib import Path
import unreal
ROOT=Path(unreal.Paths.project_dir()).resolve()
spec=importlib.util.spec_from_file_location('hub_polish',ROOT/'Tools/UIHubSidePanels/apply_polish.py')
p=importlib.util.module_from_spec(spec);spec.loader.exec_module(p)
h=p.h;M=h.M;i=p.i;r=p.r
OUT=ROOT/'Saved/UIHubDepth';OUT.mkdir(exist_ok=True)

def apply():
 assert not h.W.is_pie_running()
 p.apply()
 # Slogan-free siblings preserve the original artwork for earlier authoring passes.
 for name in ['T_Hub_CleanHeaderProfile','T_Hub_CleanDesk']:
  path=h.ART+'/'+name
  if not h.E.does_asset_exist(path):
   imported,error=unreal.AssetDiscoveryService.import_asset(str(ROOT/'ArtSource/UI/HubSidePanels'/(name+'.png')),h.ART,name);assert imported,error
  tex=unreal.load_asset(path)
  for k,v in [('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON),('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI),('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS),('srgb',True),('compression_no_alpha',False),('never_stream',True)]:tex.set_editor_property(k,v)
  assert h.E.save_loaded_asset(tex)
 idle=unreal.load_asset(h.ART+'/T_Hub_CleanHeaderProfile')
 a=h.mat('M_Hub_DepthRail');uv=M.create_material_expression(a,unreal.MaterialExpressionTextureCoordinate,-900,0)
 mapped=h.custom(a,'return UV*float2(740,899)/float2(1672,941);',unreal.CustomMaterialOutputType.CMOT_FLOAT2,[('UV',uv,'')])
 source=r.sample(a,idle);M.connect_material_expressions(mapped,'',source,'UVs')
 color=h.custom(a,p.COOL,unreal.CustomMaterialOutputType.CMOT_FLOAT3,[('C',source,'RGB')])
 contour=h.custom(a,'float2 p=UV*float2(740,899);'+r.polygon(p.LEFT),unreal.CustomMaterialOutputType.CMOT_FLOAT1,[('UV',uv,'')])
 # Only the actual button silhouette is opaque; gaps and outside borders use the rail fade.
 row_shape=h.custom(a,p.row_masks.RAIL.replace('620,899','740,899'),unreal.CustomMaterialOutputType.CMOT_FLOAT1,[('UV',uv,'')])
 fg=h.custom(a,'float2 q=UV*float2(740,899);float header=Mask*smoothstep(.10,.22,max(C.r,max(C.g,C.b)))*(1-step(144,q.y));float footer=step(829,q.y)*step(q.y,873)*step(52,q.x)*step(q.x,545);float ink=smoothstep(.035,.10,dot(C,float3(.2126,.7152,.0722)));return max(Row,max(header,footer*ink));',unreal.CustomMaterialOutputType.CMOT_FLOAT1,[('UV',uv,''),('C',color,''),('Mask',contour,''),('Row',row_shape,'')])
 noise=h.custom(a,'float2 q=UV*float2(42,61);float2 b=floor(q),f=frac(q);f=f*f*(3-2*f);float4 v=float4(dot(b,float2(127.1,311.7)),dot(b+float2(1,0),float2(127.1,311.7)),dot(b+float2(0,1),float2(127.1,311.7)),dot(b+1,float2(127.1,311.7)));v=frac(sin(v)*43758.5453);return lerp(lerp(v.x,v.y,f.x),lerp(v.z,v.w,f.x),f.y);',unreal.CustomMaterialOutputType.CMOT_FLOAT1,[('UV',uv,'')])
 rgb=h.custom(a,'float shade=.90+.10*smoothstep(0,.55,UV.x);float3 navy=float3(.018,.030,.048)*(shade+(N-.5)*.10);return lerp(navy,C*(.95+.05*smoothstep(0,.3,UV.x)),F);',unreal.CustomMaterialOutputType.CMOT_FLOAT3,[('UV',uv,''),('C',color,''),('N',noise,''),('F',fg,'')])
 alpha=h.custom(a,'float x=UV.x*740;float lower=smoothstep(710,855,UV.y*899);float n=(N-.5)*54*smoothstep(150,420,x);float fade=.97*(1-smoothstep(170+140*lower,605+120*lower,x+n));fade*=1-(N*.12*smoothstep(250+140*lower,570+120*lower,x));return lerp(fade,1,F)*(1-smoothstep(728,740,x));',unreal.CustomMaterialOutputType.CMOT_FLOAT1,[('UV',uv,''),('N',noise,''),('F',fg,'')])
 M.connect_material_property(rgb,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR);M.connect_material_property(alpha,'',unreal.MaterialProperty.MP_OPACITY);h.finishmat(a)
 bp,t=h.tree(h.MAIN);o=h.obj(t,'HubLeftBackplate');o.set_brush_from_material(a);h.rect(o,0,0,740,899)
 # The rail owns its fade now: no second black strip underneath it.
 h.obj(t,'HubLeftEdgeFade').set_visibility(unreal.SlateVisibility.COLLAPSED)
 # Neutral multiplicative shade preserves the chosen palette.
 for matname in ['M_Hub_PolishProfile','M_Hub_PolishDesk']:
  mat=unreal.load_asset(h.ART+'/'+matname)
  # Rebuild these surfaces explicitly, then add shade using known source nodes.
  tex=idle if matname.endswith('Profile') else unreal.load_asset(h.ART+'/T_Hub_CleanDesk')
  mat=h.mat(matname);u=M.create_material_expression(mat,unreal.MaterialExpressionTextureCoordinate,-700,0);s=r.sample(mat,tex)
  cool=h.custom(mat,p.COOL,unreal.CustomMaterialOutputType.CMOT_FLOAT3,[('C',s,'RGB')])
  if matname.endswith('Profile'):
   mapped=h.custom(mat,'float2 q=(UV*float2(1672,941)-float2(985,139))/float2(687,322);return (float2(1018,149)+q*float2(632,281))/float2(1672,941);',unreal.CustomMaterialOutputType.CMOT_FLOAT2,[('UV',u,'')]);M.connect_material_expressions(mapped,'',s,'UVs')
   code='float2 q=(UV*float2(1672,941)-float2(985,139))/float2(687,322);float edge=pow(saturate(abs(q.y-.5)*2),2);float corner=smoothstep(.70,1,q.x)*(1-smoothstep(0,.70,q.y));return C*(1-.10*edge-.15*corner);'
   pts=[(985,156),(1001,139),(1657,139),(1674,156),(1674,444),(1657,461),(1001,461),(985,444)]
   mask=h.custom(mat,'float2 p=UV*float2(1672,941);'+r.polygon(pts),unreal.CustomMaterialOutputType.CMOT_FLOAT1,[('UV',u,'')])
  else:
   code='float bottom=smoothstep(.82,1,UV.y);float right=smoothstep(.83,1,UV.x);return C*(1-.21*bottom-.10*right);'
   mask=h.custom(mat,'return saturate((A-.03)/.97);',unreal.CustomMaterialOutputType.CMOT_FLOAT1,[('A',s,'A')])
  c=h.custom(mat,code,unreal.CustomMaterialOutputType.CMOT_FLOAT3,[('UV',u,''),('C',cool,'')])
  M.connect_material_property(c,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR);M.connect_material_property(mask,'',unreal.MaterialProperty.MP_OPACITY);h.finishmat(mat)
 # Match live data to the slightly enlarged rectangular paper.
 for name in ['HubLevel','HubName','HubExperienceBar','HubExperience','HubLocation','HubJob','HubGold','HubCharacterId']:
  widget=h.obj(t,name);pos=widget.slot.get_position()
  widget.slot.set_position(unreal.Vector2D(985+(pos.x-1018)*687/632,(pos.y+139-149)*322/281))
 motto=h.obj(t,'HubMottoInput');motto.slot.set_position(unreal.Vector2D(1096,214))
 # Separate RGBA foreground, softly filtered with alpha-aware taps.
 name='T_Hub_DepthForeground';path=h.ART+'/'+name
 if not h.E.does_asset_exist(path):
  imported,error=unreal.AssetDiscoveryService.import_asset(str(ROOT/'ArtSource/UI/HubSidePanels'/(name+'.png')),h.ART,name);assert imported,error
 tex=unreal.load_asset(path)
 for k,v in [('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON),('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI),('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS),('compression_no_alpha',False),('never_stream',True)]:tex.set_editor_property(k,v)
 assert h.E.save_loaded_asset(tex)
 mat=h.mat('M_Hub_DepthForeground');u=M.create_material_expression(mat,unreal.MaterialExpressionTextureCoordinate,-900,0);inputs=[('UV',u,'')];colors=[];alphas=[]
 for n,(x,y,w) in enumerate([(0,0,4),(-1,0,2),(1,0,2),(0,-1,2),(0,1,2),(-1,-1,1),(1,-1,1),(-1,1,1),(1,1,1)]):
  q=h.custom(mat,f'return UV+float2({x}*5.0/1672,{y}*5.0/941);',unreal.CustomMaterialOutputType.CMOT_FLOAT2,[('UV',u,'')]);s=r.sample(mat,tex);M.connect_material_expressions(q,'',s,'UVs');inputs += [(f'C{n}',s,'RGB'),(f'A{n}',s,'A')];colors.append(f'C{n}*A{n}*{w}');alphas.append(f'A{n}*{w}')
 total='+'.join(alphas);c=h.custom(mat,'return ('+'+'.join(colors)+')/max(.001,'+total+');',unreal.CustomMaterialOutputType.CMOT_FLOAT3,inputs)
 mask=h.custom(mat,'float a=('+total+')/16;float left=lerp(1-smoothstep(.025,.070,UV.x),1,step(.5,UV.x));float clearX=smoothstep(.02,.055,length((UV-float2(.952,.045))*float2(1,0.56)));float fields=smoothstep(.82,.86,UV.x)*(1-smoothstep(.97,1,UV.x))*smoothstep(.04,.08,UV.y)*(1-smoothstep(.24,.29,UV.y));return a*.68*left*clearX*(1-.75*fields);',unreal.CustomMaterialOutputType.CMOT_FLOAT1,inputs)
 M.connect_material_property(c,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR);M.connect_material_property(mask,'',unreal.MaterialProperty.MP_OPACITY);h.finishmat(mat)
 o=i.add('Image','HubDepthForeground','RootCanvas');i.fullslot(o,4);o.set_brush_from_material(mat);o.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
 result=h.B.compile_blueprint(h.MAIN);assert result.success and result.num_errors==0,result.export_text();assert h.E.save_asset(h.MAIN,False)
 (OUT/'applied.json').write_text(json.dumps({'errors':result.num_errors,'warnings':result.num_warnings,'foreground_hit_test_invisible':True,'rail_noise_static':True,'rail_fade_end':740,'footer_glyphs_only':True},indent=2))
if __name__=='__main__':apply()
