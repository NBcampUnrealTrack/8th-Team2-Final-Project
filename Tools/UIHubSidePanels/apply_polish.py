"""Clean illustrated hub textures with restrained, neutral champagne accents."""
import importlib.util,json
from pathlib import Path
import unreal
ROOT=Path(unreal.Paths.project_dir()).resolve()
spec=importlib.util.spec_from_file_location('hub_edge',ROOT/'Tools/UIHubSidePanels/apply_edge_fade.py')
e=importlib.util.module_from_spec(spec);spec.loader.exec_module(e)
spec_mask=importlib.util.spec_from_file_location('hub_row_mask',ROOT/'Tools/UIHubSidePanels/row_mask.py')
row_masks=importlib.util.module_from_spec(spec_mask);spec_mask.loader.exec_module(row_masks)
i=e.i;h=e.h;M=h.M;r=i.r
OUT=ROOT/'Saved/UIHubPolish';OUT.mkdir(exist_ok=True)
LEFT=[(-2,-2),(408,-2),(394,144),(416,178),(392,210),(402,248),(386,277),(385,346),(384,415),(384,484),(386,553),(390,622),(401,692),(410,759),(445,801),(550,801),(568,846),(550,899),(-2,899)]
COOL='float l=dot(C,float3(.2126,.7152,.0722));float w=saturate((C.r-C.b)*5);return lerp(C,lerp(l.xxx,C,.35)*float3(.96,1,1.06),w*.8);'

def surface(name,texture,uvcode,maskcode,scalar=False,pressed=False,alpha=False):
 a=h.mat(name);uv=M.create_material_expression(a,unreal.MaterialExpressionTextureCoordinate,-700,0);inputs=[('UV',uv,'')]
 if scalar:
  q=M.create_material_expression(a,unreal.MaterialExpressionScalarParameter,-700,200);q.set_editor_property('parameter_name','RowIndex');inputs.append(('RowIndex',q,''))
 mapped=h.custom(a,uvcode,unreal.CustomMaterialOutputType.CMOT_FLOAT2,inputs)
 sample=r.sample(a,texture);M.connect_material_expressions(mapped,'',sample,'UVs')
 color=h.custom(a,COOL,unreal.CustomMaterialOutputType.CMOT_FLOAT3,[('C',sample,'RGB')])
 if pressed:color=h.custom(a,'return C*.90;',unreal.CustomMaterialOutputType.CMOT_FLOAT3,[('C',color,'')])
 mask=h.custom(a,maskcode,unreal.CustomMaterialOutputType.CMOT_FLOAT1,inputs+[('A',sample,'A')] if alpha else inputs)
 M.connect_material_property(color,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR);M.connect_material_property(mask,'',unreal.MaterialProperty.MP_OPACITY);h.finishmat(a);return a

def apply():
 assert not h.W.is_pie_running()
 e.apply()
 textures={}
 for name in ['Idle','Hover','Desk']:
  n='T_Hub_Polish'+name;p=h.ART+'/'+n
  if not h.E.does_asset_exist(p):
   imported,error=unreal.AssetDiscoveryService.import_asset(str(ROOT/'ArtSource/UI/HubSidePanels'/(n+'.png')),h.ART,n);assert imported,error
  t=unreal.load_asset(p)
  for k,v in [('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON),('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI),('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS),('srgb',True),('compression_no_alpha',False),('never_stream',True)]:t.set_editor_property(k,v)
  assert h.E.save_loaded_asset(t);textures[name]=t
 bp,tree=h.tree(h.MAIN)
 left=surface('M_Hub_PolishLeft',textures['Idle'],'return UV*float2(568,899)/float2(1672,941);','float2 p=UV*float2(568,899);'+r.polygon(LEFT))
 h.obj(tree,'HubLeftBackplate').set_brush_from_material(left)
 right=[(1019,191),(1035,177),(1600,145),(1608,161),(1622,168),(1627,212),(1660,213),(1674,267),(1674,435),(1565,450),(1041,405),(1019,379)]
 profile=surface('M_Hub_PolishProfile',textures['Idle'],'return UV;','float2 p=UV*float2(1672,941);'+r.polygon(right))
 h.obj(tree,'HubBackplate').set_brush_from_material(profile)
 h.rect(h.obj(tree,'HubCloseButton'),1570,24,40,40)
 desk=surface('M_Hub_PolishDesk',textures['Desk'],'return UV;','return saturate((A-.03)/.97);',alpha=True)
 o=h.obj(tree,'HubDeskDecor');o.set_brush_from_material(desk);h.rect(o,400,225,1272,716)
 rowuv='return (float2(47,144+RowIndex*68.5)+UV*float2(385,68.5))/float2(1672,941);'
 rowmask=row_masks.ENTRY
 hover=surface('M_Hub_PolishHover',textures['Hover'],rowuv,rowmask,scalar=True)
 press=surface('M_Hub_PolishPressed',textures['Hover'],rowuv,rowmask,scalar=True,pressed=True)
 bp,t=h.tree(h.TILE);button=h.obj(t,'Btn_Common');st=button.get_editor_property('widget_style');st.hovered=h.brush(hover);st.pressed=h.brush(press);button.set_style(st)
 bp,t=h.tree(h.SYSTEM);button=h.obj(t,'Btn_Common');st=button.get_editor_property('widget_style')
 for key in ['hovered','pressed']:
  brush=getattr(st,key);brush.tint_color=h.slate((.48,.47,.40),.18 if key=='hovered' else .30)
  outline=brush.outline_settings;outline.color=h.slate((.60,.58,.50),.80);brush.outline_settings=outline;setattr(st,key,brush)
 button.set_style(st)
 result={}
 for p in [h.TILE,h.SYSTEM,h.MAIN]:
  c=h.B.compile_blueprint(p);assert c.success and c.num_errors==0,c.export_text();assert h.E.save_asset(p,False);result[p]={'errors':c.num_errors,'warnings':c.num_warnings}
 (OUT/'applied.json').write_text(json.dumps(result,indent=2))
if __name__=='__main__':apply()
