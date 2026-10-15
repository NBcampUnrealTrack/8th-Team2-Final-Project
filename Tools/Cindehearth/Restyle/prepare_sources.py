"""Preserve generation outputs and prepare explicit P2 request receipts."""
from pathlib import Path
import json,shutil

root=Path(__file__).resolve().parent
refs=root/'reference';refs.mkdir(exist_ok=True)
gen=Path.home()/'AppData/Roaming/reddy-ide/codex-runtime-home/home/generated_images/01a06f2d-ce93-7d03-83b0-1de60ab74e0f'
sources={'EnvironmentAtlas':'exec-70f38588-4913-40f3-ae87-644d117d287b.png','Pine':'exec-b41acae6-fc62-4a48-b834-51fde01f1249.png'}
for name,file in sources.items():
    dest=refs/(name+'_v001.png')
    assert not dest.exists()
    shutil.copy2(gen/file,dest)
args=dict(asset_name='CH_Pine_P2_Restyle_v001',request_id='CH_Pine_P2_Restyle_20260905_v001',image_path=str(refs/'Pine_v001.png'),face_limit=1600)
(root/'pine_request.json').write_text(json.dumps(args,indent=2),encoding='utf-8')
