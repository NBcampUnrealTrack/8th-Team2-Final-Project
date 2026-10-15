"""Restore the previous popup's screen proportion with one uniform scale."""
import unreal,shutil
from pathlib import Path
from datetime import datetime
ROOT=Path(unreal.Paths.project_dir()).resolve()
PATH='/Game/Developers/dbals/UI/WBP_ConfirmDialog'
assert not unreal.WidgetService.is_pie_running()
bp=unreal.load_asset(PATH);assert bp
out=ROOT/'Saved/UIPopupReference'/('resize_'+datetime.now().strftime('%Y%m%d_%H%M%S'));out.mkdir(parents=True)
shutil.copy2(ROOT/'Content'/(PATH[6:]+'.uasset'),out/'WBP_ConfirmDialog.uasset')
tree=unreal.find_object(bp,'WidgetTree');panel=unreal.find_object(tree,'PanelBG');assert panel
bp.modify();panel.modify()
panel.set_render_transform_pivot(unreal.Vector2D(.5,.5))
panel.set_render_scale(unreal.Vector2D(.75,.75))
result=unreal.BlueprintService.compile_blueprint(PATH);assert result.success and not result.num_errors,result.export_text()
assert unreal.EditorAssetLibrary.save_asset(PATH,False)
scale=panel.get_editor_property('render_transform').scale
assert abs(scale.x-.75)<.0001 and abs(scale.y-.75)<.0001
print('RESIZED',out,scale,result.export_text())
