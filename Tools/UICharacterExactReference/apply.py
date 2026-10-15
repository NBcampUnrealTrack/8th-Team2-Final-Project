"""Reference-led character information with a responsive stage (minimum 4:3).
Frame/emblem/icons/static labels sample the original atlas; the surrounding
atmosphere is composed independently. Actual portrait/data remain live, with
the portrait resized about its original foot anchor and original pixels intact.
"""
import unreal,json
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir());ns={}
exec((ROOT/'Tools/UICharacterPolish/apply.py').read_text(encoding='utf-8-sig').rsplit('apply()',1)[0],ns)
ART='/Game/Developers/MinkiCho/Art/CharacterMenu/ExactReference20261014';ns['ART']=ART
E=unreal.EditorAssetLibrary;M=unreal.MaterialEditingLibrary;W=unreal.WidgetService
rect=ns['rect'];image=ns['image_brush'];color=ns['color'];mat=ns['material']
# Header starts one shared inset beyond the enlarged left rail.
header_shift=236*1.1*1.2+24-263
assert not W.is_pie_running()
for name in ['T_ReferenceAtlas','T_ReferenceRepaired','T_CharacterWideAtmosphere','T_CharacterUnifiedBackground']:
 source='ArtSource/UI/CharacterUnifiedBackground_20261014' if name=='T_CharacterUnifiedBackground' else 'ArtSource/UI/CharacterWideAtmosphere_20261014' if name=='T_CharacterWideAtmosphere' else 'ArtSource/UI/CharacterExactReference_20261014'
 if not E.does_asset_exist(ART+'/'+name):assert unreal.AssetDiscoveryService.import_texture(str(ROOT/source/(name+'.png')),ART+'/'+name)
 t=unreal.load_asset(ART+'/'+name);assert t,name
 t.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON);t.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS);t.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI);E.save_loaded_asset(t)

def textured(name,code):
 p=ART+'/'+name;t=unreal.load_asset(p)
 if t:
  for expression in unreal.MaterialNodeService.list_expressions(p):
   if expression.class_name=='MaterialExpressionCustom' and unreal.MaterialNodeService.get_expression_property(p,expression.id,'Code').strip('"')==code:
    return t
 if not t:t=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,ART,unreal.Material,unreal.MaterialFactoryNew())
 t.modify();M.delete_all_material_expressions(t);t.set_editor_property('material_domain',unreal.MaterialDomain.MD_UI);t.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
 uv=M.create_material_expression(t,unreal.MaterialExpressionTextureCoordinate,-600,0)
 custom=M.create_material_expression(t,unreal.MaterialExpressionCustom,-200,0);inputs=[]
 for key in ['UV','Art','Repair','Wide','Unified']:
  i=unreal.CustomInput();i.set_editor_property('input_name',key);inputs.append(i)
 custom.set_editor_property('inputs',inputs);custom.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT4);custom.set_editor_property('code',code)
 M.connect_material_expressions(uv,'',custom,'UV')
 for i,(key,texture) in enumerate([('Art','T_ReferenceAtlas'),('Repair','T_ReferenceRepaired'),('Wide','T_CharacterWideAtmosphere'),('Unified','T_CharacterUnifiedBackground')]):
  tex=M.create_material_expression(t,unreal.MaterialExpressionTextureObject,-600,150+i*160);tex.set_editor_property('texture',unreal.load_asset(ART+'/'+texture));M.connect_material_expressions(tex,'',custom,key)
 for channels,prop in [('RGB',unreal.MaterialProperty.MP_EMISSIVE_COLOR),('A',unreal.MaterialProperty.MP_OPACITY)]:
  mask=M.create_material_expression(t,unreal.MaterialExpressionComponentMask,100,0 if channels=='RGB' else 150)
  for ch in ['r','g','b','a']:mask.set_editor_property(ch,ch.upper() in channels)
  M.connect_material_expressions(custom,'',mask,'');M.connect_material_property(mask,'',prop)
 M.recompile_material(t);E.save_loaded_asset(t);return t

def skin(stem):
 p=ns['FOLDER']+'WBP_Character'+stem;bp=unreal.load_asset(p);bp.modify();tree=unreal.find_object(bp,'WidgetTree');o={s.widget_name:unreal.find_object(tree,s.widget_name) for s in W.get_widget_snapshot(p)};return p,tree,o
