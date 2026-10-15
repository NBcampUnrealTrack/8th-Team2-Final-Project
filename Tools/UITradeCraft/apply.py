"""Settings-family styling for trade and crafting; preserve transaction wiring."""
import unreal,importlib.util,json,shutil
from pathlib import Path
from datetime import datetime
ROOT=Path(unreal.Paths.project_dir()).resolve()
def module(name,path):
 s=importlib.util.spec_from_file_location(name,ROOT/path);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
k=module('trade_craft_key','Tools/UISettingsReference/apply_key_polish.py');p=k.p
h=module('trade_craft_helpers','Tools/UIProfilePreview/apply.py')
BASE='/Game/Developers/dbals/UI/'
NAMES=['WBP_CraftCategory','WBP_CraftEntry','WBP_CraftMaterial','WBP_CraftDetail','WBP_CraftWindow','WBP_TradeWindow']
FRAME='/Game/Developers/MinkiCho/Art/UITheme/PlayerPanels/M_PlayerPanelFrame'
CLOSE='/Game/Developers/MinkiCho/Art/UITheme/HubReference2/M_SettingsCenteredCloseIcon'
def white():return unreal.LinearColor(1,1,1,1)
def panel(o):o.set_brush(h.brush(unreal.load_asset(FRAME)));o.set_brush_color(white())
def fill(o,role,alpha=1):
 b=unreal.SlateBrush();b.draw_as=unreal.SlateBrushDrawType.BOX;b.tint_color=unreal.SlateColor(specified_color=white());o.set_brush(b);o.set_brush_color(p.col(role,alpha))
def pad(o,l=0,t=0,r=0,b=0):o.slot.set_padding(unreal.Margin(l,t,r,b))
def font(o,size,role):h.text(o,size,role)
def common(sk,name,primary=False,width=150):
 o=k.replace(sk,name,'WBP_PopupPalette'+('Primary' if primary else 'Secondary'))
 o.set_editor_property('label_color',p.col('ink' if primary else 'text'));o.set_editor_property('label_font_size',18);o.set_editor_property('override_button_size',True);o.set_editor_property('button_size',unreal.Vector2D(width,44));return o
def close(sk,box):
 sk.get(box).set_width_override(40);sk.get(box).set_height_override(40);sk.get(box).slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
 o=k.replace(sk,'CloseButton','Button');h.button_style(o);st=o.get_editor_property('widget_style').copy();st.normal_padding=unreal.Margin();st.pressed_padding=unreal.Margin();o.set_style(st)
 icon=h.new(sk.tree,unreal.Image,'ThemeCloseIcon');o.add_child(icon);icon.set_brush(h.brush(unreal.load_asset(CLOSE)));icon.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE);pad(icon,8,8,8,8);icon.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL);icon.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
