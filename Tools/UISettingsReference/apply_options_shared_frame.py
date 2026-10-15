"""Share the approved ornamental frame; put Apply before Cancel in both windows."""
import unreal,importlib.util,shutil,json
from pathlib import Path
from datetime import datetime
ROOT=Path(unreal.Paths.project_dir()).resolve()
s=importlib.util.spec_from_file_location('key',ROOT/'Tools/UISettingsReference/apply_key_polish.py')
k=importlib.util.module_from_spec(s);s.loader.exec_module(k)

def apply():
 assert not k.W.is_pie_running()
 out=ROOT/'Saved/UIKeySettingsPolish'/('shared_'+datetime.now().strftime('%Y%m%d_%H%M%S'));out.mkdir()
 paths=[k.p.PATH,k.BASE+'WBP_KeyBindingsWindow']
 for path in paths:
  src=ROOT/'Content'/(path[6:]+'.uasset');shutil.copy2(src,out/src.name)
 mat=unreal.load_asset(k.p.PRIVATE+'/M_KeySettingsGeometricFrame');assert mat
 sk=k.s.Skin('WBP_OptionsWindow')
 sk.get('WindowFrame').set_editor_property('background',k.q.brush(mat))
 # Preserve each button's width and the original 14px gap and group bounds.
 sk.get('ApplyButton').slot.set_position(unreal.Vector2D(686,560))
 sk.get('CloseButton').slot.set_position(unreal.Vector2D(848,560))
 sk=k.s.Skin('WBP_KeyBindingsWindow')
 # Preserve 150/120px widths, 16px gap, and right edge at 1152.
 sk.get('ApplyButtonBox').slot.set_position(unreal.Vector2D(866,711))
 sk.get('FooterCloseButton').slot.set_position(unreal.Vector2D(1032,711))
 report={}
 for path in paths:
  c=k.B.compile_blueprint(path);assert c.success and not c.num_errors,c.export_text()
  assert k.E.save_asset(path,False);report[path]=c.export_text()
 (out/'compile.json').write_text(json.dumps(report,indent=2));print(report)
if __name__=='__main__':apply()
