import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Restyle'
dest='/Game/Cindehearth/HD2D_v002/Textures'
journal=root/'verification/atlas_import.json'
assert not journal.exists()
assert not unreal.EditorAssetLibrary.does_asset_exist(dest+'/EnvironmentAtlas_v001')
holder={}
def on_tick(dt):
    unreal.unregister_slate_post_tick_callback(holder['handle'])
    try:
        t=unreal.AssetImportTask();t.filename=str(root/'reference/EnvironmentAtlas_v001.png');t.destination_path=dest
        t.automated=True;t.replace_existing=False;t.save=True
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
        assert t.imported_object_paths
        tex=unreal.load_asset(t.imported_object_paths[0]);assert tex
        tex.set_editor_property('filter',unreal.TextureFilter.TF_NEAREST)
        tex.set_editor_property('srgb',True)
        tex.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        assert unreal.EditorAssetLibrary.save_loaded_asset(tex)
        journal.write_text(json.dumps(dict(status='finished',assets=list(t.imported_object_paths)),indent=2),encoding='utf-8')
    except Exception as e:
        journal.write_text(json.dumps(dict(status='failed',error=str(e))),encoding='utf-8')
journal.write_text(json.dumps(dict(status='queued')),encoding='utf-8')
holder['handle']=unreal.register_slate_post_tick_callback(on_tick)
print('ATLAS IMPORT QUEUED')