def add(p,tree,cls,name,parent):
 o=unreal.find_object(tree,name)
 if not o:
  r=W.add_component(p,cls,name,parent,False);assert r.success,r.export_text();o=unreal.find_object(tree,name)
 o.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE if cls in ['Image','TextBlock'] else unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE);return o
def tile(p,tree,name,parent,bounds,res,z=0):
 w=add(p,tree,'Image',name,parent);rect(w,*bounds);w.slot.set_z_order(z);image(w,unreal.LinearColor(1,1,1,1),res);return w
def save(p):
 r=unreal.BlueprintService.compile_blueprint(p);assert r.success,r.export_text();assert E.save_asset(p,False)

p,tree,o=skin('MenuBase')
fill=add(p,tree,'Image','ReferenceViewportFill','RootCanvas');fill.slot.set_anchors(unreal.Anchors(unreal.Vector2D(),unreal.Vector2D(1,1)));fill.slot.set_offsets(unreal.Margin());fill.slot.set_z_order(4);image(fill,color('1E3443'));fill.set_visibility(unreal.SlateVisibility.COLLAPSED)
viewport=add(p,tree,'ScaleBox','ReferenceViewport','RootCanvas');viewport.slot.set_anchors(unreal.Anchors(unreal.Vector2D(),unreal.Vector2D(1,1)));viewport.slot.set_offsets(unreal.Margin());viewport.slot.set_z_order(5);viewport.set_editor_property('stretch',unreal.Stretch.SCALE_TO_FIT);viewport.set_visibility(unreal.SlateVisibility.COLLAPSED)
size=add(p,tree,'SizeBox','ReferenceDesignSize','ReferenceViewport');size.set_width_override(1448);size.set_height_override(1086)
controls=add(p,tree,'CanvasPanel','ReferenceControls','ReferenceDesignSize')
save(p)

# Independently drawn original static elements. Their tiles keep original AA/background
# pixels and are never recolored/reinterpreted. The base leaves corresponding holes.
elements=[('Emblem',(25,24,183,195)),('ProfileEmblem',(263,31,132,135)),('SelectedArrow',(8,228,247,76)),
 ('CharacterIcon',(27,243,55,49)),('CharacterLabel',(87,245,132,44)),('EquipmentIcon',(27,329,55,66)),('EquipmentLabel',(96,337,121,51)),
 ('InventoryIcon',(27,426,54,68)),('InventoryLabel',(97,434,126,48)),('StatsIcon',(25,529,60,58)),('StatsLabel',(99,533,108,48)),
 ('TraitsIcon',(21,622,65,63)),('TraitsLabel',(95,631,120,50)),('NaiaIcon',(20,723,66,60)),('NaiaLabel',(95,730,120,50)),
 ('Close',(1344,31,74,75)),('CardTitle',(907,200,270,83)),('NameIcon',(934,323,60,57)),('NameLabel',(1006,326,98,48)),
 ('ExpIcon',(930,410,67,65)),('ExpLabel',(1006,416,98,48)),('LocationIcon',(935,501,57,58)),('LocationLabel',(1006,507,111,48)),
 ('IdIcon',(935,594,57,64)),('IdLabel',(1005,601,111,48)),('GoldIcon',(935,680,57,60)),('GoldLabel',(1006,688,111,48)),
 ('CopyId',(1355,602,43,45)),('Currency',(1350,681,50,55))]

def mask(x,y,w,h,feather=0):
 if feather:return f'(smoothstep({x},{x+feather},p.x)*(1-smoothstep({x+w-feather},{x+w},p.x))*smoothstep({y},{y+feather},p.y)*(1-smoothstep({y+h-feather},{y+h},p.y)))'
 return f'(step({x},p.x)*step(p.x,{x+w})*step({y},p.y)*step(p.y,{y+h}))'

def uvcode(bounds):
 x,y,w,h=bounds;return f'float2 p=float2({x},{y})+UV*float2({w},{h});float2 uv=p/float2(1448,1086);float4 c=Texture2DSample(Art,ArtSampler,uv);'

