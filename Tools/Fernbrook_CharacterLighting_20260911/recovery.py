"""Back up the sole existing asset to be edited and hash protected content."""
import json,hashlib,shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
DOC=ROOT/'Docs/Fernbrook_CharacterLighting_20260911'
TARGET=ROOT/'Saved/FernbrookRecovery/CharacterLighting_Before_20260911'
MAP=ROOT/'Content/Fernbrook/Entrance_V1/Maps/L_Fernbrook_Entrance.umap'
assert not TARGET.exists()
TARGET.mkdir(parents=True)
shutil.copy2(MAP,TARGET/MAP.name)
protected=[]
for folder in ['Content/Fernbrook','Content/Libregrove','Content/Cindehearth','Content/Village/Materials','Content/Art/Sprites/Player']:
 for f in (ROOT/folder).rglob('*'):
  if f.is_file() and f.suffix in ['.umap','.uasset']:
   protected.append({'path':str(f.relative_to(ROOT)).replace('\\','/'),'sha256':hashlib.sha256(f.read_bytes()).hexdigest()})
with (DOC/'baseline_hashes.json').open('x',encoding='utf8') as f:json.dump({'map_backup':str(TARGET/MAP.name),'files':protected},f,indent=2)
print(json.dumps({'backup':str(TARGET),'files':len(protected)}))
