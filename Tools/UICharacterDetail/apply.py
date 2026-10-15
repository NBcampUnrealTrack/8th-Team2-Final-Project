"""Reference-led character information and shared shell detail pass. Run after depth.
Preserves widget identities, runtime page switching, portrait RT and motion owners.
"""
import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir())
ns={};exec((root/'Tools/UICharacterPolish/apply.py').read_text(encoding='utf-8-sig').rsplit('apply()',1)[0],ns)
ns['ART']='/Game/Developers/MinkiCho/Art/CharacterMenu/Detail20261014'
material=ns['material'];image=ns['image_brush'];rect=ns['rect'];rgb=ns['rgb'];alpha=ns['alpha']
navy=ns['NAVY'];ivory=ns['IVORY'];gold=ns['GOLD'];cream=ns['CREAM'];mist=ns['MIST']
assert not unreal.WidgetService.is_pie_running()
# Screen-space analytic outlines: corners retain their angle and stroke width.
chamfer='float2 sz=1/max(fwidth(UV),float2(.00001,.00001));float2 p=UV*sz;float2 e=min(p,sz-p);float d=min(min(e.x,e.y),(e.x+e.y-10)*.7071);float a=saturate(d+.5);'
card=material('M_InfoPanel',chamfer+'float stroke=1-smoothstep(.7,1.7,d);float3 c=lerp('+rgb(ivory)+',float3(.99,.97,.92),.08*(1-UV.y));return float4(lerp(c,'+rgb(gold)+',stroke*.82),a);')
body=material('M_InfoBody','float2 sz=1/max(fwidth(UV),float2(.00001,.00001));float2 b=sz*.5;float2 q=abs(UV*sz-b)-(b-8);float d=length(max(q,0))+min(max(q.x,q.y),0)-8;float a=1-smoothstep(-.5,.5,d);float edge=1-smoothstep(.5,1.5,-d);return float4(lerp(lerp('+rgb(ivory)+',float3(1,1,1),.14),'+rgb(gold)+',edge*.16),a);')
tab=material('M_InfoHeader',chamfer+'float stroke=1-smoothstep(.8,1.8,d);float3 c='+rgb(navy)+'*(.88+.15*(1-UV.y));return float4(lerp(c,'+rgb(gold)+',stroke*.9),a);')
rail=material('M_NavyRail','float shade=.8+.22*exp(-pow((UV.x-.45)*2,2))+.025*sin(UV.y*12);return float4('+rgb(navy)+'*shade,1);')
selected=material('M_NavSelected','float2 sz=1/max(fwidth(UV),float2(.00001,.00001));float2 p=UV*sz;float tip=sz.y*.28;float d=min(min(p.x,min(p.y,sz.y-p.y)),(sz.x-p.x-abs(p.y-sz.y*.5)*tip/(sz.y*.5))*.94);float a=saturate(d+.5);float stroke=1-smoothstep(.7,1.7,d);return float4(lerp('+rgb(cream)+'*(.98+.02*(1-UV.y)),'+rgb(gold)+',stroke),a);')
helmet=material('M_EquipmentHelmet',r'''float x=abs(UV.x-.5);float y=UV.y;float hw=y<.29?lerp(.03,.30,saturate((y-.10)/.19)):lerp(.30,.18,saturate((y-.56)/.27));float a=(1-smoothstep(hw-.008,hw+.008,x))*smoothstep(.09,.105,y)*(1-smoothstep(.82,.84,y));float eye=smoothstep(.07,.085,x)*(1-smoothstep(.255,.27,x))*smoothstep(.40,.415,y)*(1-smoothstep(.46,.475,y));float chin=(1-smoothstep(.025,.04,x))*smoothstep(.59,.61,y);float brow=(1-smoothstep(.008,.022,abs(y-(.31+x*.28))))*smoothstep(.055,.08,x);float ridge=(1-smoothstep(.009,.021,x))*smoothstep(.13,.15,y)*(1-smoothstep(.29,.31,y));return float4(1,1,1,a*(1-max(max(eye,chin),max(brow*.8,ridge*.6))));''')
close=material('M_CloseDiamond',r'''float2 sz=1/max(fwidth(UV),float2(.00001,.00001));float2 p=(UV-.5)*sz;float d=(min(sz.x,sz.y)*.5-1-abs(p.x)-abs(p.y))*.7071;float a=saturate(d+.5);float edge=1-smoothstep(.7,1.7,d);return float4(lerp('''+rgb(navy)+','+rgb(gold)+r''',edge),a);''')
stats=material('M_StatsBars',r'''float a=0;float aa=max(fwidth(UV.x),.001);for(int i=0;i<3;i++){float x=.22+i*.23;float h=.39+i*.16;float d=min(.072-abs(UV.x-x),min(UV.y-(.84-h),.84-UV.y));a=max(a,smoothstep(-aa,aa,d));}return float4(1,1,1,a);''')
orbit=material('M_ExhibitOrbit',r'''float2 q=UV-.5;float r=length(q);float aa=max(fwidth(r),.0001);float ring=1-smoothstep(aa*.3,aa*1.5,abs(r-.43));float outer=(1-smoothstep(aa*.2,aa*1.2,abs(r-.485)))*.32;float inner=(1-smoothstep(aa*.2,aa*1.2,abs(r-.36)))*.14;return float4('''+rgb(gold)+r''',max(ring*.66,max(outer,inner)));''')
floor=material('M_DisplayPlinth',r'''float2 q=(UV-float2(.5,.39))/float2(.44,.22);float r=length(q);float aa=max(fwidth(r),.0001);float top=1-smoothstep(1-aa,1+aa,r);float r2=length((UV-float2(.5,.47))/float2(.44,.22));float side=(1-smoothstep(1-aa,1+aa,r2))*(1-top);float rim=1-smoothstep(aa*.3,aa*1.6,abs(r-1));float inner=(1-smoothstep(aa*.3,aa*1.5,abs(r-.85)))*top;float3 face=lerp('''+rgb(mist)+','+rgb(ivory)+r''',.50+.25*(1-UV.y));float3 c=lerp(face,''' +rgb(navy)+r''',side*.34);c=lerp(c,''' +rgb(gold)+r''',max(rim*.84,inner*.45));float shadow=exp(-dot((UV-float2(.5,.57))/float2(.43,.19),(UV-float2(.5,.57))/float2(.43,.19))*3)*.12;return float4(c,max(max(top*.52,side*.46),shadow));''')
exp=material('M_ExperienceTrack',r'''float2 sz=1/max(fwidth(UV),float2(.00001,.00001));float2 p=UV*sz;float2 b=sz*.5;float rad=min(5,b.y);float2 q=abs(p-b)-(b-rad);float d=length(max(q,0))+min(max(q.x,q.y),0)-rad;float a=1-smoothstep(-.5,.5,d);float edge=1-smoothstep(.5,1.5,-d);return float4(lerp('''+rgb(mist)+','+rgb(gold)+r''',edge*.55),a);''')