dynamic=[(408,54,287,62),(408,114,169,45),(595,114,108,45),(1124,326,266,51),(1110,407,280,39),(1125,507,266,50),(1110,597,238,51),(1170,683,172,53)]
base=uvcode((0,0,1448,1086))
particles=[(370,335,12),(336,371,10),(321,502,10),(398,529,11),(418,594,9),(795,550,14),(849,412,11),(872,461,11),(392,650,9),(300,708,8),(304,951,11),(842,968,8)]
repairmask='max('+mask(440,313,321,488,8)+','+mask(440,787,336,173,8)+')'
base+='float repaired='+repairmask+';float4 r=Texture2DSample(Repair,RepairSampler,uv);c=lerp(c,r,repaired);'
for x,y,w,h in dynamic:
 if y<160:
  cleanY=42 if y<100 else 173
  clean=f'Texture2DSample(Art,ArtSampler,float2(p.x,{cleanY})/float2(1448,1086)).rgb'
 else:clean='Texture2DSample(Art,ArtSampler,float2(1397,p.y)/float2(1448,1086)).rgb'
 base+='c.rgb=lerp(c.rgb,'+clean+','+mask(x,y,w,h,1)+');'
# Remove the reference's baked EXP fill, retaining its original rounded outline.
base+='float2 tq=abs(p-float2(1250,455))-(float2(145,10)-8);float td=length(max(tq,0))+min(max(tq.x,tq.y),0)-8;float emptyTrack=1-smoothstep(-.5,.5,td);c.rgb=lerp(c.rgb,Texture2DSample(Art,ArtSampler,float2(1310,p.y)/float2(1448,1086)).rgb,emptyTrack);'
for x,y,r in particles:
 clean=f'(Texture2DSample(Art,ArtSampler,float2({x-r-4},{y})/float2(1448,1086)).rgb+Texture2DSample(Art,ArtSampler,float2({x+r+4},{y})/float2(1448,1086)).rgb)*.5'
 base+=f'c.rgb=lerp(c.rgb,{clean},1-smoothstep({r-2},{r+2},length(p-float2({x},{y}))));'
# Static element tiles cover separate, original-pixel regions. Nested arrow/icon
# roles can overlap; they sample identical pixels at identical coordinates.
base+='float separate=0;'
# Keep a two-source-pixel overlap under each tile, avoiding exposed one-pixel
# holes when Slate rounds the independent tile and base to different pixels.
for name,(x,y,w,h) in elements:
 inset=0 if name=='SelectedArrow' else 2
 base+='separate=max(separate,'+mask(x+inset,y+inset,w-2*inset,h-2*inset)+');'
base+='return float4(c.rgb,1-separate);'
baseMaterial=textured('M_ExactBase',base)

p,tree,o=skin('InfoPage')
exact=add(p,tree,'CanvasPanel','ReferenceExactCanvas','PageCanvas');rect(exact,0,0,1448,1086);exact.slot.set_z_order(10)
for n,w in o.items():
 if w.get_parent() and w.get_parent().get_name()=='PageCanvas' and n!='ReferenceExactCanvas':w.set_visibility(unreal.SlateVisibility.COLLAPSED)
tile(p,tree,'ReferenceSourceBase','ReferenceExactCanvas',(0,0,1448,1086),baseMaterial,-10)
for name,b in elements:
 code=uvcode(b)+'return c;'
 if name=='ProfileEmblem':
  code=uvcode(b)+'float3 bg=Texture2DSample(Art,ArtSampler,float2(270,35)/float2(1448,1086)).rgb;float ink=max(max(abs(c.r-bg.r),abs(c.g-bg.g)),abs(c.b-bg.b));return float4(c.rgb,smoothstep(.025,.15,ink));'
 if name=='Close':
  code=uvcode(b)+'float diamond=abs(p.x-1381)+abs(p.y-68.5)-36.5;return float4(c.rgb,1-smoothstep(-.7,.7,diamond));'
 if name=='CardTitle':
  # One inset header contour; erase both baked atlas rims before drawing it.
  code=uvcode(b)+'float2 q=abs(p-float2(1042,241.5))-float2(135,41.5);float d=max(max(q.x,q.y),(q.x+q.y+14)*.70710678);float rim=smoothstep(-12,-10,d);float3 navy=Texture2DSample(Art,ArtSampler,float2(930,220)/float2(1448,1086)).rgb;c.rgb=lerp(c.rgb,navy,rim);float stroke=1-smoothstep(.3,1.1,abs(d+1));return float4(lerp(c.rgb,float3(.4793,.3663,.1746),stroke),1-smoothstep(-.5,.5,d));'
 if b[0]<236 and name not in ['SelectedArrow','CharacterIcon','CharacterLabel']:
  code=uvcode(b)+'float lum=dot(c.rgb,float3(.2126,.7152,.0722));return float4(c.rgb,smoothstep(.12,.40,lum));'
 if name=='SelectedArrow':
  # Widen the face independently: never stretch its icon or Korean lettering.
  code=uvcode(b)+'float edge=max(max(228-p.y,p.y-304),p.x-(255-abs(p.y-266)*26/38));float3 face=Texture2DSample(Art,ArtSampler,float2(150,235)/float2(1448,1086)).rgb;float stroke=1-smoothstep(.3,1.2,abs(edge+1));return float4(lerp(face,float3(.4793,.3663,.1746),stroke),1-smoothstep(-.5,.5,edge));'
 res=textured('M_Exact_'+name,code)
 source_tile=tile(p,tree,'ReferenceSource_'+name,'ReferenceExactCanvas',b,res,-1 if name=='SelectedArrow' else 0)
 # Keep the widened navigation opaque over independently moving exhibit glints.
 if b[0]<236:source_tile.slot.set_z_order(4 if name=='SelectedArrow' else 5)
 if name=='ProfileEmblem':source_tile.slot.set_position(unreal.Vector2D(b[0]+header_shift,b[1]))

