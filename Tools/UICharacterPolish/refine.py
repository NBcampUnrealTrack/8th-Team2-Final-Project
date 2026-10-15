"""Final component pass: shallow independent shadows and empty gear silhouettes."""
import unreal,importlib.util,json
from pathlib import Path
root=Path(unreal.Paths.project_dir()).resolve();spec=importlib.util.spec_from_file_location('polish_support',root/'Tools/UICharacterPolish/apply.py')
# Load helpers without re-running the authoring pass.
src=(root/'Tools/UICharacterPolish/apply.py').read_text(encoding='utf-8-sig');ns={};exec(src.rsplit('apply()',1)[0],ns)
material=ns['material'];image_brush=ns['image_brush'];color=ns['color'];alpha=ns['alpha'];rect=ns['rect'];E=unreal.EditorAssetLibrary
assert not unreal.WidgetService.is_pie_running()
folder=ns['FOLDER'];gold=ns['GOLD'];navy=ns['NAVY'];cream=ns['CREAM']
# All silhouettes are empty-state indicators, never inventory-item artwork.
shapes={
 'Head':'float a=step(length((p-float2(.5,.47))/float2(.33,.36)),1);a*=1-step(.39,p.y)*step(p.y,.62)*step(.25,p.x)*step(p.x,.75);',
 'Chest':'float a=max(step(.26,p.x)*step(p.x,.74)*step(.26,p.y)*step(p.y,.82),step(.12,p.x)*step(p.x,.88)*step(.18,p.y)*step(p.y,.38));a*=1-step(.39,p.x)*step(p.x,.61)*step(p.y,.3);',
 'Gloves':'float a=step(.23,p.x)*step(p.x,.68)*step(.20,p.y)*step(p.y,.80);a=max(a,step(.58,p.x)*step(p.x,.83)*step(.48,p.y)*step(p.y,.69));',
 'Boots':'float a=step(.29,p.x)*step(p.x,.58)*step(.16,p.y)*step(p.y,.82);a=max(a,step(.29,p.x)*step(p.x,.82)*step(.62,p.y)*step(p.y,.82));',
 'Legs':'float a=step(.24,p.x)*step(p.x,.76)*step(.18,p.y)*step(p.y,.82);a*=1-step(.43,p.x)*step(p.x,.57)*step(.45,p.y);',
 'Weapon':'float a=step(abs(p.x-.5),.07)*step(.12,p.y)*step(p.y,.83);a=max(a,step(abs(p.x-.5),.29)*step(abs(p.y-.66),.045));',
 'Earring':'float d=length(p-float2(.5,.53));float a=step(abs(d-.24),.055);a=max(a,step(length(p-float2(.5,.22)),.07));',
 'Ring1':'float d=length(p-float2(.5,.56));float a=step(abs(d-.22),.055);a=max(a,step(abs(p.x-.5)+abs(p.y-.29),.13));',
 'Ring2':'float d=length(p-float2(.5,.56));float a=step(abs(d-.22),.055);a=max(a,step(abs(p.x-.5)+abs(p.y-.29),.13));'
}
mats={k:material('M_EmptyGear_'+k,'float2 p=UV;'+v+'return float4(1,1,1,a);') for k,v in shapes.items()}
for path in unreal.WidgetService.list_widget_blueprints(folder):
 path=str(path).split('.')[0];bp=unreal.load_asset(path);bp.modify();tree=unreal.find_object(bp,'WidgetTree');snaps=unreal.WidgetService.get_widget_snapshot(path)
 for s in snaps:
  n=s.widget_name;w=unreal.find_object(tree,n)
  if n.endswith('Surface') and n not in ['IvorySurface','NavySurface','MenuTitleSurface','MirrorFloorSurface','NaiaPortraitFrameSurface'] and isinstance(w.slot,unreal.CanvasPanelSlot):
   shadowname='PolishShadow_'+n;shadow=unreal.find_object(tree,shadowname)
   if not shadow:
    r=unreal.WidgetService.add_component(path,'Image',shadowname,s.parent_widget,True);assert r.success,r.export_text();shadow=unreal.find_object(tree,shadowname)
   shadow.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
   shadow.slot.set_layout(w.slot.get_layout());p=shadow.slot.get_position();shadow.slot.set_position(unreal.Vector2D(p.x+2,p.y+3));shadow.slot.set_z_order(w.slot.get_z_order()-1)
   br=unreal.SlateBrush();br.draw_as=unreal.SlateBrushDrawType.ROUNDED_BOX;br.tint_color=unreal.SlateColor(specified_color=alpha(navy,.075));outline=br.outline_settings.copy();outline.corner_radii=unreal.Vector4(7,7,7,7);br.outline_settings=outline;shadow.set_brush(br)
 if path.endswith('WBP_CharacterEquipmentPage'):
  for key,mat in mats.items():
   w=unreal.find_object(tree,'Equip'+key+'EmptyMark');assert w,key
   image_brush(w,alpha(cream,.3),mat);w.set_render_transform_angle(0);rect(w,28,14,32,32)
 if path.endswith('WBP_CharacterNaiaPage'):
  # The page already explains the actual first-slot battle participation rule.
  w=unreal.find_object(tree,'FormationHelp');w.set_color_and_opacity(unreal.SlateColor(specified_color=navy))
 result=unreal.BlueprintService.compile_blueprint(path);assert result.success,result.export_text();assert E.save_asset(path,False)
print('SHADOWS_AND_GEAR_SILHOUETTES_SAVED')