def skin(stem):
 path=ns['FOLDER']+'WBP_Character'+stem;bp=unreal.load_asset(path);bp.modify();tree=unreal.find_object(bp,'WidgetTree')
 objs={s.widget_name:unreal.find_object(tree,s.widget_name) for s in unreal.WidgetService.get_widget_snapshot(path)}
 return path,bp,tree,objs
def hide(w):
 if w:w.set_visibility(unreal.SlateVisibility.COLLAPSED)
def font(w,size):
 f=w.get_editor_property('font').copy();f.font_object=unreal.load_asset('/Game/Art/Font/MaruBuri-SemiBold_Font');f.size=size;w.set_font(f)
def add(path,tree,name,parent,x,y,w,h,res,z=0):
 obj=unreal.find_object(tree,name)
 if not obj:
  result=unreal.WidgetService.add_component(path,'Image',name,parent,False);assert result.success,result.export_text();obj=unreal.find_object(tree,name)
 obj.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE);rect(obj,x,y,w,h);obj.slot.set_z_order(z);image(obj,unreal.LinearColor(1,1,1,1),res);return obj

path,bp,tree,o=skin('MenuBase')
image(o['NavySurface'],unreal.LinearColor(1,1,1,1),rail)
image(o['MenuTitleSurface'],unreal.LinearColor(1,1,1,1),rail)
o['NavyGrain'].set_render_opacity(.055)
o['BackdropAtmosphere'].set_render_opacity(.065)
for n,w in o.items():
 if n.startswith(('OuterNavyFrame','InnerFrame','FrameCorner')) or n in ['RailFooterCompass','RailFooterLeft','RailFooterRight','TitleCompassOrbit']:hide(w)
 if n.startswith('GoldFrame'):image(w,alpha(gold,.78));w.slot.set_size(unreal.Vector2D(w.slot.get_size().x,1.1)) if w.slot.get_size().y<2 else None
 if n.startswith('Leaf'):w.set_render_opacity(.09)