# Center the icon + label group as a unit, with a shared gap and row center.
for prefix,cy in [('Character',266),('Equipment',362),('Inventory',463),('Stats',565),('Traits',666),('Naia',771)]:
 icon=unreal.find_object(tree,'ReferenceSource_'+prefix+'Icon');label=unreal.find_object(tree,'ReferenceSource_'+prefix+'Label')
 iw=icon.slot.get_size();lw=label.slot.get_size();left=(236-iw.x-12-lw.x)*.5
 icon.slot.set_position(unreal.Vector2D(left,cy-iw.y*.5));label.slot.set_position(unreal.Vector2D(left+iw.x+12,cy-lw.y*.5))

# Move the original live values, preserving their names/delegates. Unused card
# job/level values remain in the old hidden tree and feed the live profile mirrors.
valueBounds={'NameText':(1120,333,261,50),'ExperienceText':(1100,406,282,43),'LocationText':(1120,515,265,43),'CharacterIdText':(1100,608,241,43),'GoldText':(1130,692,207,43)}
for name,b in valueBounds.items():
 w=o[name];w.remove_from_parent();exact.add_child_to_canvas(w);rect(w,*b);w.slot.set_z_order(8);w.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
 f=w.get_editor_property('font').copy();f.font_object=unreal.load_asset('/Engine/EngineFonts/Roboto');f.size=19 if name=='CharacterIdText' else 20;w.set_font(f)
 w.set_color_and_opacity(unreal.SlateColor(specified_color=color('143455')));w.set_editor_property('justification',unreal.TextJustify.RIGHT)
 w.set_editor_property('text_overflow_policy',unreal.TextOverflowPolicy.ELLIPSIS)
for name,b,source,size in [('RefProfileName',(416,62,282,57),'NameText',30),('RefProfileJob',(416,120,163,44),'JobText',19),('RefProfileLevel',(599,120,108,44),'LevelText',19)]:
 w=add(p,tree,'TextBlock',name,'ReferenceExactCanvas');rect(w,b[0]+header_shift,b[1],b[2],b[3]);w.slot.set_z_order(8)
 f=o[source].get_editor_property('font').copy();f.font_object=unreal.load_asset('/Game/Art/Font/MaruBuri-SemiBold_Font') if name=='RefProfileName' else unreal.load_asset('/Engine/EngineFonts/Roboto');f.size=size;w.set_font(f);w.set_color_and_opacity(unreal.SlateColor(specified_color=color('1E3443')));w.set_editor_property('text_overflow_policy',unreal.TextOverflowPolicy.ELLIPSIS)

