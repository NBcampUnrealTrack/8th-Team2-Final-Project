"""Key-panel outer artwork: smooth round frame corners with pointed stars retained."""
import unreal, importlib.util, json, shutil
from pathlib import Path
from datetime import datetime
ROOT=Path(unreal.Paths.project_dir()).resolve()
s=importlib.util.spec_from_file_location('key',ROOT/'Tools/UISettingsReference/apply_key_polish.py')
k=importlib.util.module_from_spec(s);s.loader.exec_module(k)
p=k.p;E=p.E;M=p.M

def apply():
 assert not p.W.is_pie_running()
 path=k.BASE+'WBP_KeyBindingsWindow'
 out=ROOT/'Saved/UIKeySettingsPolish'/('outer_'+datetime.now().strftime('%Y%m%d_%H%M%S'));out.mkdir()
 shutil.copy2(ROOT/'Content'/(path[6:]+'.uasset'),out/'WBP_KeyBindingsWindow.uasset')
 texpath=p.PRIVATE+'/T_KeySettings_FrameRounded'
 if not E.does_asset_exist(texpath):
  ok,error=unreal.AssetDiscoveryService.import_asset(str(ROOT/'ArtSource/UI/SettingsReference/T_KeySettings_FrameRounded.png'),p.PRIVATE,'T_KeySettings_FrameRounded');assert ok,error
 tex=unreal.load_asset(texpath)
 for key,value in [('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON),('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI),('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS),('srgb',True),('compression_no_alpha',False),('never_stream',True)]:tex.set_editor_property(key,value)
 assert E.save_loaded_asset(tex)
 matpath=p.PRIVATE+'/M_KeySettingsRoundedFrame'
 if not E.does_asset_exist(matpath):assert E.duplicate_asset(p.PRIVATE+'/M_OptionsPolishedFrame',matpath)
 graph=json.loads(unreal.MaterialNodeService.export_material_graph(matpath))
 node=next(n for n in graph['expressions'] if n['class']=='TextureSample')
 assert unreal.MaterialNodeService.set_expression_property(matpath,node['id'],'Texture',tex.get_path_name())
 mat=unreal.load_asset(matpath);assert not M.recompile_material(mat);assert E.save_loaded_asset(mat)
 sk=k.s.Skin('WBP_KeyBindingsWindow');sk.get('WindowBG').set_editor_property('background',k.q.brush(mat))
 c=p.B.compile_blueprint(path);assert c.success and not c.num_errors,c.export_text();assert E.save_asset(path,False)
 (out/'compile.txt').write_text(c.export_text());print(c.export_text())

if __name__=='__main__':apply()