o['RailCompass'].set_render_opacity(.86);rect(o['RailCompass'],61,33,130,130)
for key in ['Character','Equipment','Inventory','Stats','Traits','Naia']:
 prefix='Nav'+key
 for n,w in o.items():
  if n.startswith(prefix+'Selected') and w.get_name()!=prefix+'Selected':hide(w)
 add(path,tree,prefix+'DetailSelected',prefix+'Selected',0,2,248,76,selected,-1)
 font(o[prefix+'Label'],18);rect(o[prefix+'Icon'],20,16,44,44)
 if key=='Equipment':image(o[prefix+'Icon'],ivory,helmet)
 if key=='Stats':image(o[prefix+'Icon'],ivory,stats)
rect(o['ProfileJobEmblemSwitcher'],38,38,94,94)
rect(o['Align_ProfileName'],176,38,520,54);font(o['ProfileName'],29)
rect(o['ProfileMetaRow'],178,99,560,34)
font(o['ProfileJob'],16);font(o['ProfileLevel'],16)
row=o['ProfileMetaRow'];divider=unreal.find_object(tree,'ProfileMetaDivider')
if not divider:
 result=unreal.WidgetService.add_component(path,'Image','ProfileMetaDivider','ProfileMetaRow',False);assert result.success,result.export_text();divider=unreal.find_object(tree,'ProfileMetaDivider')
# Insert between existing live texts without replacing their identities.
box=unreal.find_object(tree,'ProfileMetaDividerBox')
if not box:
 result=unreal.WidgetService.add_component(path,'SizeBox','ProfileMetaDividerBox','ProfileMetaRow',False);assert result.success,result.export_text();box=unreal.find_object(tree,'ProfileMetaDividerBox')
if divider.get_parent()!=box:divider.remove_from_parent();box.add_child(divider)
box.set_width_override(1);box.set_height_override(16);box.slot.set_padding(unreal.Margin(16,8,16,8));box.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
level=o['ProfileLevel'];padding=level.slot.get_editor_property('padding');row.remove_child(level);row.add_child_to_horizontal_box(level);level.slot.set_padding(padding)
divider.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
br=unreal.SlateBrush();br.draw_as=unreal.SlateBrushDrawType.IMAGE
br.tint_color=unreal.SlateColor(specified_color=alpha(gold,.5));divider.set_brush(br)
for n in ['HeaderRuleStart','HeaderRuleEnd']:hide(o[n])
o['HeaderRule'].slot.set_anchors(unreal.Anchors(unreal.Vector2D(),unreal.Vector2D(1,0)));o['HeaderRule'].slot.set_offsets(unreal.Margin(670,78,98,1));image(o['HeaderRule'],alpha(gold,.48))
# Actual existing close button owns input; its decorative diamond remains fixed.
for n in ['CloseInner0','CloseInner1','CloseInner2','CloseInner3']:hide(o[n])
o['CloseCanvas'].set_render_transform(unreal.WidgetTransform())
for n,w in o.items():
 if n.startswith('CloseOuter') or n=='CloseFace':hide(w)
for n,angle in [('CloseCrossA',45),('CloseCrossB',-45)]:
 rect(o[n],13,21,18,1.5);o[n].set_render_transform_angle(angle);image(o[n],cream)
style=o['CloseButton'].get_editor_property('widget_style').copy()
for state,mult in [('normal',1),('hovered',1.16),('pressed',.84),('disabled',.6)]:
 br=style.get_editor_property(state).copy();br.resource_object=close;br.draw_as=unreal.SlateBrushDrawType.IMAGE;br.tint_color=unreal.SlateColor(specified_color=unreal.LinearColor(mult,mult,mult,1));style.set_editor_property(state,br)
style.normal_padding=unreal.Margin();style.pressed_padding=unreal.Margin();o['CloseButton'].set_style(style)
result=unreal.BlueprintService.compile_blueprint(path);assert result.success,result.export_text();assert unreal.EditorAssetLibrary.save_asset(path,False)

