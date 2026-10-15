"""Rich reference finish. Latest explicit user art direction supersedes quiet finish.
Run after UICharacterDetail. No screenshot is used as runtime composite.
"""
import unreal,json
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir());ns={}
exec((ROOT/'Tools/UICharacterPolish/apply.py').read_text(encoding='utf-8-sig').rsplit('apply()',1)[0],ns)
ART='/Game/Developers/MinkiCho/Art/CharacterMenu/FinalReference20261014';ns['ART']=ART
mat=ns['material'];rgb=ns['rgb'];image=ns['image_brush'];rect=ns['rect'];alpha=ns['alpha']
navy=ns['NAVY'];ivory=ns['IVORY'];gold=ns['GOLD'];mist=ns['MIST'];cream=ns['CREAM']
E=unreal.EditorAssetLibrary;M=unreal.MaterialEditingLibrary;W=unreal.WidgetService
assert not W.is_pie_running()
for name in ['T_CharacterBlueMist','T_CharacterRichOrbit']:
 if not E.does_asset_exist(ART+'/'+name):
  success=unreal.AssetDiscoveryService.import_texture(str(ROOT/'ArtSource/UI/CharacterFinalReference_20261014'/(name+'.png')),ART+'/'+name);assert success,name
 t=unreal.load_asset(ART+'/'+name);assert t,name
 t.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON);t.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS);t.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI);E.save_loaded_asset(t)

def textured(name,texture,code):
 p=ART+'/'+name;t=unreal.load_asset(p)
 if not t:t=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,ART,unreal.Material,unreal.MaterialFactoryNew())
 t.modify();M.delete_all_material_expressions(t);t.set_editor_property('material_domain',unreal.MaterialDomain.MD_UI);t.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
 uv=M.create_material_expression(t,unreal.MaterialExpressionTextureCoordinate,-600,0)
 tex=M.create_material_expression(t,unreal.MaterialExpressionTextureObject,-600,150);tex.set_editor_property('texture',unreal.load_asset(texture))
 custom=M.create_material_expression(t,unreal.MaterialExpressionCustom,-200,0);inputs=[]
 for n in ['UV','Art']:
  i=unreal.CustomInput();i.set_editor_property('input_name',n);inputs.append(i)
 custom.set_editor_property('inputs',inputs);custom.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT4);custom.set_editor_property('code',code)
 M.connect_material_expressions(uv,'',custom,'UV');M.connect_material_expressions(tex,'',custom,'Art')
 for channels,prop in [('RGB',unreal.MaterialProperty.MP_EMISSIVE_COLOR),('A',unreal.MaterialProperty.MP_OPACITY)]:
  mask=M.create_material_expression(t,unreal.MaterialExpressionComponentMask,100,0 if channels=='RGB' else 150)
  for ch in ['r','g','b','a']:mask.set_editor_property(ch,ch.upper() in channels)
  M.connect_material_expressions(custom,'',mask,'');M.connect_material_property(mask,'',prop)
 M.recompile_material(t);E.save_loaded_asset(t);return t