# Scale about the authored foot position, not the top-left corner. The original
# 384:512 render-target aspect and the shared reflection center stay unchanged.
portrait_scale=.82;portrait_width=705*portrait_scale;portrait_height=940*portrait_scale
portrait_left=595-portrait_width*.5;portrait_top=795-(795+31.166)*portrait_scale
portrait=o['CharacterPortrait'];portrait.remove_from_parent();exact.add_child_to_canvas(portrait);rect(portrait,portrait_left,portrait_top,portrait_width,portrait_height);portrait.slot.set_z_order(5);portrait.set_render_transform(unreal.WidgetTransform());portrait.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
fallback=o['PortraitFallback'];fallback.remove_from_parent();exact.add_child_to_canvas(fallback);rect(fallback,595-170*portrait_scale*.5,795-240*portrait_scale,170*portrait_scale,240*portrait_scale);fallback.slot.set_z_order(5);fallback.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
reflection=o['CharacterReflection'];reflection.remove_from_parent();exact.add_child_to_canvas(reflection);rect(reflection,portrait_left,795,portrait_width,166*portrait_scale);reflection.slot.set_z_order(4);reflection.set_render_opacity(.5);reflection.set_render_transform(unreal.WidgetTransform());reflection.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
bar=o['ExperienceBar'];bar.remove_from_parent();exact.add_child_to_canvas(bar);rect(bar,1108,448,284,14);bar.slot.set_z_order(8);bar.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
st=bar.get_editor_property('widget_style').copy();br=st.background_image.copy();br.draw_as=unreal.SlateBrushDrawType.NO_DRAW_TYPE;st.background_image=br
br=st.fill_image.copy();br.tint_color=unreal.SlateColor(specified_color=color('B99A60'));st.fill_image=br;bar.set_editor_property('widget_style',st)
copy=add(p,tree,'Button','RefCopyIdButton','ReferenceExactCanvas');rect(copy,1351,598,51,52);copy.slot.set_z_order(12);copy.set_visibility(unreal.SlateVisibility.VISIBLE)
st=copy.get_editor_property('widget_style').copy()
for state in ['normal','hovered','pressed','disabled']:
 br=st.get_editor_property(state).copy();br.draw_as=unreal.SlateBrushDrawType.NO_DRAW_TYPE;st.set_editor_property(state,br)
copy.set_style(st)
# Full-width presentation: keep the original rail, exhibit, and card at uniform
# scale. Only the neutral background and horizontal perimeter span the new width.
unreal.find_object(tree,'ReferenceSourceBase').set_visibility(unreal.SlateVisibility.COLLAPSED)
mist=ns['rgb'](color('D5DFE4'));blue_ivory=ns['rgb'](color('E9EEED'));navy=ns['rgb'](color('1E3443'))
back=uvcode((0,0,1448,1086))+f'''float4 b=Texture2DSample(Art,ArtSampler,float2(1428,p.y)/float2(1448,1086));
float body=smoothstep(.012,.032,UV.y)*(1-smoothstep(.967,.988,UV.y));
float ribbon=exp(-pow((UV.y-(.19+.44*UV.x))/.23,2));
float lower=exp(-dot((UV-float2(.67,.85))/float2(.72,.62),(UV-float2(.67,.85))/float2(.72,.62)));
float3 atmosphere=lerp({mist},{blue_ivory},.22+.37*ribbon);
atmosphere=lerp(atmosphere,{navy},.035*lower);
float2 focal=(UV-float2(.35,.52))/float2(.34,.44);
atmosphere=lerp(atmosphere,{blue_ivory},.28*exp(-dot(focal,focal)*2));
return float4(lerp(b.rgb,atmosphere,body*.88),1);'''
tile(p,tree,'ReferenceResponsiveBackground','ReferenceExactCanvas',(236,0,1212,1086),textured('M_ResponsiveBackground',back),-20)
air_particles=[(217,154,30),(318,207,16),(177,389,30),(402,516,30),(1178,266,30),(1394,446,30),(1426,63,24),(1542,266,25)]
wide='float4 c=Texture2DSample(Wide,WideSampler,UV);float2 p=UV*float2(1672,941);'
for x,y,r in air_particles:
 bg=f'(Texture2DSample(Wide,WideSampler,float2({x-r-6},{y})/float2(1672,941)).rgb+Texture2DSample(Wide,WideSampler,float2({x+r+6},{y})/float2(1672,941)).rgb)*.5'
 wide+=f'c.rgb=lerp(c.rgb,{bg},1-smoothstep({r-3},{r+3},length(p-float2({x},{y}))));'
