"""Create a namespaced orchestration copy; no Blender code execution."""
from pathlib import Path
import json, shutil
ROOT=Path(__file__).resolve().parent
PROJECT=ROOT.parents[3]
OLD=PROJECT/'Tools/Cindehearth/Polish_v003'
for d in ('reference','requests','verification','jobs'):(ROOT/d).mkdir(exist_ok=True)
for name in ('task.py','repair_asset.py','cap_plan.py','overlap_plan.py'):
    text=(OLD/name).read_text(encoding='utf-8')
    text=text.replace("PROJECT=ROOT.parents[2];PIPE=ROOT.parent/'Pipeline'", "PROJECT=ROOT.parents[3];PIPE=PROJECT/'Tools/Cindehearth/Pipeline'")
    text=text.replace("Polish_v003", "Dense_v004/Architecture").replace("CH_Polish_", "CH_Dense_").replace("polish_", "dense_arch_").replace("v003", "v004").replace("topology003", "topology004").replace("review003", "review004").replace("_003.png", "_004.png")
    (ROOT/name).write_text(text,encoding='utf-8')
(ROOT/'catalog.json').write_text(json.dumps([{'name':'Tavern','faces':1800,'use':'Complete two-storey tavern'}, {'name':'Smithy','faces':1600,'use':'Complete open-front smithy'}],indent=2),encoding='utf-8')
src=Path('C:/Users/Ndus_072/AppData/Roaming/reddy-ide/codex-runtime-home/home/generated_images/01a070c7-5aa7-7ba1-8b6c-243023070e5c')
refs={'Tavern':'exec-768c5d71-8284-4c61-913c-37fbd3fd3382.png','Smithy':'exec-4e627c84-9aa3-4bbf-ad81-8b11088ee674.png'}
for name,file in refs.items():shutil.copy2(src/file,ROOT/'reference'/(name+'.png'))
(ROOT/'reference_sources.json').write_text(json.dumps({k:str(src/v) for k,v in refs.items()},indent=2),encoding='utf-8')
