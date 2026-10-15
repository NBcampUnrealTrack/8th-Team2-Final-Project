"""Copy existing meaningful route/state checks to this pass, preserving earlier receipts."""
from pathlib import Path
root=Path(__file__).resolve().parent
for name in ['qa_runtime.py','qa_camera.py','walk_gorm.py']:
 src=(root.parent/'Restyle'/name).read_text(encoding='utf-8')
 (root/name).write_text(src.replace('Tools/Cindehearth/Restyle/verification','Tools/Cindehearth/Polish_v003/verification'),encoding='utf-8')
