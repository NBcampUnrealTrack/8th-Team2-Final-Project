"""Reference-v2 hub: left navigation, upper-right profile, transparent game center."""
import unreal,json,shutil,re
from pathlib import Path
from datetime import datetime
ROOT=Path(unreal.Paths.project_dir()).resolve();OUT=ROOT/'Saved/UIHubSidePanels';OUT.mkdir(exist_ok=True)
ART='/Game/Developers/MinkiCho/Art/UITheme/HubSidePanels'
MAIN='/Game/Developers/dbals/UI/WBP_GameMenu'
TILE='/Game/Developers/dbals/UI/GameMenuHub/WBP_GameMenuTileEntry'
SYSTEM='/Game/Developers/dbals/UI/GameMenuHub/WBP_GameMenuSystem'
W=unreal.WidgetService;M=unreal.MaterialEditingLibrary;E=unreal.EditorAssetLibrary;B=unreal.BlueprintService
NAVY=(.012,.028,.045);GOLD=(.65,.43,.20);IVORY=(.9,.84,.69)
def color(rgb,a=1):return unreal.LinearColor(*rgb,a)
def slate(rgb,a=1):return unreal.SlateColor(specified_color=color(rgb,a))
def mat(name):
 p=ART+'/'+name
 a=unreal.load_asset(p) if E.does_asset_exist(p) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,ART,unreal.Material,unreal.MaterialFactoryNew())
 a.modify();M.delete_all_material_expressions(a)
 a.set_editor_property('material_domain',unreal.MaterialDomain.MD_UI);a.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
 return a
def custom(a,code,typ,inputs):
 n=M.create_material_expression(a,unreal.MaterialExpressionCustom,0,0);n.set_editor_property('code',code);n.set_editor_property('output_type',typ)
 items=[]
 for name,src,pin in inputs:
  i=unreal.CustomInput();i.set_editor_property('input_name',name);items.append(i)
 n.set_editor_property('inputs',items)
 for name,src,pin in inputs:assert M.connect_material_expressions(src,pin,n,name)
 return n
def finishmat(a):
 assert not M.recompile_material(a)
 d=unreal.MaterialNodeService.get_material_diagnostics(a.get_path_name());assert d.success and d.is_compiled_ok
 assert E.save_loaded_asset(a)
def rect(o,x,y,w,h):
 s=o.slot;s.modify();s.set_anchors(unreal.Anchors(unreal.Vector2D(0,0),unreal.Vector2D(0,0)));s.set_alignment(unreal.Vector2D(0,0));s.set_position(unreal.Vector2D(x,y));s.set_size(unreal.Vector2D(w,h))
def tree(p):
 bp=unreal.load_asset(p);bp.modify();t=unreal.find_object(bp,'WidgetTree');t.modify();return bp,t
def obj(t,n):
 o=unreal.find_object(t,n);assert o,n;o.modify();return o
def text(t,n,x,y,w,h,size,rgb,content=None):
 o=obj(t,n);rect(o,x,y,w,h);font=o.get_editor_property('font');font.size=size;o.set_font(font);o.set_color_and_opacity(slate(rgb))
 if content is not None:o.set_text(content)
 return o
def brush(resource):
 b=unreal.SlateBrush();b.draw_as=unreal.SlateBrushDrawType.IMAGE;b.set_editor_property('resource_object',resource);b.tint_color=slate((1,1,1));return b