wide+='float perimeter=smoothstep(.013,.037,UV.y)*(1-smoothstep(.965,.992,UV.y));float header=.40+.48*smoothstep(.14,.24,UV.y);return float4(c.rgb,perimeter*header);'
tile(p,tree,'ReferenceWideAtmosphere','ReferenceExactCanvas',(236,0,1212,1086),textured('M_ReferenceWideAtmosphere',wide),-18)
for i,(x,y,r) in enumerate(air_particles):
 diameter=(r+4)*2
 bg=f'(Texture2DSample(Wide,WideSampler,float2({x-r-6},{y})/float2(1672,941)).rgb+Texture2DSample(Wide,WideSampler,float2({x+r+6},{y})/float2(1672,941)).rgb)*.5'
 code=f'float2 p=float2({x-r-4},{y-r-4})+UV*{diameter};float4 c=Texture2DSample(Wide,WideSampler,p/float2(1672,941));float3 bg={bg};float light=max(max(c.r-bg.r,c.g-bg.g),c.b-bg.b);float a=smoothstep(.015,.20,light)*(1-smoothstep({r},{r+4},length(p-float2({x},{y}))));float header=.40+.48*smoothstep(.14,.24,p.y/941);return float4(c.rgb,a*header);'
 size=diameter*1086/941
 bounds=(236+x/1672*1212-size*.5,y/941*1086-size*.5,size,size)
 tile(p,tree,'ReferenceAirParticle_%02d'%i,'ReferenceExactCanvas',bounds,textured('M_ReferenceAirParticle_%02d'%i,code),-16)
for name,bounds in [('ReferenceRailBase',(0,0,236,1086)),('ReferenceExhibitBase',(236,0,944,1086)),('ReferenceCardBase',(906,0,542,1086))]:
 code=base.replace(uvcode((0,0,1448,1086)),uvcode(bounds),1)
 if name=='ReferenceExhibitBase':
  code=code.replace('return float4(c.rgb,1-separate);','float floorReveal=smoothstep(791,811,p.y);float rightLimit=lerp(906,1180,floorReveal);float fade=smoothstep(236,266,p.x)*(1-smoothstep(rightLimit-24,rightLimit,p.x));float excludeCard=1-step(906,p.x)*(1-floorReveal);float3 bg=Texture2DSample(Art,ArtSampler,float2(1428,p.y)/float2(1448,1086)).rgb;float ink=max(max(abs(c.r-bg.r),abs(c.g-bg.g)),abs(c.b-bg.b));float neutral=1-smoothstep(.10,.28,ink);c.rgb=lerp(c.rgb,c.rgb*float3(.94,.985,1.015),neutral*.4);return float4(c.rgb,(1-separate)*fade*excludeCard*smoothstep(.025,.12,ink)*smoothstep(140,230,p.y));')
 if name=='ReferenceCardBase':
  # A single contour owns the card edge. No sampled inner/outer atlas frames.
  code=uvcode(bounds)+'float2 q=abs(p-float2(1164,494))-float2(257,294);float edge=max(max(q.x,q.y),(q.x+q.y+14)*.70710678);float card=1-smoothstep(-.5,.5,edge);float3 face=Texture2DSample(Art,ArtSampler,float2(1397,clamp(p.y,310,750))/float2(1448,1086)).rgb;float divider=0;'
  for y in [389,486,578,665]:code+=f'divider=max(divider,(1-smoothstep(.2,.8,abs(p.y-{y})))*smoothstep(936,940,p.x)*(1-smoothstep(1390,1394,p.x)));'
  code+='face=lerp(face,float3(.70,.70,.66),divider*.32);float stroke=1-smoothstep(.25,1.0,abs(edge+1));face=lerp(face,float3(.4793,.3663,.1746),stroke);return float4(face,card);'
 tile(p,tree,name,'ReferenceExactCanvas',bounds,textured('M_'+name,code),-10)
tile(p,tree,'ReferenceOuterRightFrame','ReferenceExactCanvas',(1437,0,11,1086),textured('M_ReferenceOuterRightFrame',uvcode((1437,0,11,1086))+'return c;'),-10)
for i,(x,y,r) in enumerate(particles):
 bounds=(x-r-3,y-r-3,(r+3)*2,(r+3)*2)
 bg=f'(Texture2DSample(Art,ArtSampler,float2({x-r-4},{y})/float2(1448,1086)).rgb+Texture2DSample(Art,ArtSampler,float2({x+r+4},{y})/float2(1448,1086)).rgb)*.5'
 code=uvcode(bounds)+f'float3 bg={bg};float light=max(max(c.r-bg.r,c.g-bg.g),c.b-bg.b);float a=smoothstep(.015,.13,light)*(1-smoothstep({r},{r+3},length(p-float2({x},{y}))));return float4(c.rgb,a);'
 tile(p,tree,'ReferenceParticle_%02d'%i,'ReferenceExactCanvas',bounds,textured('M_ReferenceParticle_%02d'%i,code),2)
