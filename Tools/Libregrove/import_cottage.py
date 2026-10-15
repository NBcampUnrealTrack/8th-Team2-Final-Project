import json
from blender_mcp_client import Client,EVIDENCE
c=Client()
try:
    h=json.loads((EVIDENCE/'repair_handoff_LG_01_Cottage_A.json').read_text(encoding='utf-8'))
    c.call('blender_scene_inspect',{},'pre_import_LG_01_Cottage_A')
    print(json.dumps(c.call('blender_model_import',{'path':h['export_path'],'name_prefix':'LGQA01_'},'import_LG_01_Cottage_A'),indent=2))
    c.call('blender_scene_inspect',{},'post_import_LG_01_Cottage_A')
finally:c.close()
