"""Key-settings continuation of the options palette and detail polish."""
import unreal,importlib.util,json,shutil
from pathlib import Path
from datetime import datetime
ROOT=Path(unreal.Paths.project_dir()).resolve()
def mod(name,file):
 s=importlib.util.spec_from_file_location(name,ROOT/file);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
q=mod('detail','Tools/UISettingsReference/apply_detail_polish.py');p=q.p
s=mod('skin','Tools/UISettingsReference/apply.py')
W=p.W;E=p.E;B=p.B
BASE='/Game/Developers/dbals/UI/'
OUT=ROOT/'Saved/UIKeySettingsPolish';OUT.mkdir(exist_ok=True)
WIDTH=986.0

def style(kind):
 st=unreal.ButtonStyle()
 for state in ['normal','hovered','pressed']:
  setattr(st,state,q.brush(unreal.load_asset(p.PRIVATE+'/MI_Options'+kind+'_'+state)))
 st.disabled=st.normal;st.normal_padding=unreal.Margin(12,0,12,0);st.pressed_padding=unreal.Margin(12,1,12,-1)
 return st

def common(kind):
 name='WBP_KeySettings'+kind;path=p.PRIVATE+'/'+name
 if not E.does_asset_exist(path):
  f=unreal.WidgetBlueprintFactory();f.set_editor_property('parent_class',unreal.NU_CommonButtonWidget)
  assert unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,p.PRIVATE,unreal.WidgetBlueprint,f)
  assert W.add_component(path,'Button','Btn_Common','',True).success
  assert W.add_component(path,'TextBlock','Txt_Label','Btn_Common',True).success
 bp=unreal.load_asset(path);bp.modify();t=unreal.find_object(bp,'WidgetTree');button=p.obj(t,'Btn_Common');button.set_style(style(kind))
 label=p.obj(t,'Txt_Label');label.set_editor_property('justification',unreal.TextJustify.CENTER);label.slot.set_padding(unreal.Margin());label.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER);label.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER);label.set_render_translation(unreal.Vector2D(0,-1))
 c=B.compile_blueprint(path);assert c.success and c.num_errors==0,c.export_text();assert E.save_asset(path,False)
 return name

def replace(sk,n,cls):
 old=sk.get(n);parent=old.get_parent().get_name();slot=old.slot
 props={}
 for k in ['padding','horizontal_alignment','vertical_alignment','anchors','alignment','offsets','auto_size','z_order','size']:
  try:props[k]=slot.get_editor_property(k)
  except:pass
 values={}
 for k in ['label_text','label_font_size','label_color','tool_tip_text']:
  try:values[k]=old.get_editor_property(k)
  except:pass
 assert W.remove_component(sk.path,n)
 assert W.add_component(sk.path,cls,n,parent,True).success
 new=sk.get(n)
 for k,v in props.items():new.slot.set_editor_property(k,v)
 for k,v in values.items():
  try:new.set_editor_property(k,v)
  except:pass
 return new

def text(sk,n,key='text',size=None):
 o=sk.get(n);o.set_color_and_opacity(p.slate(key))
 if size:
  font=o.get_editor_property('font');font.size=size;o.set_font(font)
 return o

