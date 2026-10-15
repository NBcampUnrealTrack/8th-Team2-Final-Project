"""Settings-derived panel palette. Preserve layout, input, graph wiring and non-hub motifs."""
import unreal,json,re,shutil
from pathlib import Path
from datetime import datetime
ROOT=Path(unreal.Paths.project_dir()).resolve()
OUT=ROOT/'Saved/UIPanelConsistency'
ART='/Game/Developers/MinkiCho/Art/UITheme/SettingsNavy'
W=unreal.WidgetService;B=unreal.BlueprintService;E=unreal.EditorAssetLibrary
M=unreal.MaterialEditingLibrary;R=unreal.ObjectReflectionService
NAVY=(.008,.022,.046);SURFACE=(.016,.035,.063);TEXT=(.84,.88,.93);GOLD=(.83,.64,.32)
NAMES={'WBP_GameMenu','WBP_GameMenuTileEntry','WBP_GameMenuSystem',
 'WBP_CharacterMenuBase','WBP_CharacterInfoPage','WBP_CharacterEquipmentPage',
 'WBP_CharacterInventoryPage','WBP_CharacterInventoryCell','WBP_CharacterStatsPage',
 'WBP_CharacterTraitsPage','WBP_CharacterNaiaPage','WBP_InventoryWindow','WBP_EquipmentWindow',
 'WBP_StorageWindow','WBP_TradeWindow','WBP_CraftWindow','WBP_WorldMapWindow',
 'WBP_QuestJournal','WBP_QuestJournalEntry','WBP_QuestJournalObjectiveRow','WBP_QuestJournalRewardTile',
 'WBP_QuestWidget','WBP_ShopPage','WBP_ShopItemEntry','WBP_ShopInventorySlot',
 'WBP_ShopWindow','WBP_DialogueWidget','WBP_DialogueOptionEntryPanel2','WBP_NaiaRosterWindow'}
# User requested original CharacterMenu and quest designs on 2026-10-06.
NAMES={name for name in NAMES if not name.startswith(('WBP_Character','WBP_Quest'))}
# Preserve the user's restored ivory right panel and its original foreground colors.
NAMES.difference_update({'WBP_GameMenu','WBP_GameMenuTileEntry','WBP_GameMenuSystem'})
# 2026-10-08: shop retains its original ivory family; never recolor it here.
NAMES.difference_update({'WBP_ShopPage','WBP_ShopItemEntry','WBP_ShopInventorySlot','WBP_ShopWindow'})
# Final per-screen settings-family pass owns these windows now.
NAMES.difference_update({'WBP_TradeWindow','WBP_CraftWindow'})
TEXTURE_BACKDROPS={'T_Default_Panel','T_CharacterMenu_Atmosphere','T_QuestV4_Paper',
 'T_QuestJournal_Navy','T_QuestFrame_Reading','T_Dialogue_Panel2'}
COLOR_CODE='''float3 c=max(InColor,0.0);
float lum=dot(c,float3(0.2126,0.7152,0.0722));
float gold=smoothstep(0.30,0.60,(c.r-c.b)/max(c.r,0.0001))*smoothstep(0.035,0.16,c.r-c.g);
float3 navy=float3(0.008,0.022,0.046)*(0.72+0.48*saturate(lum));
return lerp(navy,c,gold);'''

def lc(rgb,a=1):return unreal.LinearColor(*rgb,a)
def rgba(c):return (c.r,c.g,c.b,c.a)
def slate(rgb,a=1):return unreal.SlateColor(specified_color=lc(rgb,a))
def snapshot(path):
 return [{'name':r.widget_name,'class':r.widget_class,'parent':r.parent_widget,
          'slot':r.slot_info.export_text(),'props':{p.property_name:p.current_value for p in r.properties}}
         for r in W.get_widget_snapshot(path)]

def new_color(c,text=False):
 r,g,b,a=rgba(c)
 if a==0:return None
 if any(max(abs(x-y) for x,y in zip((r,g,b),rgb))<.00001 for rgb in [NAVY,SURFACE]):return None
 if text:
  if max(r,g,b)<.48 and abs(r-g)<.20 and b>=r*.65:return lc(TEXT,a)
  if r>g>b and r<.6 and r-b>.12:return lc(GOLD,a)
 else:
  if min(r,g,b)>.30 and max(r,g,b)<.99 and max(r,g,b)-min(r,g,b)<.33:return lc(SURFACE,a)
  if b>r*1.12 and b<.18 and r<.10:return lc(NAVY,a)
 return None

