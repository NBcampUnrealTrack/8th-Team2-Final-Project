"""Profile/context design pass; private materials, original bound widgets retained."""
import unreal,importlib.util,shutil,json
from pathlib import Path
from datetime import datetime
ROOT=Path(unreal.Paths.project_dir()).resolve()
s=importlib.util.spec_from_file_location('palette',ROOT/'Tools/UISettingsReference/apply_palette_pilot.py');p=importlib.util.module_from_spec(s);s.loader.exec_module(p)
E=p.E;M=p.M
PRIVATE='/Game/Developers/MinkiCho/Art/UITheme/PlayerPanels'
PROFILE='/Game/Developers/dbals/UI/WBP_CharacterProfile'
CONTEXT='/Game/Developers/dongh/UI/WBP_PlayerContextMenu'
def brush(resource):
 b=unreal.SlateBrush();b.resource_object=resource;b.draw_as=unreal.SlateBrushDrawType.IMAGE;b.tint_color=p.slate('text');b.tint_color=unreal.SlateColor(specified_color=unreal.LinearColor(1,1,1,1));return b
def frame():
 path=PRIVATE+'/M_PlayerPanelFrame'
 a=unreal.load_asset(path) or unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_PlayerPanelFrame',PRIVATE,unreal.Material,unreal.MaterialFactoryNew())
 a.modify();M.delete_all_material_expressions(a);a.set_editor_property('material_domain',unreal.MaterialDomain.MD_UI);a.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
 uv=M.create_material_expression(a,unreal.MaterialExpressionTextureCoordinate,-400,0)
 n=M.create_material_expression(a,unreal.MaterialExpressionCustom,0,0);n.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT4)
 ci=unreal.CustomInput();ci.set_editor_property('input_name','UV');n.set_editor_property('inputs',[ci])
 navy=','.join(map(str,p.rgb('navy')));gold=','.join(map(str,p.rgb('gold')))
 n.set_editor_property('code',r'''
float2 size=clamp(1/max(fwidth(UV),.0001),64,4096);
float2 v=(UV-.5)*size; float2 a=abs(v); float2 h=size*.5-8;
float r=17; float2 q=a-h+r;
float d=length(max(q,0))+min(max(q.x,q.y),0)-r;
float aa=max(fwidth(d),.65);float body=1-smoothstep(-aa,aa,d);
float H=1-smoothstep(h.x-24,h.x-22,a.x),V=1-smoothstep(h.y-24,h.y-22,a.y);
float rods=max((1-smoothstep(.65,1.5,abs(a.y-h.y+1)))*H,(1-smoothstep(.65,1.5,abs(a.x-h.x+1)))*V);
float arc=(1-smoothstep(.35,1.1,abs(d+1)))*(1-H)*(1-V);
float inner=max((1-smoothstep(.25,1.05,abs(a.y-h.y+5)))*H,(1-smoothstep(.25,1.05,abs(a.x-h.x+5)))*V)*.26;
float2 sh=abs(a-float2(h.x-23,h.y-1))/float2(6,4);
float2 sv=abs(a-float2(h.x-1,h.y-23))/float2(4,6);
float sd=min(sqrt(sh.x)+sqrt(sh.y),sqrt(sv.x)+sqrt(sv.y))-1;
float star=1-smoothstep(-.06,.12,sd);
float3 navy=float3(NAVY),gold=float3(GOLD);
float3 fill=navy*(.96+.09*(1-UV.y));
float rim=saturate(max(rods,arc*.78)+inner);
float3 rgb=lerp(fill,gold,rim);rgb=lerp(rgb,gold*1.06,star);
return float4(rgb,max(body,star));
'''.replace('NAVY',navy).replace('GOLD',gold))
 M.connect_material_expressions(uv,'',n,'UV')
 for alpha,prop in [(False,unreal.MaterialProperty.MP_EMISSIVE_COLOR),(True,unreal.MaterialProperty.MP_OPACITY)]:
  mask=M.create_material_expression(a,unreal.MaterialExpressionComponentMask,300,100 if alpha else 0)
  for c in ['r','g','b']:mask.set_editor_property(c,not alpha)
  mask.set_editor_property('a',alpha);M.connect_material_expressions(n,'',mask,'');M.connect_material_property(mask,'',prop)
 assert not M.recompile_material(a);assert E.save_loaded_asset(a);return a
def text(o,size,role='text'):
 f=o.get_editor_property('font').copy();f.size=size;o.set_font(f);o.set_color_and_opacity(p.slate(role));o.set_render_translation(unreal.Vector2D())
def pad(o,l=0,t=0,r=0,b=0):o.slot.set_padding(unreal.Margin(l,t,r,b))
def new(tree,cls,name):return unreal.find_object(tree,name) or unreal.new_object(cls,outer=tree,name=name)
def button_style(o):
 st=o.get_editor_property('widget_style').copy()
 for state in ['normal','hovered','pressed','disabled']:
  mat=unreal.load_asset(p.PRIVATE+'/MI_OptionsSecondary_'+('normal' if state=='disabled' else state));setattr(st,state,brush(mat))
 st.normal_padding=unreal.Margin(16,10,16,10);st.pressed_padding=unreal.Margin(16,10,16,10);o.set_style(st)
 o.set_render_transform_pivot(unreal.Vector2D(.5,.5))
