"""Flush left rail, reference desk decoration, independent row/footer hover states."""
import unreal,importlib.util,json,shutil
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve()
spec=importlib.util.spec_from_file_location('hub_reference',ROOT/'Tools/UIHubSidePanels/apply_reference.py')
r=importlib.util.module_from_spec(spec);spec.loader.exec_module(r)
h=r.h;M=h.M;E=h.E;W=h.W;B=h.B
OUT=ROOT/'Saved/UIHubInteraction';OUT.mkdir(exist_ok=True)
LEFT=[(-2,-2),(447,-2),(420,140),(433,182),(405,215),(394,340),(391,465),(411,632),(421,775),(549,795),(568,838),(561,876),(545,901),(-2,901)]

def add(typ,name,parent):
 if not W.widget_exists(h.MAIN,name):
  result=W.add_component(h.MAIN,typ,name,parent,True);assert result.success,result.export_text()
 return h.obj(h.tree(h.MAIN)[1],name)

def uv_expression(a,code,inputs):return h.custom(a,code,unreal.CustomMaterialOutputType.CMOT_FLOAT2,inputs)

def material(name,tex,uvcode,maskcode,variant=None,colorcode=None,scalar=False):
 a=h.mat(name);uv=M.create_material_expression(a,unreal.MaterialExpressionTextureCoordinate,-700,0)
 inputs=[('UV',uv,'')]
 if scalar:
  q=M.create_material_expression(a,unreal.MaterialExpressionScalarParameter,-700,200);q.set_editor_property('parameter_name','RowIndex');q.set_editor_property('default_value',0);inputs.append(('RowIndex',q,''))
 mapped=uv_expression(a,uvcode,inputs)
 s=r.sample(a,tex);M.connect_material_expressions(mapped,'',s,'UVs')
 rgb=s;pin='RGB'
 if variant:
  t=r.sample(a,variant);M.connect_material_expressions(mapped,'',t,'UVs')
  rgb=h.custom(a,colorcode,unreal.CustomMaterialOutputType.CMOT_FLOAT3,inputs+[('Original',s,'RGB'),('Variant',t,'RGB')]);pin=''
 mask=h.custom(a,maskcode,unreal.CustomMaterialOutputType.CMOT_FLOAT1,inputs)
 M.connect_material_property(rgb,pin,unreal.MaterialProperty.MP_EMISSIVE_COLOR);M.connect_material_property(mask,'',unreal.MaterialProperty.MP_OPACITY);h.finishmat(a);return a

def fullslot(o,z):
 s=o.slot;s.set_anchors(unreal.Anchors(unreal.Vector2D(0,0),unreal.Vector2D(1,1)));s.set_offsets(unreal.Margin(0,0,0,0));s.set_alignment(unreal.Vector2D(0,0));s.set_z_order(z)

