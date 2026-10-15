"""Move panel watermark, align menu groups, add editable local profile motto."""
import unreal,json,shutil
from pathlib import Path
from datetime import datetime
root=Path(unreal.Paths.project_dir()).resolve();out=root/'Saved/UIPanelConsistency'
W=unreal.WidgetService;M=unreal.MaterialEditingLibrary;E=unreal.EditorAssetLibrary;B=unreal.BlueprintService
assert not W.is_pie_running()
p='/Game/Developers/dbals/UI/WBP_GameMenu';art='/Game/Developers/MinkiCho/Art/UITheme/SettingsNavy'
backup=out/('personalize_before_'+datetime.now().strftime('%Y%m%d_%H%M%S'));backup.mkdir()
shutil.copy2(root/'Content/Developers/dbals/UI/WBP_GameMenu.uasset',backup/'WBP_GameMenu.uasset')
name='T_MenuHub_RightCompass'
if not E.does_asset_exist(art+'/'+name):
 a,error=unreal.AssetDiscoveryService.import_asset(str(root/'ArtSource/UI/PanelConsistency'/(name+'.png')),art,name);assert a,error
tex=unreal.load_asset(art+'/'+name)
tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON)
tex.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI)
tex.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
tex.set_editor_property('srgb',True);tex.set_editor_property('compression_no_alpha',False);tex.set_editor_property('never_stream',True)
assert E.save_loaded_asset(tex)
mp=art+'/M_Hub_RightCompass'
mat=unreal.load_asset(mp) if E.does_asset_exist(mp) else E.duplicate_asset(art+'/M_Hub_OriginalRight',mp)
for n in M.get_material_expressions(mat):
 if isinstance(n,unreal.MaterialExpressionTextureSample) and n.get_editor_property('texture').get_name() in ['T_GameMenuHub_GoldPattern',name]:n.set_editor_property('texture',tex)
assert not M.recompile_material(mat)
d=unreal.MaterialNodeService.get_material_diagnostics(mp);assert d.success and d.is_compiled_ok
assert E.save_loaded_asset(mat)
bp=unreal.load_asset(p);bp.modify();tree=unreal.find_object(bp,'WidgetTree');tree.modify()
def obj(n):return unreal.find_object(tree,n)
def rect(n,x,y,w,h):
 s=obj(n).slot;s.modify();s.set_position(unreal.Vector2D(x,y));s.set_size(unreal.Vector2D(w,h))
obj('HubBackplate').set_brush_from_material(mat)
obj('MainMenuTileView').set_editor_property('tile_alignment',unreal.ListItemAlignment.LEFT_ALIGNED)
assert obj('MainMenuTileView').get_editor_property('entry_width')==208
assert obj('MainMenuTileView').slot.get_size().x==1040
for n,x in [('OptionsButton',1240),('CharacterSelectButton',1350),('QuitGameButton',1460)]:rect(n,x,714,72,56)
for n,x in [('HubFooterSeparator0',1330.5),('HubFooterSeparator1',1440.5)]:rect(n,x,729,1,26)
if not obj('HubMottoInput'):
 r=W.add_component(p,'EditableTextBox','HubMottoInput','HubCanvas',True);assert r.success
inp=obj('HubMottoInput');inp.modify();rect('HubMottoInput',98,756,267,38);inp.slot.set_z_order(7)
inp.set_editor_property('revert_text_on_escape',True)
inp.set_editor_property('clear_keyboard_focus_on_commit',True)
inp.set_editor_property('select_all_text_when_focused',True)
inp.set_editor_property('overflow_policy',unreal.TextOverflowPolicy.ELLIPSIS)
inp.set_tool_tip_text('클릭하여 인사말 수정 (32자 이내)\nEnter 또는 다른 곳 클릭: 저장 · Esc: 취소')
inp.set_text('안녕하세요')
style=inp.get_editor_property('widget_style')
style.padding=unreal.Margin(4,4,4,4)
ts=style.text_style;ts.font=obj('HubMotto').get_editor_property('font');style.text_style=ts
for n in ['foreground_color','focused_foreground_color','read_only_foreground_color']:setattr(style,n,unreal.SlateColor(specified_color=unreal.LinearColor(.84,.88,.93,1)))
for n,a in [('background_image_normal',.10),('background_image_hovered',.28),('background_image_focused',.45),('background_image_read_only',0)]:
 brush=getattr(style,n);brush.draw_as=unreal.SlateBrushDrawType.ROUNDED_BOX;brush.set_editor_property('resource_object',None)
 brush.tint_color=unreal.SlateColor(specified_color=unreal.LinearColor(.16,.24,.34,a));setattr(style,n,brush)
inp.set_editor_property('widget_style',style)
obj('HubMotto').set_visibility(unreal.SlateVisibility.COLLAPSED)
if not obj('HubMottoHint'):
 r=W.add_component(p,'TextBlock','HubMottoHint','HubCanvas',False);assert r.success
hint=obj('HubMottoHint');hint.set_text('클릭하여 수정 · Enter 저장');font=obj('HubMotto').get_editor_property('font');font.size=11;hint.set_font(font)
hint.set_color_and_opacity(unreal.SlateColor(specified_color=unreal.LinearColor(.56,.64,.70,.8)))
rect('HubMottoHint',102,795,260,18);hint.slot.set_z_order(7);hint.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
r=B.compile_blueprint(p);assert r.success and r.num_errors==0
assert E.save_asset(p,False)
(out/'hub_personalize.json').write_text(json.dumps({'errors':r.num_errors,'warnings':r.num_warnings,'columns':5,'alignment':'left','footer':'right','motto_input':True},indent=2))