path,bp,tree,o=skin('InfoPage')
for n,w in o.items():
 if n.startswith(('InfoTableFrame','InfoTableTL','InfoTableTR','InfoTableBL','InfoTableBR','InfoCorner','InfoRibbonEdge','PolishShadow_PolishShadow')):hide(w)
rect(o['InfoContent'],628,12,552,504)
rect(o['InfoTableSurface'],0,0,552,504);image(o['InfoTableSurface'],unreal.LinearColor(1,1,1,1),card)
add(path,tree,'InfoBodyFace','InfoContent',12,77,528,414,body,0)
hide(o['InfoPaper']);o['InfoWatermarkCompass'].set_render_opacity(.018)
rect(o['PolishShadow_InfoTableSurface'],2,4,552,504);image(o['PolishShadow_InfoTableSurface'],alpha(navy,.045),card);o['PolishShadow_InfoTableSurface'].set_render_opacity(1)
rect(o['InfoRibbonFill'],7,7,232,58);image(o['InfoRibbonFill'],unreal.LinearColor(1,1,1,1),tab)
rect(o['InfoTitleStar'],25,24,22,22)
rect(o['Align_InfoRibbonText'],58,7,170,58);font(o['InfoRibbonText'],18)
rows=[('NameLabel','NameText',92),('JobLabel','JobText',139),('LevelLabel','LevelText',178),('ExperienceLabel','ExperienceText',232),('LocationLabel','LocationText',305),('IdentifierLabel','CharacterIdText',365),('CurrencyLabel','GoldText',425)]
for label,value,y in rows:
 rect(o['Align_'+label],78,y,145,32);rect(o['Align_'+value],224,y,294,32)
 font(o[label],14);font(o[value],15 if value not in ['JobText','LevelText'] else 13)
 o[value].set_editor_property('text_overflow_policy',unreal.TextOverflowPolicy.ELLIPSIS)
 if value in ['JobText','LevelText']:
  c=ns['MUTED'];o[value].set_color_and_opacity(unreal.SlateColor(specified_color=c));o[label].set_color_and_opacity(unreal.SlateColor(specified_color=c))
for n,y in [('InfoNameIcon',96),('InfoJobIcon',143),('InfoLevelIcon',182),('InfoExperienceIcon',236)]:rect(o[n],32,y,26,26)
# Keep existing location/document/coin glyphs intact as groups by additive translation.
for n,w in o.items():
 delta=33 if n.startswith('InfoLocation') and n not in ['InfoLocationTextRule'] else 43 if n.startswith('InfoId') else 53 if n.startswith('InfoGold') else 0
 if delta:w.set_render_translation(unreal.Vector2D(0,delta))
for n,y in [('InfoNameTextRule',132),('InfoJobTextRule',172),('InfoLevelTextRule',215),('InfoExperienceTextRule',294),('InfoLocationTextRule',352),('InfoCharacterIdTextRule',412)]:
 rect(o[n],30,y,488,.7);image(o[n],alpha(gold,.23))
rect(o['Align_ExperienceText'],228,223,290,26);font(o['ExperienceText'],13)
rect(o['ProgressTrackOutline'],227,257,292,13);image(o['ProgressTrackOutline'],unreal.LinearColor(1,1,1,1),exp)
rect(o['ExperienceBar'],229,259,288,9)
st=o['ExperienceBar'].get_editor_property('widget_style').copy();br=st.background_image.copy();br.draw_as=unreal.SlateBrushDrawType.NO_DRAW_TYPE;st.background_image=br;o['ExperienceBar'].set_editor_property('widget_style',st)
# Independent exhibit artwork stays compatible with the existing motion component.
o['CharacterIllustration'].set_render_opacity(.23)
image(o['DepthRearOrbit'],unreal.LinearColor(1,1,1,1),orbit)
rect(o['DepthRearOrbit'],120,16,480,480)
image(o['MirrorFloorSurface'],unreal.LinearColor(1,1,1,1),floor);rect(o['MirrorFloorSurface'],80,392,560,148);o['MirrorFloorSurface'].set_render_opacity(1)
o['CharacterReflection'].set_render_opacity(.20)
o['DepthFrontLine'].set_render_opacity(.4)
result=unreal.BlueprintService.compile_blueprint(path);assert result.success,result.export_text();assert unreal.EditorAssetLibrary.save_asset(path,False)
print('REFERENCE_DETAIL_SAVED: shared shell + character information; original input/bindings preserved')