def fit_group(prefix,width,height,stretch,horizontal,z):
 fit=add('ScaleBox',prefix+'Fit','RootCanvas');fullslot(fit,z);fit.set_stretch(stretch)
 size=add('SizeBox',prefix+'Size',prefix+'Fit');size.set_width_override(width);size.set_height_override(height)
 size.slot.set_horizontal_alignment(horizontal);size.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_BOTTOM)
 canvas=add('CanvasPanel',prefix+'Canvas',prefix+'Size')
 for o in [fit,size,canvas]:o.set_visibility(unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
 return canvas

def apply():
 assert not W.is_pie_running()
 r.apply()
 p=h.ART+'/T_Hub_ButtonStates'
 if not E.does_asset_exist(p):
  imported,error=unreal.AssetDiscoveryService.import_asset(str(ROOT/'ArtSource/UI/HubSidePanels/T_Hub_ButtonStates.png'),h.ART,'T_Hub_ButtonStates');assert imported,error
 states=unreal.load_asset(p);original=unreal.load_asset(h.ART+'/T_Hub_Reference2')
 for k,v in [('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON),('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI),('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS),('srgb',True),('compression_no_alpha',False),('never_stream',True)]:states.set_editor_property(k,v)
 assert E.save_loaded_asset(states)
 leftmat=material('M_Hub_LeftFlush',original,'return UV*float2(568,899)/float2(1672,941);','float2 p=UV*float2(568,899);'+r.polygon(LEFT),states,'float2 p=UV*float2(568,899);return (p.x>47 && p.x<432 && p.y>=144 && p.y<212.5) ? Variant:Original;')
 cutpath=h.ART+'/T_Hub_DeskCutout'
 if not E.does_asset_exist(cutpath):
  imported,error=unreal.AssetDiscoveryService.import_asset(str(ROOT/'ArtSource/UI/HubSidePanels/T_Hub_DeskCutout.png'),h.ART,'T_Hub_DeskCutout');assert imported,error
 cutout=unreal.load_asset(cutpath)
 for k,v in [('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON),('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI),('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS),('compression_no_alpha',False),('never_stream',True)]:cutout.set_editor_property(k,v)
 assert E.save_loaded_asset(cutout)
 # Use the cutout only for its detailed alpha silhouette; retain original RGB.
 deskmat=h.mat('M_Hub_Desk');s=r.sample(deskmat,original);cut=r.sample(deskmat,cutout)
 alpha=h.custom(deskmat,'return saturate((A-.02)/.98);',unreal.CustomMaterialOutputType.CMOT_FLOAT1,[('A',cut,'A')])
 M.connect_material_property(s,'RGB',unreal.MaterialProperty.MP_EMISSIVE_COLOR);M.connect_material_property(alpha,'',unreal.MaterialProperty.MP_OPACITY);h.finishmat(deskmat)
 # Existing centered fit continues to own only the upper-right profile card.
 a=unreal.load_asset(h.ART+'/M_Hub_Reference2')
 # Reconnect opacity with a right-card-only mask, leaving exact profile RGB untouched.
 uv=M.create_material_expression(a,unreal.MaterialExpressionTextureCoordinate,-800,600)
 RIGHT=[(1024,180),(1100,171),(1320,160),(1510,147),(1560,146),(1600,142),(1672,139),(1672,424),(1625,430),(1585,427),(1540,446),(1430,436),(1340,426),(1250,416),(1115,408),(1040,405),(1019,378),(1019,240),(1017,195)]
 mask=h.custom(a,'float2 p=UV*float2(1672,941);'+r.polygon(RIGHT),unreal.CustomMaterialOutputType.CMOT_FLOAT1,[('UV',uv,'')])
 M.connect_material_property(mask,'',unreal.MaterialProperty.MP_OPACITY);h.finishmat(a)
 left=fit_group('HubLeft',568,899,unreal.Stretch.SCALE_TO_FIT_Y,unreal.HorizontalAlignment.H_ALIGN_LEFT,2)
 desk=fit_group('HubDesk',1672,941,unreal.Stretch.SCALE_TO_FIT,unreal.HorizontalAlignment.H_ALIGN_RIGHT,-1)
 img=add('Image','HubLeftBackplate','HubLeftCanvas');h.rect(img,0,0,568,899);img.set_brush_from_material(leftmat);img.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE);img.slot.set_z_order(-1)
 img=add('Image','HubDeskDecor','HubDeskCanvas');h.rect(img,0,0,1672,941);img.set_brush_from_material(deskmat);img.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
 bp,t=h.tree(h.MAIN)
 for n,x,y,w,hh in [('MainMenuTileView',47,144,385,620),('OptionsButton',61,829,123,49),('CharacterSelectButton',204,829,186,49),('QuitGameButton',399,829,149,49)]:
  o=h.obj(t,n)
  if o.get_parent()!=left:o.remove_from_parent();left.add_child_to_canvas(o)
  h.rect(o,x,y,w,hh);o.slot.set_z_order(1)
  if n!='MainMenuTileView':o.set_editor_property('bIconOnly',False)
 # Reference row atlas. First source image is hovered row 0 + idle rows 1..8;
 # variant is idle row 0 + hovered rows 1..8. Native code sets RowIndex per item.
 rowuv='return (float2(47,144+RowIndex*68.5)+UV*float2(385,68.5))/float2(1672,941);'
 rowmask='float2 p=float2(47,144+RowIndex*68.5)+UV*float2(385,68.5);if(RowIndex<0)return 0;'+r.polygon(LEFT)
 hovered=material('M_Hub_RowIvory',original,rowuv,rowmask,states,'return RowIndex<0.5 ? Original:Variant;',True)
 pressed=material('M_Hub_RowIvoryPressed',original,rowuv,rowmask,states,'return (RowIndex<0.5 ? Original:Variant)*float3(.88,.85,.79);',True)
 bp,t=h.tree(h.TILE);st=h.obj(t,'Btn_Common').get_editor_property('widget_style')
 for key,resource in [('hovered',hovered),('pressed',pressed)]:setattr(st,key,h.brush(resource))
 empty=unreal.SlateBrush();empty.draw_as=unreal.SlateBrushDrawType.NO_DRAW_TYPE;st.normal=empty;st.disabled=empty
 st.normal_padding=unreal.Margin(0,0,0,0);st.pressed_padding=unreal.Margin(0,0,0,0)
 h.obj(t,'Btn_Common').set_style(st)
 # Footer: warm translucent fill and a crisp gold frame.
 bp,t=h.tree(h.SYSTEM);st=h.obj(t,'Btn_Common').get_editor_property('widget_style');st.normal=empty;st.disabled=empty
 for key,alpha in [('hovered',.18),('pressed',.30)]:
  b=unreal.SlateBrush();b.draw_as=unreal.SlateBrushDrawType.ROUNDED_BOX;b.tint_color=h.slate((.83,.57,.27),alpha)
  outline=b.outline_settings;outline.corner_radii=unreal.Vector4(5,5,5,5);outline.rounding_type=unreal.SlateBrushRoundingType.FIXED_RADIUS;outline.width=1.2;outline.color=h.slate((.83,.57,.27),.85);b.outline_settings=outline;setattr(st,key,b)
 h.obj(t,'Btn_Common').set_style(st)
 report={}
 for p in [h.TILE,h.SYSTEM,h.MAIN]:
  result=B.compile_blueprint(p);assert result.success and result.num_errors==0,result.export_text();assert E.save_asset(p,False)
  report[p]={'errors':result.num_errors,'warnings':result.num_warnings}
 (OUT/'applied.json').write_text(json.dumps(report,indent=2))
if __name__=='__main__':apply()
