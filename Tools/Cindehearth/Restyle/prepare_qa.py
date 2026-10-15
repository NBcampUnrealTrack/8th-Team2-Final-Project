from pathlib import Path
import json
from PIL import Image
r=Path(__file__).resolve().parent
src=(r.parent/'qa_runtime.py').read_text(encoding='utf-8')
src=src.replace("Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()))/'CindehearthAutomation'","Path(unreal.Paths.project_dir())/'Tools/Cindehearth/Restyle/verification'")
(r/'qa_runtime.py').write_text(src,encoding='utf-8')
src=(r.parent/'Pipeline/walk_repaired.py').read_text(encoding='utf-8').replace('Tools/Cindehearth/Pipeline/verification/repair_walk.json','Tools/Cindehearth/Restyle/verification/gorm_walk.json')
(r/'walk_gorm.py').write_text(src,encoding='utf-8')
for p in (r.parents[2]/'Saved/Revvy/Screenshots').glob('CH_Restyle_*.png'):
    # Correct the ScreenshotService BGRA byte order; no art retouching.
    im=Image.open(p).convert('RGB')
    Image.merge('RGB',tuple(reversed(im.split()))).save(r/'verification'/p.name)
d=json.loads((r/'verification/level_before.json').read_text(encoding='utf-8'))
print('OLD COLLISION',[(a['label'],a.get('collision')) for a in d if any(k in a['label'] for k in ['RidgeMass_50','RidgeMass_48','NorthCliff_20'])])