def apply():
 assert not p.W.is_pie_running()
 out=ROOT/'Saved/UITradeCraft'/datetime.now().strftime('%Y%m%d_%H%M%S');out.mkdir(parents=True)
 bindings={};report={}
 for n in NAMES:
  path=BASE+n;shutil.copy2(ROOT/'Content'/(path[6:]+'.uasset'),out/(n+'.uasset'));bindings[path]=unreal.ObjectReflectionService.get_property(path,'Bindings')
 for n in NAMES:
  sk=k.s.Skin(n);sk.bp.modify() if hasattr(sk,'bp') else unreal.load_asset(sk.path).modify()
  for row in p.snapshot(sk.path):
   o=sk.get(row['name']);o.modify();name=o.get_name()
   if isinstance(o,unreal.TextBlock):
    role='gold' if any(v in name for v in ['Title','Gold','Cost','Expanded','Collapsed','Plus']) else 'muted' if name in ['MaterialText','PadHintText','MaxCountText','QuantityLabel','OwnedLabel','CostLabel'] else 'text'
    # Red warnings carry gameplay meaning; retain their authored color.
    if name in ['ReasonText','LockText']:continue
    size=18 if role!='muted' else 16
    if name=='TitleText':size=28 if n.endswith('Window') else 18
    if name=='ResultNameText':size=22;role='gold'
    font(o,size,role)
   elif isinstance(o,unreal.Border):
    if name in ['WindowBG','DetailBG']:panel(o)
    elif name in ['RowBG','CategoryBG','ResultBG','IconBG','MyOfferFrame','PartnerOfferFrame']:fill(o,'surface')
    elif name=='SelectedBox':fill(o,'gold',.16)
    elif name=='OffersDividerFill':fill(o,'gold',.35)
    elif 'LockShackle' in name or name.endswith('LockBody'):fill(o,'gold')
    elif name.endswith('LockHole'):fill(o,'ink')
  if n=='WBP_TradeWindow':
   sk.get('WindowBG').slot.set_size(unreal.Vector2D(780,560));sk.get('WindowBG').set_padding(unreal.Margin(60,48,60,48))
   pad(sk.get('HeaderRow'),0,0,0,24);font(sk.get('PartnerNameText'),18,'muted');sk.get('PartnerNameText').slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER);sk.get('TitleText').slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
   close(sk,'SizeBox_0')
   for prefix in ['My','Partner']:
    pad(sk.get(prefix+'OfferTitle'),0,0,0,12)
    for suffix in ['OfferFrame','LockCover']:
     slot=sk.get(prefix+suffix).slot;slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL);slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
    sk.get(prefix+'OfferFrame').set_padding(unreal.Margin(12,12,12,12));sk.get(prefix+'OfferBox').slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER);sk.get(prefix+'OfferBox').slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
   pad(sk.get('OffersDivider'),16,0,16,0);sk.get('OffersDivider').set_width_override(1)
   pad(sk.get('FooterRow'),0,24,0,0);pad(common(sk,'LockButton'),0,0,16,0);common(sk,'ConfirmButton',True)
  elif n=='WBP_CraftWindow':
   sk.get('FrameBox').set_height_override(700);sk.get('FrameBox').slot.set_alignment(unreal.Vector2D(.5,.5));sk.get('FrameBox').slot.set_position(unreal.Vector2D())
   sk.get('MainBox').set_width_override(560);sk.get('WindowBG').set_padding(unreal.Margin(46,48,46,48))
   pad(sk.get('HeaderRow'),0,0,0,18);sk.get('TitleText').slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER);close(sk,'CloseButtonBox')
   pad(sk.get('GoldText'),0,0,0,22);pad(sk.get('DetailPanel'),16,0,0,0);sk.get('ListBox').set_scrollbar_padding(unreal.Margin(10,0,0,0))
  elif n=='WBP_CraftCategory':
   sk.get('CategorySizeBox').set_height_override(44);sk.get('CategoryBG').set_padding(unreal.Margin(12,8,12,8))
   for name in ['TitleText','CountText','ExpandedBox','CollapsedBox']:sk.get(name).slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
  elif n=='WBP_CraftEntry':
   sk.get('RowBG').set_padding(unreal.Margin(12,10,12,10));pad(sk.get('RowBG'),0,0,0,6);pad(sk.get('IconBox'));sk.get('IconBox').set_width_override(44);sk.get('IconBox').set_height_override(44);font(sk.get('NameText'),18,'text');font(sk.get('MaterialText'),14,'muted');font(sk.get('GoldText'),16,'gold')
   fill(sk.get('LockedBox'),'navy',.94)
  elif n=='WBP_CraftDetail':
   sk.get('DetailSizeBox').set_width_override(360);sk.get('DetailBG').set_padding(unreal.Margin(30,48,30,48))
   sk.get('ResultIconBox').set_width_override(88);sk.get('ResultIconBox').set_height_override(88);pad(sk.get('ResultIconBox'),0,0,0,16);pad(sk.get('ResultNameText'),0,0,0,18)
   sk.get('QuantityBox').set_width_override(92);sk.get('QuantityBox').set_height_override(40)
   for name in ['QuantityLabel','MaxCountText']:sk.get(name).slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER);font(sk.get(name),16,'muted')
   spin=sk.get('QuantitySpin');spin.set_editor_property('foreground_color',p.slate('text'));st=spin.get_editor_property('widget_style').copy()
   for field in ['background_brush','hovered_background_brush','active_fill_brush','inactive_fill_brush']:
    b=getattr(st,field).copy();b.tint_color=p.slate('surface');setattr(st,field,b)
   spin.set_editor_property('widget_style',st)
   sk.get('CraftButtonBox').set_height_override(44);pad(sk.get('CraftButtonBox'),0,20,0,0);common(sk,'CraftButton',True,300)
  elif n=='WBP_CraftMaterial':
   font(sk.get('LabelText'),13,'text');font(sk.get('CountText'),14,'text');font(sk.get('PlusBox'),18,'gold')
  c=p.B.compile_blueprint(sk.path);assert c.success and not c.num_errors,c.export_text();assert p.E.save_asset(sk.path,False)
  assert unreal.ObjectReflectionService.get_property(sk.path,'Bindings')==bindings[sk.path]
  report[sk.path]={'errors':c.num_errors,'warnings':c.num_warnings,'bindings_preserved':True}
 (out/'verified.json').write_text(json.dumps(report,indent=2));print(json.dumps(report));print(out)
if __name__=='__main__':apply()