orbit=textured('M_RichRearOrbit',ART+'/T_CharacterRichOrbit','float4 c=Texture2DSample(Art,ArtSampler,UV);float highlight=smoothstep(.65,.95,dot(c.rgb,float3(.3,.5,.2)));return float4(lerp('+rgb(gold)+','+rgb(cream)+',highlight*.6),c.a*.94);')
backdrop=textured('M_GlobalBlueMist',ART+'/T_CharacterBlueMist','float4 c=Texture2DSample(Art,ArtSampler,UV);float lum=dot(c.rgb,float3(.2126,.7152,.0722));float3 cool=lerp(lum.xxx,c.rgb,.54);return float4(lerp(cool,'+rgb(mist)+',.10),1);')
haze=textured('M_FarBlueHaze',ART+'/T_CharacterBlueMist','float4 c=Texture2DSample(Art,ArtSampler,float2(.08+UV.x*.65,.16+UV.y*.76));float edge=smoothstep(0,.07,min(min(UV.x,1-UV.x),min(UV.y,1-UV.y)));return float4(c.rgb,edge*.22);')
chamfer='float2 sz=1/max(fwidth(UV),float2(.00001,.00001));float2 p=UV*sz;float2 e=min(p,sz-p);float d=min(min(e.x,e.y),(e.x+e.y-11)*.7071);float a=saturate(d+.5);'
panel=mat('M_FloatingInfoPanel',chamfer+'float edge=1-smoothstep(.8,1.8,d);float inset=(1-smoothstep(.4,1.2,abs(d-4)))*.47;float corner=(1-smoothstep(.5,1.2,abs(e.x+e.y-22)))*step(max(e.x,e.y),25);float3 c=lerp('+rgb(ivory)+','+rgb(mist)+',.14*UV.y);return float4(lerp(c,'+rgb(gold)+',max(edge,max(inset,corner))),a);')
header=mat('M_FloatingNavyHeader',chamfer+'float edge=1-smoothstep(.8,1.8,d);float inset=(1-smoothstep(.4,1.2,abs(d-4)))*.42;float3 c='+rgb(navy)+'*(.86+.24*(1-UV.y));return float4(lerp(c,'+rgb(gold)+',max(edge,inset)),a);')
plinth=mat('M_MirrorDisplayPlinth',r'''float2 q=(UV-float2(.5,.24))/float2(.44,.20);float r=length(q);float aa=max(fwidth(r),.0001);float top=1-smoothstep(1-aa,1+aa,r);float r2=length((UV-float2(.5,.50))/float2(.44,.20));float side=(1-smoothstep(1-aa,1+aa,r2))*(1-top)*step(.24,UV.y);float rim=1-smoothstep(aa*.2,aa*1.5,abs(r-1));float ring=(1-smoothstep(aa*.2,aa*1.5,abs(r-.86)))*top;float rb=(1-smoothstep(aa*.2,aa*1.5,abs(r2-1)))*step(.5,UV.y);float sheen=pow(.5+.5*cos(UV.x*52),8)*.055;float3 c=lerp('''+rgb(mist)+','+rgb(navy)+r''',side*(.34+.16*UV.y));c+=sheen*side;float lines=max(rim,max(ring*.68,rb*.52));c=lerp(c,lerp('''+rgb(gold)+','+rgb(cream)+r''',rim*.38),lines);return float4(c,max(max(top*.39,side*.44),lines*.94));''')
front=mat('M_FrontIntersectingOrbits',r'''float2 sz=1/max(fwidth(UV),float2(.00001,.00001));float2 p=UV*sz;float2 q=UV-float2(.565,.35);float outa=0;for(int i=0;i<3;i++){float angle=(i-1)*.06;float2 v=float2(q.x*cos(angle)-q.y*sin(angle),q.x*sin(angle)+q.y*cos(angle));float r=length(v/float2(.415-i*.055,.24+i*.10));float aa=max(fwidth(r),.0001);float core=1-smoothstep(aa*.2,aa*1.4,abs(r-1));outa=max(outa,core*(.82-i*.16));}return float4('''+rgb(gold)+r''',outa);''')
star=mat('M_LightStar',r'''float2 q=(UV-.5)*2;float r=length(q);float shape=abs(q.x)*abs(q.y)+.10*(abs(q.x)+abs(q.y));float aa=max(fwidth(shape),.002);float core=1-smoothstep(.082-aa,.082+aa,shape);float halo=exp(-r*r*10)*.22;return float4(lerp('''+rgb(gold)+','+rgb(cream)+r''',core*.8),max(core*.94,halo));''')
dust=mat('M_LightDust',r'''float r=length((UV-.5)*2);float core=1-smoothstep(.18,.36,r);float halo=exp(-r*r*7)*.34;return float4(lerp('''+rgb(gold)+','+rgb(cream)+r''',core),max(core*.94,halo));''')
contact=mat('M_ContactShadow',r'''float2 q=(UV-.5)/float2(.48,.38);return float4('''+rgb(navy)+r''',exp(-dot(q,q)*3)*.17);''')

def skin(stem):
 p=ns['FOLDER']+'WBP_Character'+stem;bp=unreal.load_asset(p);bp.modify();tree=unreal.find_object(bp,'WidgetTree');o={s.widget_name:unreal.find_object(tree,s.widget_name) for s in W.get_widget_snapshot(p)};return p,tree,o
