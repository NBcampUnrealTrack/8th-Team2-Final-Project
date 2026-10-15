"""Palette-only rollout to shared confirm/count/reward popups; private resources."""
import unreal,importlib.util,json,shutil,hashlib
from pathlib import Path
from datetime import datetime
ROOT=Path(unreal.Paths.project_dir()).resolve()
s=importlib.util.spec_from_file_location('popup_key',ROOT/'Tools/UISettingsReference/apply_key_polish.py');k=importlib.util.module_from_spec(s);s.loader.exec_module(k);p=k.p;M=p.M;E=p.E;B=p.B
PRIVATE='/Game/Developers/MinkiCho/Art/UITheme/CommonPopupPalette'
# Confirmation now has its own final reference authoring pass; preserve it.
PATHS=['/Game/Developers/dbals/UI/WBP_CountDialog','/Game/Developers/zxcv1/Quest/UI/WBP_RewardPopup']

def private_mi(source,role):
 dst=source.get_path_name().split('.')[0] if source.get_path_name().startswith(PRIVATE+'/') else PRIVATE+'/'+source.get_name()+'_'+role
 if not E.does_asset_exist(dst):assert E.duplicate_asset(source.get_path_name(),dst)
 mat=unreal.load_asset(dst)
 for name in M.get_vector_parameter_names(mat):
  if str(name)=='SurfaceColor':M.set_material_instance_vector_parameter_value(mat,name,p.col(role))
  elif str(name)=='LineColor':M.set_material_instance_vector_parameter_value(mat,name,p.col('ink' if role=='gold' else 'gold'))
 assert E.save_loaded_asset(mat);return mat

def style(button,role):
 st=button.get_editor_property('widget_style').copy()
 for state in ['normal','hovered','pressed','disabled']:
  br=getattr(st,state).copy();resource=br.resource_object
  if isinstance(resource,unreal.MaterialInstanceConstant):br.resource_object=private_mi(resource,role)
  setattr(st,state,br)
 button.set_style(st)

def texture_material(tex):
 dst=PRIVATE+'/M_CountPanelPalette'
 mat=unreal.load_asset(dst) if E.does_asset_exist(dst) else unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_CountPanelPalette',PRIVATE,unreal.Material,unreal.MaterialFactoryNew())
 M.delete_all_material_expressions(mat);mat.set_editor_property('material_domain',unreal.MaterialDomain.MD_UI);mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
 sm=M.create_material_expression(mat,unreal.MaterialExpressionTextureSample,-500,0);sm.set_editor_property('texture',tex)
 n=M.create_material_expression(mat,unreal.MaterialExpressionCustom,0,0);ci=unreal.CustomInput();ci.set_editor_property('input_name','C');n.set_editor_property('inputs',[ci]);n.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
 navy=','.join(map(str,p.rgb('navy')));gold=','.join(map(str,p.rgb('gold')))
 n.set_editor_property('code',f'float l=dot(C,float3(.2126,.7152,.0722));float warm=smoothstep(.01,.16,C.r-C.b);float pale=smoothstep(.16,.5,l);return lerp(float3({navy})*clamp(sqrt(l/.035),.8,1.15),float3({gold}),max(warm,pale));')
 M.connect_material_expressions(sm,'RGB',n,'C');M.connect_material_property(n,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR);M.connect_material_property(sm,'A',unreal.MaterialProperty.MP_OPACITY);assert not M.recompile_material(mat);assert E.save_loaded_asset(mat);return mat

def apply():
 assert not p.W.is_pie_running()
 out=ROOT/'Saved/UIPopupPalette'/datetime.now().strftime('%Y%m%d_%H%M%S');out.mkdir(parents=True)
 # Protect source assets and unrelated screens against shared-resource edits.
 protected=['/Game/Developers/dbals/UI/WBP_OptionsWindow','/Game/Developers/dbals/UI/WBP_KeyBindingsWindow','/Game/Developers/MinkiCho/UI/Buttons/WBP_CommonButton','/Game/Developers/MinkiCho/Art/FieldHUD/Alerts20260930/MI_Alert_Panel']
 hashes={v:hashlib.sha256((ROOT/'Content'/(v[6:]+'.uasset')).read_bytes()).hexdigest() for v in protected}
 for path in PATHS:shutil.copy2(ROOT/'Content'/(path[6:]+'.uasset'),out/(path.split('/')[-1]+'.uasset'))
 for path in PATHS:
  bp=unreal.load_asset(path);bp.modify();t=unreal.find_object(bp,'WidgetTree');bindings=unreal.ObjectReflectionService.get_property(path,'Bindings')
  snap=p.snapshot(path);(out/(bp.get_name()+'.json')).write_text(json.dumps(snap,ensure_ascii=False,indent=2),encoding='utf-8')
  for row in snap:
   o=unreal.find_object(t,row['name']);o.modify();n=o.get_name()
   if isinstance(o,unreal.TextBlock):
    role='gold' if n in ['AlertHeading','TitleText','PriceText'] else 'muted' if n in ['CountLabel','RewardCaption'] else 'ink' if n=='CloseButtonText' else 'text'
    o.set_color_and_opacity(p.slate(role))
   if isinstance(o,unreal.Image):
    if 'Rule' in n:
     br=o.get_editor_property('brush').copy();alpha=br.tint_color.specified_color.a;br.tint_color=p.slate('gold',alpha);o.set_brush(br)
    elif n=='AlertSeal':o.set_color_and_opacity(unreal.LinearColor(*[new/old for new,old in zip(p.rgb('gold'),(.67,.55,.34))],1))
   if isinstance(o,unreal.Border):
    if n=='PanelBG':
     br=o.get_editor_property('background').copy();res=br.resource_object;br.resource_object=private_mi(res,'navy') if isinstance(res,unreal.MaterialInstanceConstant) else texture_material(res) if isinstance(res,unreal.Texture2D) else res;o.set_brush(br);o.set_brush_color(unreal.LinearColor(1,1,1,1))
    elif n=='CountFrame':o.set_brush_color(p.col('surface'))
   if isinstance(o,unreal.Button) and n!='ConfirmButton':style(o,'gold' if n=='CloseButton' else 'surface')
  if path.endswith('WBP_CountDialog'):
   sk=k.s.Skin('WBP_CountDialog')
   for name,kind in [('ConfirmButton','Primary'),('CancelButton','Secondary')]:
    src=p.PRIVATE+'/WBP_SettingsFooter'+kind;dst=PRIVATE+'/WBP_PopupPalette'+kind
    if not E.does_asset_exist(dst):assert E.duplicate_asset(src,dst)
    o=k.replace(sk,name,'WBP_PopupPalette'+kind);o.set_editor_property('label_color',p.col('ink' if kind=='Primary' else 'text'))
   box=sk.get('CountTextBox');st=box.get_editor_property('widget_style').copy();st.foreground_color=p.slate('text');st.focused_foreground_color=p.slate('text');st.background_color=p.slate('surface');box.set_editor_property('widget_style',st)
  result=B.compile_blueprint(path);assert result.success and not result.num_errors;assert E.save_asset(path,False)
  assert unreal.ObjectReflectionService.get_property(path,'Bindings')==bindings
  print(path,result.export_text())
 for v,expected in hashes.items():assert hashlib.sha256((ROOT/'Content'/(v[6:]+'.uasset')).read_bytes()).hexdigest()==expected,v
 (out/'verified.json').write_text(json.dumps({'palette':p.HEX,'changed':PATHS,'protected_source_hashes':hashes},indent=2))
if __name__=='__main__':apply()