def rule(tree,parent,name):
 o=new(tree,unreal.Border,name);parent.add_child(o);o.set_brush_color(p.col('gold',.45));o.set_padding(unreal.Margin());o.set_desired_size_scale(unreal.Vector2D(1,1))
 sb=new(tree,unreal.SizeBox,name+'Height');o.remove_from_parent();parent.add_child(sb);sb.add_child(o);sb.set_height_override(1);pad(sb,0,10,0,14);o.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE);return sb
def apply():
 assert not unreal.WidgetService.is_pie_running()
 out=ROOT/'Saved/UIProfilePreview'/datetime.now().strftime('%Y%m%d_%H%M%S');out.mkdir(parents=True)
 for path in [PROFILE,CONTEXT]:shutil.copy2(ROOT/'Content'/(path[6:]+'.uasset'),out/(path.split('/')[-1]+'.uasset'))
 mat=frame()
 bp=unreal.load_asset(PROFILE);bp.modify();tree=unreal.find_object(bp,'WidgetTree');get=lambda n:unreal.find_object(tree,n)
 bg=get('WindowBG');bg.set_brush(brush(mat));bg.set_padding(unreal.Margin(30,28,30,28));bg.set_brush_color(unreal.LinearColor(1,1,1,1))
 content=get('ContentBox');content.remove_from_parent();width=new(tree,unreal.SizeBox,'ProfileContentWidth');bg.add_child(width);width.set_width_override(300);width.add_child(content);pad(content)
 order=[get(n) for n in ['HeaderRow','NameText','LevelText','JobText','EquipListBox','EquipToggleButton']]
 content.clear_children();content.add_child(order[0]);pad(order[0],0,0,0,2)
 text(get('TitleText'),22,'gold');get('TitleText').slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
 get('SizeBox_0').set_width_override(32);get('SizeBox_0').set_height_override(32);get('SizeBox_0').slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
 rule(tree,content,'ProfileHeaderRule')
 for o in order[1:]:content.add_child(o)
 text(get('NameText'),24);text(get('LevelText'),17,'gold');text(get('JobText'),17,'muted')
 pad(get('NameText'),0,0,0,10);pad(get('LevelText'),0,0,0,4);pad(get('JobText'),0,0,0,18)
 pad(get('EquipListBox'),0,0,0,18);get('EquipListBox').slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
 unreal.get_default_object(bp.generated_class()).set_editor_property('slot_cell_size',72)
 spec=importlib.util.spec_from_file_location('profile_key',ROOT/'Tools/UISettingsReference/apply_key_polish.py');k=importlib.util.module_from_spec(spec);spec.loader.exec_module(k)
 toggle=k.replace(k.s.Skin('WBP_CharacterProfile'),'EquipToggleButton','WBP_PopupPaletteSecondary');toggle.set_editor_property('label_font_size',18);toggle.set_editor_property('label_color',p.col('text'));toggle.set_editor_property('override_button_size',True);toggle.set_editor_property('button_size',unreal.Vector2D(300,44))
 pad(toggle,0,6,0,0)
 close=k.replace(k.s.Skin('WBP_CharacterProfile'),'CloseButton','Button');button_style(close)
 st=close.get_editor_property('widget_style').copy();st.normal_padding=unreal.Margin();st.pressed_padding=unreal.Margin();close.set_style(st)
 icon=new(tree,unreal.Image,'ProfileCloseIcon');close.add_child(icon);icon.set_brush(brush(unreal.load_asset('/Game/Developers/MinkiCho/Art/UITheme/HubReference2/M_SettingsCenteredCloseIcon')));icon.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE);pad(icon,6,6,6,6)
 icon.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL);icon.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
 bp=unreal.load_asset(CONTEXT);bp.modify();tree=unreal.find_object(bp,'WidgetTree');get=lambda n:unreal.find_object(tree,n)
 get('RootSize').set_width_override(320);get('RootSize').clear_height_override()
 get('Background').set_brush(brush(mat));get('Background').set_brush_color(unreal.LinearColor(1,1,1,1));get('Background').set_padding(unreal.Margin(28,26,28,26))
 rows=get('MenuList');children=list(rows.get_all_children())
 get('NameRow').set_brush_color(unreal.LinearColor(0,0,0,0));get('NameRow').set_padding(unreal.Margin(4,2,4,2));text(get('Text_PlayerName'),20,'gold')
 pad(get('NameRow'),0,0,0,18)
 for o in children:
  if o.get_name() in ['NameRow','ContextHeaderRuleHeight']:continue
  if isinstance(o,unreal.Button):
   button_style(o);pad(o,0,0,0,6)
   for child in o.get_all_children():
    if isinstance(child,unreal.TextBlock):text(child,18);child.set_editor_property('justification',unreal.TextJustify.CENTER);pad(child);child.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER);child.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
 for path in [PROFILE,CONTEXT]:
  result=p.B.compile_blueprint(path);print(path,result);assert result.success and not result.num_errors;assert E.save_asset(path,False)
 print('BACKUP',out)
if __name__=='__main__':apply()