def apply():
 assert not W.is_pie_running()
 backup=OUT/datetime.now().strftime('%Y%m%d_%H%M%S');backup.mkdir()
 graphs={};bindings={}
 for p in [MAIN,TILE,SYSTEM]:
  src=ROOT/'Content'/(p[6:]+'.uasset');shutil.copy2(src,backup/src.name)
  graphs[p]=B.describe_graph(p,'EventGraph')[0].export_text()
  bindings[p]=unreal.ObjectReflectionService.get_property(p,'Bindings')
 name='T_Hub_SidePanels'
 if not E.does_asset_exist(ART+'/'+name):
  imported,error=unreal.AssetDiscoveryService.import_asset(str(ROOT/'ArtSource/UI/HubSidePanels'/(name+'.png')),ART,name);assert imported,error
 tex=unreal.load_asset(ART+'/'+name)
 for k,v in [('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON),('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI),('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS),('srgb',True),('compression_no_alpha',False),('never_stream',True)]:tex.set_editor_property(k,v)
 assert E.save_loaded_asset(tex)
 a=mat('M_Hub_SidePanels');s=M.create_material_expression(a,unreal.MaterialExpressionTextureSample,-400,0);s.set_editor_property('texture',tex)
 uv=M.create_material_expression(a,unreal.MaterialExpressionTextureCoordinate,-400,300)
 mask=custom(a,'float2 p=UV*float2(1672,941); return saturate((Alpha-0.01)/0.99)*((p.x<485 || (p.x>1010 && p.y>140 && p.y<450)) ? 1.0 : 0.0);',unreal.CustomMaterialOutputType.CMOT_FLOAT1,[('Alpha',s,'A'),('UV',uv,'')])
 M.connect_material_property(s,'RGB',unreal.MaterialProperty.MP_EMISSIVE_COLOR);M.connect_material_property(mask,'',unreal.MaterialProperty.MP_OPACITY);finishmat(a);backplate=a
 rows={}
 edge='float2 p=UV*float2(360,70);float edge=min(min(p.y,70-p.y),min(p.x-abs(p.y-35)*0.28,360-p.x-abs(p.y-35)*0.28));'
 for state,fill in [('Normal',NAVY),('Hovered',IVORY),('Pressed',(.59,.42,.22))]:
  a=mat('M_Hub_Row_'+state);uv=M.create_material_expression(a,unreal.MaterialExpressionTextureCoordinate,-400,0)
  c=custom(a,edge+f'return lerp(float3{fill},float3(0.65,0.43,0.20),1-smoothstep(1,2.2,edge));',unreal.CustomMaterialOutputType.CMOT_FLOAT3,[('UV',uv,'')])
  opacity=custom(a,edge+'return saturate(edge)*0.97;',unreal.CustomMaterialOutputType.CMOT_FLOAT1,[('UV',uv,'')])
  M.connect_material_property(c,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR);M.connect_material_property(opacity,'',unreal.MaterialProperty.MP_OPACITY);finishmat(a);rows[state]=a
 bp,t=tree(TILE);cdo=unreal.get_default_object(bp.generated_class());cdo.modify()
 cdo.set_editor_property('bIconOnly',False);cdo.set_editor_property('bOverrideButtonSize',False);cdo.set_editor_property('LabelFontSize',22);cdo.set_editor_property('IconSize',unreal.Vector2D(40,40))
 style=obj(t,'Btn_Common').get_editor_property('widget_style')
 for state,key,fg in [('normal','Normal',IVORY),('hovered','Hovered',NAVY),('pressed','Pressed',NAVY),('disabled','Normal',(.25,.29,.31))]:
  setattr(style,state,brush(rows[key]));setattr(style,state+'_foreground',slate(fg))
 style.normal_padding=unreal.Margin(0,0,0,0);style.pressed_padding=unreal.Margin(0,1,0,-1);obj(t,'Btn_Common').set_style(style)
 rect(obj(t,'Img_Icon'),24,15,40,40)
 o=text(t,'Txt_Label',84,20,218,32,22,IVORY);o.set_justification(unreal.TextJustify.LEFT) if hasattr(o,'set_justification') else o.set_editor_property('justification',unreal.TextJustify.LEFT)
 o.set_color_and_opacity(unreal.SlateColor(color_use_rule=unreal.SlateColorStylingMode.USE_COLOR_FOREGROUND))
 rect(obj(t,'ShortcutKeycap'),312,24,25,21)
 for n,x,y,w,h in [('HubStatBar1',25,35,8,17),('HubStatBar2',37,20,8,32),('HubStatBar3',49,29,8,23),('HubStatBaseline',22,55,38,2)]:rect(obj(t,n),x,y,w,h)
 for n in ['Img_Icon','HubStatBar1','HubStatBar2','HubStatBar3','HubStatBaseline']:
  o=obj(t,n);b=o.get_editor_property('brush');b.tint_color=unreal.SlateColor(color_use_rule=unreal.SlateColorStylingMode.USE_COLOR_FOREGROUND);o.set_brush(b)
 # System actions remain the same bound controls, compacted into the left footer.
 bp,t=tree(SYSTEM);cdo=unreal.get_default_object(bp.generated_class());cdo.modify();cdo.set_editor_property('LabelFontSize',10);cdo.set_editor_property('IconSize',unreal.Vector2D(28,28));cdo.set_editor_property('LabelColor',color(IVORY))
 rect(obj(t,'Img_Icon'),0,8,28,28);o=text(t,'Txt_Label',34,12,82,28,10,IVORY);o.set_render_opacity(1);o.set_editor_property('justification',unreal.TextJustify.LEFT)
 style=obj(t,'Btn_Common').get_editor_property('widget_style')
 for state,rgb in [('normal',IVORY),('hovered',(.96,.7,.35)),('pressed',(.6,.42,.23)),('disabled',(.25,.29,.31))]:setattr(style,state+'_foreground',slate(rgb))
 obj(t,'Btn_Common').set_style(style)
 # Main shell: remove all world-obscuring/portrait widgets; preserve data and delegates.
 for n in ['HubWorldBlur','HubPortrait','HubPortraitFallback']:
  if W.widget_exists(MAIN,n):assert W.remove_component(MAIN,n)
 # Revvy removal leaves stale variable GUIDs until the compiler repairs them.
 # Clear only those removed names before compilation to avoid UMG ensures.
 R=unreal.ObjectReflectionService
 guid_entries=re.findall(r'\("[^"]+", [A-Fa-f0-9]+\)',R.get_property(MAIN,'WidgetVariableNameToGuidMap'))
 guid_entries=[e for e in guid_entries if not any(('"'+n+'"') in e for n in ['HubWorldBlur','HubPortrait','HubPortraitFallback'])]
 assert R.set_property(MAIN,'WidgetVariableNameToGuidMap','('+','.join(guid_entries)+')')
 bp,t=tree(MAIN);fit=obj(t,'HubFit');fit.slot.set_anchors(unreal.Anchors(unreal.Vector2D(0,0),unreal.Vector2D(1,1)));fit.slot.set_offsets(unreal.Margin(0,0,0,0))
 obj(t,'HubBackplate').set_brush_from_material(backplate)
 for n in ['HubBannerM','HubWordmark','TitleText','HubFooterSeparator0','HubFooterSeparator1','HubMotto']:
  obj(t,n).set_visibility(unreal.SlateVisibility.COLLAPSED)
 text(t,'HubBannerProject',126,40,270,34,23,GOLD,'P R O J E C T  M')
 text(t,'HubSubtitle',127,84,275,28,10,IVORY,'A SMALL STEP\nFOR ANOTHER WORLD.')
 rect(obj(t,'HubTitleCompass'),43,37,64,64)
 rect(obj(t,'HubHeaderRule'),55,128,345,1);o=obj(t,'HubHeaderRule');b=o.get_editor_property('brush');b.tint_color=slate(GOLD,.6);o.set_brush(b)
 rect(obj(t,'MainMenuTileView'),48,157,360,596)
 for k,v in [('tile_alignment',unreal.ListItemAlignment.LEFT_ALIGNED),('entry_width',360.0),('entry_height',74.0),('horizontal_entry_spacing',0.0),('vertical_entry_spacing',4.0),('bEntrySizeIncludesEntrySpacing',True)]:obj(t,'MainMenuTileView').set_editor_property(k,v)
 rect(obj(t,'HubFooterRule'),55,803,345,1)
 for n,x in [('OptionsButton',50),('CharacterSelectButton',176),('QuitGameButton',314)]:rect(obj(t,n),x,828,118,48)
 # Native common-button fields on embedded instances override their class defaults.
 for n in ['OptionsButton','CharacterSelectButton','QuitGameButton']:
  o=obj(t,n);o.set_editor_property('LabelFontSize',10);o.set_editor_property('LabelColor',color(IVORY));o.set_editor_property('IconSize',unreal.Vector2D(28,28))
 rect(obj(t,'HubCloseButton'),1588,98,50,50)
 x=obj(t,'HubCloseX');x.set_text('X');x.set_color_and_opacity(slate(IVORY));font=x.get_editor_property('font');font.size=26;x.set_font(font)
 text(t,'HubLevel',1070,190,215,32,24,NAVY)
 text(t,'HubName',1070,227,228,42,29,NAVY)
 rect(obj(t,'HubExperienceBar'),1070,287,214,7)
 text(t,'HubExperience',1070,305,214,24,14,NAVY)
 for label,value,y in [('HubLocationLabel','HubLocation',206),('HubJobLabel','HubJob',246),('HubGoldLabel','HubGold',286),('HubCharacterIdLabel','HubCharacterId',326)]:
  text(t,label,1320,y,120,27,15,(.27,.19,.12));text(t,value,1444,y,146,27,15,NAVY)
 rect(obj(t,'HubMottoInput'),1066,357,523,34)
 inp=obj(t,'HubMottoInput');style=inp.get_editor_property('widget_style')
 for n in ['foreground_color','focused_foreground_color','read_only_foreground_color']:setattr(style,n,slate(NAVY))
 ts=style.text_style;font=ts.font;font.size=15;ts.font=font;style.text_style=ts
 inp.set_editor_property('widget_style',style)
 text(t,'HubMottoHint',1070,396,450,18,10,(.28,.23,.17))
 report={}
 # Compile dependencies before parent to propagate entry styles.
 for p in [TILE,SYSTEM,MAIN]:
  r=B.compile_blueprint(p);assert r.success and r.num_errors==0,r.export_text()
  assert unreal.ObjectReflectionService.get_property(p,'Bindings')==bindings[p]
  assert E.save_asset(p,False)
  report[p]={'errors':r.num_errors,'warnings':r.num_warnings}
 (OUT/'applied.json').write_text(json.dumps(report,indent=2))
if __name__ == '__main__':
 apply()
