import json
from blender_mcp_client import Client,EVIDENCE
c=Client()
try:
    c.call('blender_mesh_inspect',{'object_name':'LG_01_Cottage_A'},'unit_probe_before')
    print(c.call('blender_model_export',{'object_names':['LG_01_Cottage_A'],'path':'Libregrove_20260908/source/Cottage_UNIT_PROBE.glb','overwrite':False},'unit_probe_export'))
finally:c.close()
