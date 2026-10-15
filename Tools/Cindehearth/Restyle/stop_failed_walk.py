import unreal,json
from pathlib import Path
if 'ch_walk_test' in globals():
    unreal.unregister_slate_post_tick_callback(ch_walk_test.handle)
    (Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Restyle/verification/walk_cemetery_failed.json').write_text(json.dumps(dict(passed=False,reason='pawn destroyed during traversal',samples=ch_walk_test.samples),indent=2),encoding='utf-8')
print(unreal.WidgetService.stop_pie())
