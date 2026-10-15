"""Opt-in exhibit depth layers; existing character/floor/card/input bindings preserved."""
import unreal,json,shutil
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve();P='/Game/Developers/MinkiCho/UI/CharacterMenu/WBP_CharacterInfoPage';ART='/Game/Developers/MinkiCho/Art/CharacterMenu/Depth20261014'
E=unreal.EditorAssetLibrary;M=unreal.MaterialEditingLibrary;W=unreal.WidgetService
def rgb(c):return f'float3({c.r:.8f},{c.g:.8f},{c.b:.8f})'
# Read the current shell palette instead of importing historical style passes.
shell=unreal.load_asset('/Game/Developers/MinkiCho/UI/CharacterMenu/WBP_CharacterMenuBase')
shelltree=unreal.find_object(shell,'WidgetTree')
IVORY=unreal.find_object(shelltree,'IvorySurface').get_editor_property('brush').tint_color.specified_color
GOLD=unreal.find_object(shelltree,'GoldFrame0').get_editor_property('brush').tint_color.specified_color
assert not W.is_pie_running()

def texmaterial(name,texture,code):
 p=ART+'/'+name;mat=unreal.load_asset(p)
 if not mat:mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,ART,unreal.Material,unreal.MaterialFactoryNew())
 mat.modify();M.delete_all_material_expressions(mat);mat.set_editor_property('material_domain',unreal.MaterialDomain.MD_UI);mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
 uv=M.create_material_expression(mat,unreal.MaterialExpressionTextureCoordinate,-600,0)
 tex=M.create_material_expression(mat,unreal.MaterialExpressionTextureObject,-600,140);tex.set_editor_property('texture',unreal.load_asset(texture))
 custom=M.create_material_expression(mat,unreal.MaterialExpressionCustom,-200,0)
 inputs=[]
 for key in ['UV','Art']:
  ci=unreal.CustomInput();ci.set_editor_property('input_name',key);inputs.append(ci)
 custom.set_editor_property('inputs',inputs);custom.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT4);custom.set_editor_property('code',code)
 M.connect_material_expressions(uv,'',custom,'UV');M.connect_material_expressions(tex,'',custom,'Art')
 for c,prop in [('RGB',unreal.MaterialProperty.MP_EMISSIVE_COLOR),('A',unreal.MaterialProperty.MP_OPACITY)]:
  mask=M.create_material_expression(mat,unreal.MaterialExpressionComponentMask,150,0 if c=='RGB' else 150)
  for channel in ['r','g','b','a']:mask.set_editor_property(channel,channel.upper() in c)
  M.connect_material_expressions(custom,'',mask,'');M.connect_material_property(mask,'',prop)
 M.recompile_material(mat);d=unreal.MaterialNodeService.get_material_diagnostics(p);assert d.success and d.is_compiled_ok,d.export_text();assert E.save_loaded_asset(mat)
 return mat
paper='/Game/Developers/MinkiCho/Art/Quest/JournalV4/Textures/T_QuestV4_Paper'
atlas='/Game/Developers/MinkiCho/Art/CharacterMenu/Ornaments/T_CharacterMenu_Ornaments'
far=texmaterial('M_DepthFarTexture',paper,'float3 c=Texture2DSample(Art,ArtSampler,UV).rgb;float l=dot(c,float3(.2126,.7152,.0722));float diag=sin((UV.x*.82-UV.y)*28)*.007;float detail=(l-.65)*.045+diag;float edge=smoothstep(0,.08,min(min(UV.x,1-UV.x),min(UV.y,1-UV.y)));return float4('+rgb(IVORY)+'+detail,edge*.32);')
rear=texmaterial('M_DepthRearOrbit',atlas,'float4 c=Texture2DSample(Art,ArtSampler,float2(UV.x*.5,.5+UV.y*.5));return float4('+rgb(GOLD)+',c.a*.13);')
front=texmaterial('M_DepthFrontLine',atlas,'float4 c=Texture2DSample(Art,ArtSampler,float2(.5+UV.x*.5,.64+UV.y*.19));return float4('+rgb(GOLD)+',c.a*.14);')
bp=unreal.load_asset(P);bp.modify();tree=unreal.find_object(bp,'WidgetTree')
backup=ROOT/'Saved/UICharacterDepth';backup.mkdir(exist_ok=True)
if not (backup/'WBP_CharacterInfoPage.before.uasset').exists():shutil.copy2(ROOT/'Content'/(P[6:]+'.uasset'),backup/'WBP_CharacterInfoPage.before.uasset')
old={s.widget_name:s for s in W.get_widget_snapshot(P)}