tile(p,tree,'ReferenceHeaderRule','ReferenceExactCanvas',(698+header_shift,97,618,3),textured('M_ReferenceHeaderRule',uvcode((698,97,618,3))+'return c;'),0)
# Only one opaque static-background image is drawn. Its panoramic source already
# joins the illustration, reflective floor, and structural orbit without seams.
# Avatar, its live reflection, all twenty animated glints and dust stay separate.
for name in ['ReferenceResponsiveBackground','ReferenceWideAtmosphere','ReferenceExhibitBase','ReferenceOuterRightFrame']:
 unreal.find_object(tree,name).set_visibility(unreal.SlateVisibility.COLLAPSED)
tile(p,tree,'ReferenceUnifiedBackground','ReferenceExactCanvas',(0,0,3342,1114),textured('M_ReferenceUnifiedBackground','return Texture2DSample(Unified,UnifiedSampler,UV);'),-20)
exact.set_editor_property('clipping',unreal.WidgetClipping.CLIP_TO_BOUNDS)
gold=ns['rgb'](color('B9A374'))
rail='float2 p=UV*float2(236,1086);float4 c=Texture2DSample(Art,ArtSampler,float2(clamp(p.x,18,218),850+UV.y*180)/float2(1448,1086));float border=smoothstep(234.4,235.2,p.x);return float4(lerp(c.rgb,'+gold+',border*.72),1);'
tile(p,tree,'ReferenceRailBase','ReferenceExactCanvas',(0,0,236,1086),textured('M_ReferenceRailSurface',rail),3)
separator=mat('M_ReferenceMenuSeparator','return float4('+gold+',.42*smoothstep(0,.03,UV.x)*(1-smoothstep(.97,1,UV.x)));')
for i,y in enumerate([407,506,603,701]):tile(p,tree,'ReferenceMenuSeparator_%02d'%i,'ReferenceExactCanvas',(23,y,190,1.2),separator,4)
frame=mat('M_ReferenceViewportFrame','float2 wh=1/max(fwidth(UV),float2(.000001,.000001));float2 q=abs(UV*wh-wh*.5)-(wh*.5-float2(6,7));float d=max(max(q.x,q.y),(q.x+q.y+8)*.70710678);return float4('+gold+',1-smoothstep(.2,1.1,abs(d)));')
tile(p,tree,'ReferenceViewportFrame','ReferenceExactCanvas',(0,0,1448,1086),frame,15)
dust='float2 px=float2(1.6/1672,1.6/941);float3 c=Texture2DSample(Wide,WideSampler,UV).rgb;float3 blur=(Texture2DSample(Wide,WideSampler,UV+float2(px.x,0)).rgb+Texture2DSample(Wide,WideSampler,UV-float2(px.x,0)).rgb+Texture2DSample(Wide,WideSampler,UV+float2(0,px.y)).rgb+Texture2DSample(Wide,WideSampler,UV-float2(0,px.y)).rgb)*.25;float light=max(max(c.r-blur.r,c.g-blur.g),c.b-blur.b);float a=smoothstep(.012,.08,light)*.30*(1-smoothstep(.62,.69,UV.y))*smoothstep(0,.035,UV.x)*(1-smoothstep(.965,1,UV.x));float2 p=UV*float2(1672,941);'
for x,y,r in air_particles:dust+=f'a*=smoothstep({r+3},{r+8},length(p-float2({x},{y})));'
dust+='return float4(c,a);'
tile(p,tree,'ReferenceAtmosphereDust','ReferenceExactCanvas',(236,0,1212,1086),textured('M_ReferenceAtmosphereDust',dust),-16)
save(p)
(ROOT/'Saved/UICharacterExactReference/source_regions.json').write_text(json.dumps({'design_size':[1448,1086],'static_original_tiles':elements,'dynamic_regions':dynamic,'repair_only':[440,313,321,488,440,787,336,173],'other_tabs':'runtime restoration of original controls and named slot'},indent=2))
print('EXACT_REFERENCE_ASSETS_SAVED: original-pixel visual roles + live portrait/data')
