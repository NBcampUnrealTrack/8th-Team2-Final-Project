import unreal,json,sys,importlib
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Polish_v003';sys.path.insert(0,str(root))
import ue_common as C
importlib.reload(C)
assert not unreal.WidgetService.is_pie_running();assert not (root/'verification/tunnel_sightlines.json').exists()
rows=[]
with unreal.ScopedEditorTransaction('Cindehearth clear dungeon player sightline'):
 for i in range(7):
  prefix='CH_MineTimber_'+str(i)+'_P3'
  for label in [prefix,prefix+'_InnerLintel']:
   C.hide(C.ACTORS[label],True);rows.append(label)
  # The two solid side jambs remain, while the main mine and bear-den arches remain intact.
C.save_result('tunnel_sightlines',rows);print('INTERNAL OVERHEAD FRAMES HIDDEN',len(rows))
