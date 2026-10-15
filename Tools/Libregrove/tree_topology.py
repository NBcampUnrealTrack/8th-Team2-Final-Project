import os
from blender_mcp_client import Client
b=os.path.expandvars(r'%LOCALAPPDATA%/Revvy/BlenderExchange/Cindehearth/Libregrove_20260908')
c=Client('repair')
try:
    c.call('blender_topology_inspect',{'job_id':'LG_selected_tree_topology','blend_path':b+'/stages/Libregrove_Selected03.blend','object_name':'LG_08_BroadleafTree_Balanced03'},'topology_LG_08_BroadleafTree_Balanced03')
finally:c.close()