materials={};diags={}

def quest_tab_colors():
 path='/Game/Developers/zxcv1/Quest/UI/WBP_QuestJournal'
 graph='EventGraph'
 before=B.describe_graph(path,graph)[0]
 nodes=['2DDF9A5F44A4E3806D6A7DA07FCD0F7D','4E6326E546499B1B4B9AC8A3F100C041','29727864487F5A70F7A3119C3EDE570D','2CBEF51241D60FBB422CA6B1B3C6A582','F99C81DD4816D0F4B2F71E89B5E9ECBA']
 for node in nodes:
  assert B.set_node_pin_value(path,graph,node,'A','(R=.83,G=.64,B=.32,A=1)')
  assert B.set_node_pin_value(path,graph,node,'B','(R=.84,G=.88,B=.93,A=1)')
 after=B.describe_graph(path,graph)[0]
 assert [e.export_text() for e in before.edges]==[e.export_text() for e in after.edges]
 result=B.compile_blueprint(path);assert result.success and result.num_errors==0
 assert E.save_asset(path,False)
 (OUT/'quest_tab_colors.json').write_text(json.dumps({'nodes':nodes,'errors':result.num_errors,'warnings':result.num_warnings,'edges_unchanged':True},indent=2))
def recolor_node(mat,source,output=''):
 node=M.create_material_expression(mat,unreal.MaterialExpressionCustom,500,0)
 node.set_editor_property('code',COLOR_CODE)
 node.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
 inp=unreal.CustomInput();inp.set_editor_property('input_name','InColor');node.set_editor_property('inputs',[inp])
 assert M.connect_material_expressions(source,output,node,'InColor')
 assert M.connect_material_property(node,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
 return node

def recolored(resource):
 key=resource.get_path_name()
 if key in materials:return materials[key]
 name='M_Navy_'+resource.get_name();path=ART+'/'+name
 if E.does_asset_exist(path) and isinstance(resource,unreal.Material):
  result=unreal.load_asset(path);materials[key]=result;return result
 if isinstance(resource,unreal.Texture2D):
  result=unreal.load_asset(path) if E.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,ART,unreal.Material,unreal.MaterialFactoryNew())
  M.delete_all_material_expressions(result)
  result.set_editor_property('material_domain',unreal.MaterialDomain.MD_UI)
  result.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
  tex=M.create_material_expression(result,unreal.MaterialExpressionTextureSample,-400,0)
  tex.set_editor_property('texture',resource)
  recolor_node(result,tex,'RGB')
  assert M.connect_material_property(tex,'A',unreal.MaterialProperty.MP_OPACITY)
 elif isinstance(resource,unreal.Material):
  (OUT/(resource.get_name()+'.original_graph.json')).write_text(unreal.MaterialNodeService.export_material_graph(key),encoding='utf-8')
  result=E.duplicate_asset(key,path);assert result
  src=M.get_material_property_input_node(result,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
  pin=M.get_material_property_input_node_output_name(result,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
  assert src,key
  recolor_node(result,src,pin)
 else:
  result=unreal.load_asset(path) if E.does_asset_exist(path) else E.duplicate_asset(key,path)
  assert isinstance(result,unreal.MaterialInstanceConstant)
  values=result.get_editor_property('vector_parameter_values')
  for i,entry in enumerate(values):
   parameter=str(entry.parameter_info.name)
   if parameter in ['SurfaceColor','BackgroundColor','FillColor']:
    entry.set_editor_property('parameter_value',lc(NAVY,entry.parameter_value.a))
    values[i]=entry
  result.set_editor_property('vector_parameter_values',values)
 if isinstance(result,unreal.Material):
  errors=M.recompile_material(result);assert not errors,list(errors)
  diag=unreal.MaterialNodeService.get_material_diagnostics(path)
  assert diag.success and diag.is_compiled_ok,diag.export_text()
  diags[path]=diag.export_text()
 assert E.save_loaded_asset(result)
 materials[key]=result;return result

def patch_brush(brush,is_background=True):
 changed=False
 resource=brush.get_editor_property('resource_object')
 if resource and (resource.get_name() in TEXTURE_BACKDROPS or resource.get_name() in ['M_Shop_BackdropTrim','MI_Shop_Paper','MI_Shop_Navy','MI_Dialogue_Choices']):
  brush.set_editor_property('resource_object',recolored(resource));changed=True
 elif not resource or resource.get_name()=='WhiteSquareTexture':
  tint=brush.tint_color
  c=new_color(tint.specified_color)
  if c is not None:
   tint.specified_color=c;brush.tint_color=tint;changed=True
 return changed

def apply():
 assert not W.is_pie_running()
 OUT.mkdir(exist_ok=True)
 backup=OUT/datetime.now().strftime('%Y%m%d_%H%M%S');backup.mkdir()
 paths=[]
 for f in (ROOT/'Content/Developers').rglob('WBP_*.uasset'):
  if f.stem in NAMES:
   p='/Game/'+f.relative_to(ROOT/'Content').with_suffix('').as_posix();paths.append(p)
   target=backup/f.relative_to(ROOT/'Content');target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(f,target)
 before={p:snapshot(p) for p in paths}
 bindings={p:R.get_property(p,'Bindings') for p in paths}
 guids={p:R.get_property(p,'WidgetVariableNameToGuidMap') for p in paths}
 (backup/'before.json').write_text(json.dumps(before,ensure_ascii=False),encoding='utf-8')
 art={}
 for name in ['T_MenuHub_SettingsNavy','T_Settings_Compass']:
  p=ART+'/'+name
  if not E.does_asset_exist(p):
   imported,error=unreal.AssetDiscoveryService.import_asset(str(ROOT/'ArtSource/UI/PanelConsistency'/(name+'.png')),ART,name)
   assert imported,error
  a=unreal.load_asset(p);a.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON)
  a.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI);a.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
  a.set_editor_property('srgb',True);a.set_editor_property('compression_no_alpha',False);a.set_editor_property('never_stream',True)
  if name=='T_Settings_Compass':a.set_editor_property('max_texture_size',256)
  assert E.save_loaded_asset(a);art[name]=a
 report={}
 for path in paths:
  bp=unreal.load_asset(path);tree=unreal.find_object(bp,'WidgetTree');bp.modify();tree.modify()
  changes=[]
  if path.endswith('WBP_CharacterMenuBase'):
   cdo=unreal.get_default_object(bp.generated_class());cdo.modify()
   cdo.set_editor_property('SelectedNavigationColor',lc(GOLD))
   cdo.set_editor_property('IdleNavigationColor',lc(TEXT));changes.append('navigation state colors')
  for row in before[path]:
   name=row['name'];obj=unreal.find_object(tree,name);obj.modify()
   # Quest tracker shares a blueprint with the offer; only recolor the offer branch.
   if path.endswith('WBP_QuestWidget') and not (name.startswith('QO_') or name in ['OfferPanel','Txt_Quest_Name','Txt_Quest_Description']):continue
   if isinstance(obj,unreal.Image):
    if path.endswith('WBP_GameMenu') and name in ['HubBackplate','HubTitleCompass']:
     obj.set_brush_from_texture(art['T_MenuHub_SettingsNavy' if name=='HubBackplate' else 'T_Settings_Compass'],False)
     brush=obj.get_editor_property('brush');brush.tint_color=slate((1,1,1));obj.set_brush(brush)
     changes.append(name+':hub artwork');continue
    brush=obj.get_editor_property('brush')
    resource=brush.get_editor_property('resource_object')
    if resource and any(t in name for t in ['Icon','Emblem','ProfileCompass']):
     c=new_color(brush.tint_color.specified_color,True)
     if c is not None:brush.tint_color=unreal.SlateColor(specified_color=c);obj.set_brush(brush);changes.append(name+':icon contrast')
     c=new_color(obj.get_editor_property('color_and_opacity'),True)
     if c is not None:obj.set_color_and_opacity(c);changes.append(name+':icon contrast')
    if patch_brush(brush):obj.set_brush(brush);changes.append(name+':brush')
    if name in ['InfoLocationPin','InfoLocationTip','InfoIdSheet','InfoGoldCoin','InfoGoldCore','QR_LocationPinHead'] or name.startswith(('ViewCompactGrid','ViewListLine','ViewLargeGrid','Trash','ShopAllGridIcon','TabPotion')):
     brush.tint_color=slate(TEXT,brush.tint_color.specified_color.a);obj.set_brush(brush)
     obj.set_color_and_opacity(lc((1,1,1),obj.get_editor_property('color_and_opacity').a));changes.append(name+':symbol contrast')
   elif isinstance(obj,unreal.Border):
    if name=='ShortcutKeycap':
     brush=obj.get_editor_property('background');brush.tint_color=slate((.72,.69,.56),.96);obj.set_brush(brush);changes.append(name+':keycap contrast');continue
    brush=obj.get_editor_property('background')
    if patch_brush(brush):obj.set_brush(brush);changes.append(name+':background')
    c=new_color(obj.get_editor_property('brush_color'))
    if c is not None:obj.set_brush_color(c);changes.append(name+':tint')
   elif isinstance(obj,unreal.TextBlock):
    if path.endswith(('WBP_ShopItemEntry','WBP_ShopPage')) and name in ['BuyButtonLabel','PurchaseButtonLabel']:
     obj.set_color_and_opacity(slate(NAVY));changes.append(name+':gold button contrast');continue
    # Keep dark glyphs on ivory keyboard keycaps.
    if 'keycap' in (name+row['parent']).lower() or 'Shortcut' in name:continue
    old=obj.get_editor_property('color_and_opacity')
    if old.color_use_rule!=unreal.SlateColorStylingMode.USE_COLOR_SPECIFIED:continue
    c=new_color(old.specified_color,True)
    if c is not None:obj.set_color_and_opacity(unreal.SlateColor(specified_color=c));changes.append(name+':text')
   elif isinstance(obj,unreal.Button):
    old=obj.get_editor_property('color_and_opacity')
    c=new_color(old,True)
    if c is not None:obj.set_color_and_opacity(c);changes.append(name+':content contrast')
    style=obj.get_editor_property('widget_style');changed=False
    for state in ['normal','hovered','pressed','disabled']:
     brush=getattr(style,state)
     if patch_brush(brush):setattr(style,state,brush);changed=True
     foreground=getattr(style,state+'_foreground')
     c=new_color(foreground.specified_color,True)
     if c is not None:setattr(style,state+'_foreground',unreal.SlateColor(specified_color=c));changed=True
    if path.endswith(('WBP_GameMenuTileEntry','WBP_GameMenuSystem')):
     for state,rgb in [('normal',TEXT),('hovered',GOLD),('pressed',(.65,.46,.23)),('disabled',(.25,.31,.39))]:
      setattr(style,state+'_foreground',slate(rgb))
     changed=True
    if changed:obj.set_style(style);changes.append(name+':button colors')
  result=B.compile_blueprint(path);assert result.success and result.num_errors==0,result.export_text()
  after=snapshot(path)
  assert [(r['name'],r['class'],r['parent'],r['slot']) for r in after]==[(r['name'],r['class'],r['parent'],r['slot']) for r in before[path]],'Layout changed: '+path
  assert R.get_property(path,'Bindings')==bindings[path]
  assert R.get_property(path,'WidgetVariableNameToGuidMap')==guids[path]
  if changes:assert E.save_asset(path,False)
  report[path]={'changes':changes,'errors':result.num_errors,'warnings':result.num_warnings}
 # Shop Blueprints restore original instance brushes when selecting tabs/rows.
 # Update only their panel-color parameters, never the shared master material.
 shop_materials=[]
 for f in (ROOT/'Content/Developers/MinkiCho/Art').rglob('*.uasset'):
  if f.stem.startswith(('MI_Shop_','M_Navy_MI_Shop_')):continue
  if not (f.stem.startswith('MI_Shop_') or f.stem.startswith('M_Navy_MI_')):continue
  p='/Game/'+f.relative_to(ROOT/'Content').with_suffix('').as_posix()
  a=unreal.load_asset(p)
  if not isinstance(a,unreal.MaterialInstanceConstant):continue
  vals=a.get_editor_property('vector_parameter_values');changed=False
  for i,v in enumerate(vals):
   if str(v.parameter_info.name)!='SurfaceColor':continue
   c=v.parameter_value
   if 'Gold' in f.stem:
    rgb=(.96,.78,.44) if 'Hover' in f.stem else ((.57,.39,.15) if 'Pressed' in f.stem else GOLD)
   else:
    rgb=(.034,.065,.092) if 'Hover' in f.stem else ((.004,.012,.025) if 'Pressed' in f.stem else (SURFACE if 'Selected' in f.stem else NAVY))
   v.set_editor_property('parameter_value',lc(rgb,c.a));vals[i]=v;changed=True
  if changed:
   target=backup/f.relative_to(ROOT/'Content');target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(f,target)
   a.modify();a.set_editor_property('vector_parameter_values',vals)
   M.update_material_instance(a);assert E.save_loaded_asset(a);shop_materials.append(p)
 if 'WBP_QuestJournal' in NAMES:quest_tab_colors()
 (OUT/'shop_materials.json').write_text(json.dumps(shop_materials,indent=2))
 (OUT/'applied.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
 (OUT/'materials_verified.json').write_text(json.dumps(diags,indent=2),encoding='utf-8')
 return report