def obj(n):return unreal.find_object(tree,n)
def add(cls,n,parent):
 if not obj(n):
  r=W.add_component(P,cls,n,parent,True);assert r.success,r.export_text()
 w=obj(n);w.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE);return w
def rect(w,x,y,ww,hh,z):
 w.slot.set_position(unreal.Vector2D(x,y));w.slot.set_size(unreal.Vector2D(ww,hh));w.slot.set_z_order(z)
def brush(w,res=None,c=None):
 b=unreal.SlateBrush();b.draw_as=unreal.SlateBrushDrawType.IMAGE
 if res:b.resource_object=res
 b.tint_color=unreal.SlateColor(specified_color=c or unreal.LinearColor(1,1,1,1));w.set_brush(b)
exhibit=add('CanvasPanel','DepthExhibit','PageCanvas');rect(exhibit,0,0,608,600,-2);exhibit.set_clipping(unreal.WidgetClipping.CLIP_TO_BOUNDS_ALWAYS)
base=add('Image','DepthBase','DepthExhibit');rect(base,0,0,608,600,-5);brush(base,c=IVORY)
f=add('Image','DepthFarTexture','DepthExhibit');rect(f,-24,-24,656,648,-4);brush(f,far)
# Reparent the existing independent illustration group at the same authored origin.
atmosphere=obj('PortraitAtmosphere')
if atmosphere.get_parent()!=exhibit:
 layout=atmosphere.slot.get_layout();atmosphere.get_parent().remove_child(atmosphere);s=exhibit.add_child_to_canvas(atmosphere);s.set_layout(layout)
atmosphere.slot.set_z_order(-2)
r=add('Image','DepthRearOrbit','DepthExhibit');rect(r,104,44,512,512,-3);brush(r,rear);r.set_render_transform_pivot(unreal.Vector2D(.5,.5))
obj('PortraitFloorLayer').set_clipping(unreal.WidgetClipping.CLIP_TO_BOUNDS_ALWAYS)
nearclip=add('CanvasPanel','DepthForegroundClip','PageCanvas');rect(nearclip,0,0,608,600,2);nearclip.set_clipping(unreal.WidgetClipping.CLIP_TO_BOUNDS_ALWAYS)
f=add('Image','DepthFrontLine','DepthForegroundClip');rect(f,100,464,460,88,0);brush(f,front)
# Bound portrait, floor and reflection transforms remain exactly as before.
result=unreal.BlueprintService.compile_blueprint(P);assert result.success,result.export_text();assert E.save_asset(P,False)
report={'asset':P,'compile':result.export_text(),'ordered_layers':['DepthBase','DepthFarTexture','DepthRearOrbit','CharacterIllustration','PortraitFloorLayer / CharacterPortrait (unchanged)','DepthFrontLine (below pixel character)','InfoContent / shell UI (unchanged)'],'resources':{'far':paper,'rear_and_front':atlas,'illustration':'existing CharacterIllustration resource'},'fixed_originals':['CharacterPortrait','PortraitFloorLayer','MirrorFloorSurface','CharacterReflection','InfoContent'],'clip':[608,600],'texture_overscan':24}
(backup/'layers.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print('DEPTH_LAYERS_SAVED',result.export_text())