def add(p,tree,name,parent,x,y,w,h,res,z=0):
 obj=unreal.find_object(tree,name)
 if not obj:
  result=W.add_component(p,'Image',name,parent,False);assert result.success,result.export_text();obj=unreal.find_object(tree,name)
 obj.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE);rect(obj,x,y,w,h);obj.slot.set_z_order(z);image(obj,unreal.LinearColor(1,1,1,1),res);return obj
def save(p):
 r=unreal.BlueprintService.compile_blueprint(p);assert r.success,r.export_text();assert E.save_asset(p,False)

p,tree,o=skin('MenuBase')
image(o['IvorySurface'],unreal.LinearColor(1,1,1,1),backdrop)
for n,w in o.items():
 if n.startswith(('Leaf','AmbientLeaves')) or n=='BackdropAtmosphere':w.set_visibility(unreal.SlateVisibility.COLLAPSED)
# Keep original representative artwork until its separately supplied exact asset is identified.
o['RailCompass'].set_render_opacity(1)
for n,w in o.items():
 if n.startswith('GoldFrame'):image(w,alpha(gold,.96))
save(p)

p,tree,o=skin('InfoPage')
o['DepthBase'].set_render_opacity(0)
image(o['DepthFarTexture'],unreal.LinearColor(1,1,1,1),haze)
image(o['DepthRearOrbit'],unreal.LinearColor(1,1,1,1),orbit);rect(o['DepthRearOrbit'],88,-12,544,544);o['DepthRearOrbit'].slot.set_z_order(1)
o['CharacterIllustration'].set_color_and_opacity(unreal.LinearColor(1,1,1,1));o['CharacterIllustration'].set_render_opacity(.31)
image(o['MirrorFloorSurface'],unreal.LinearColor(1,1,1,1),plinth);rect(o['MirrorFloorSurface'],80,385,560,188);o['MirrorFloorSurface'].set_render_opacity(1)
rect(o['CharacterReflection'],110.4,430,499.2,128);o['CharacterReflection'].set_render_opacity(.62)
add(p,tree,'ReferenceContactShadow','PortraitFloorLayer',295,427,130,17,contact,2)
image(o['DepthFrontLine'],unreal.LinearColor(1,1,1,1),front);rect(o['DepthFrontLine'],0,390,608,200);o['DepthFrontLine'].set_render_opacity(1)
# Every point has its own transform/alpha and can be animated separately.
points=[(70,100,18,34),(83,208,10,10),(99,323,22,22),(120,413,12,12),(142,115,8,8),(160,374,10,10),(196,282,12,12),(236,61,8,8),(354,46,22,32),(408,118,10,10),(471,161,10,10),(542,207,18,30),(565,280,10,10),(558,356,12,12),(536,392,17,17),(548,501,13,13),(445,547,9,9),(353,520,30,48),(268,482,8,8),(146,520,10,10),(76,550,14,14),(67,447,8,8),(477,474,8,8),(507,91,8,8),(374,390,8,8),(574,125,9,9),(206,457,8,8),(486,305,10,10)]
for i,(x,y,w,h) in enumerate(points):
 isstar=i in [0,2,8,11,14,17,21,25]
 if not isstar:w=max(w,12);h=max(h,12)
 add(p,tree,'ReferenceLight_%02d'%i,'DepthExhibit',x-w/2,y-h/2,w,h,star if isstar else dust,3)
image(o['InfoTableSurface'],unreal.LinearColor(1,1,1,1),panel)
image(o['InfoRibbonFill'],unreal.LinearColor(1,1,1,1),header)
o['PolishShadow_InfoTableSurface'].set_render_opacity(1)
save(p)
(ROOT/'Saved/UICharacterFinalReference/layers.json').write_text(json.dumps({'background':ART+'/T_CharacterBlueMist','rear_orbit':ART+'/T_CharacterRichOrbit','independent_lights':len(points),'fixed_pixel_character':True,'reflection_footline':430,'reference_emblem':'awaiting exact separately supplied asset; existing unchanged symbol retained'},indent=2))
print('RICH_REFERENCE_SAVED: two WBPs, cool mist, rich rear/front orbits, 28 independent lights, mirror stage, detailed card')