def apply():
 assert not W.is_pie_running()
 backup=OUT/datetime.now().strftime('%Y%m%d_%H%M%S');backup.mkdir()
 paths=[p.PATH]+[BASE+n for n in ['WBP_KeyBindingsWindow','WBP_KeyBindingRow','WBP_KeyBindingGroup']]
 for path in paths:
  src=ROOT/'Content'/(path[6:]+'.uasset');shutil.copy2(src,backup/src.name)
 secondary=common('Secondary');primary=common('Primary')
 # 756-ish wide launcher becomes 190 x 48, approximately quarter width and 1.5x text-driven height.
 sk=s.Skin('WBP_OptionsWindow');button=sk.get('KeyBindingsButton')
 box=sk.add('SizeBox','KeySettingsLauncherSize','KeyBindingsRow');box.set_width_override(190);box.set_height_override(48);box.add_child(sk.get('KeyBindingsButton'));button=sk.get('KeyBindingsButton')
 box.slot.set_size(unreal.SlateChildSize(1,unreal.SlateSizeRule.AUTOMATIC));box.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
 label=text(sk,'KeyBindingsButtonLabel','text',18);label.set_text('키 설정');button.set_tool_tip_text('키보드 · 마우스 · 게임패드 키 설정')
 label.slot.set_padding(unreal.Margin());label.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER);label.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
 text(sk,'SettingsControlsHelp','muted',16).set_text('키보드 · 마우스 · 게임패드의 이동, 전투, 상호작용 키를 변경합니다.\n\n마우스 감도 · 게임패드 입력 방식 설정은 준비 중입니다.')
 # Panel and footer.
 sk=s.Skin('WBP_KeyBindingsWindow');p.recolor(sk.path)
 sk.get('WindowBG').set_editor_property('background',q.brush(unreal.load_asset(p.PRIVATE+'/M_OptionsPolishedFrame')))
 for n in ['HeaderRow','FooterRow']:sk.get(n).set_visibility(unreal.SlateVisibility.COLLAPSED)
 text(sk,'TitleText','text',29);text(sk,'GuideText','muted',14)
 sk.rect('SettingsHeaderRule',152,111,WIDTH,1);sk.rect('SettingsFooterRule',152,662,WIDTH,1)
 # All headings use the exact same x/width fractions as the fixed-width rows.
 for n,left,right in [('Action',.012,.43),('Keyboard',.445,.695),('Gamepad',.735,.985)]:
  name='Settings'+n+'Heading';sk.rect(name,152+WIDTH*left,129,WIDTH*(right-left),30,3)
  o=text(sk,name,'muted',18);o.set_editor_property('justification',unreal.TextJustify.LEFT if n=='Action' else unreal.TextJustify.CENTER)
 sk.rect('RowScroll',152,169,1000,479,2)
 sk.get('RowBox').slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_LEFT)
 for n,cls,key in [('DefaultsButton',secondary,'text'),('ApplyButton',primary,'ink')]:
  o=replace(sk,n,cls);o.set_editor_property('bIconOnly',False);o.set_editor_property('bOverrideButtonSize',False);o.set_editor_property('label_color',p.col(key));o.set_editor_property('label_font_size',17)
  sk.get('Settings'+n+'BG').set_visibility(unreal.SlateVisibility.COLLAPSED)
 for n in ['DefaultsButtonBox','ApplyButtonBox']:sk.get(n).set_height_override(44);sz=sk.get(n).slot.get_size();sk.get(n).slot.set_size(unreal.Vector2D(sz.x,44))
 o=sk.get('FooterCloseButton');o.set_style(style('Secondary'));o.set_background_color(unreal.LinearColor(1,1,1,1));sk.rect('FooterCloseButton',866,711,120,44,3)
 label=text(sk,'FooterCloseLabel');label.slot.set_padding(unreal.Margin());label.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER);label.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER);label.set_render_translation(unreal.Vector2D(0,-1))
 close=replace(sk,'CloseButton','Button');st=unreal.ButtonStyle()
 for state in ['normal','hovered','pressed','disabled']:
  br=unreal.SlateBrush();br.draw_as=unreal.SlateBrushDrawType.NO_DRAW_TYPE;setattr(st,state,br)
 close.set_style(st);close.set_render_scale(unreal.Vector2D(1,1))
 glyph=sk.add('TextBlock','KeySettingsCloseGlyph','CloseButton');glyph.set_text('×');text(sk,'KeySettingsCloseGlyph','gold',28);glyph.slot.set_padding(unreal.Margin());glyph.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER);glyph.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
 # Dynamic row and group templates: same palette, same popup frame, native capture behavior preserved.
 sk=s.Skin('WBP_KeyBindingRow');p.recolor(sk.path);sk.get('RowSizeBox').set_width_override(WIDTH);sk.get('RowSizeBox').set_height_override(48)
 for n,left,right in [('ChangeButton',.445,.695),('PadChangeButton',.735,.985)]:
  o=replace(sk,n,secondary);o.set_editor_property('bIconOnly',False);o.set_editor_property('bOverrideButtonSize',False);o.set_editor_property('label_text','')
  sk.relative(n,left,right,3,3,1)
 for n in ['SettingsKeyCell','SettingsPadCell']:sk.get(n).set_visibility(unreal.SlateVisibility.COLLAPSED)
 text(sk,'NameText','text',18);text(sk,'KeyText','text',17);text(sk,'PadKeyText','text',17)
 for n in ['KeyboardKeycap','GamepadKeycap']:
  b=unreal.SlateBrush();b.draw_as=unreal.SlateBrushDrawType.NO_DRAW_TYPE;sk.get(n).set_editor_property('background',b);sk.get(n).set_padding(unreal.Margin(8,0,8,0))
 for n in ['KeyText','PadKeyText','WaitingText','PadWaitingText','EmptyText','PadEmptyText']:
  o=sk.get(n);o.set_editor_property('justification',unreal.TextJustify.CENTER)
  o.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER);o.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
 for n in ['WaitingText','PadWaitingText']:text(sk,n,'gold',14)
 for n in ['WaitingBox','PadWaitingBox']:
  sk.get(n).set_editor_property('background',q.brush(unreal.load_asset(p.PRIVATE+'/MI_OptionsSecondary_hovered')))
 for n in ['ChangedBox','PadChangedBox']:sk.get(n).set_brush_color(p.col('gold'))
 line=sk.get('SettingsRowRule');br=line.get_editor_property('background');br.tint_color=p.slate('muted',.16);line.set_editor_property('background',br)
 sk=s.Skin('WBP_KeyBindingGroup');p.recolor(sk.path);sk.get('GroupSizeBox').set_width_override(WIDTH);sk.get('GroupSizeBox').set_height_override(40)
 sk.get('GroupBG').set_editor_property('background',q.brush(unreal.load_asset(p.PRIVATE+'/MI_OptionsSecondary_normal')));sk.get('GroupBG').set_padding(unreal.Margin(14,0,12,0));sk.get('GroupText').slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER);text(sk,'GroupText','gold',18)
 result={}
 for path in [BASE+'WBP_KeyBindingRow',BASE+'WBP_KeyBindingGroup',BASE+'WBP_KeyBindingsWindow',p.PATH]:
  c=B.compile_blueprint(path);assert c.success and c.num_errors==0,c.export_text();assert E.save_asset(path,False);result[path]=c.export_text()
 (OUT/'applied.json').write_text(json.dumps({'compile':result,'row_width':WIDTH,'launcher':[190,48],'backup':str(backup)},indent=2));print(result)
if __name__=='__main__':apply()
