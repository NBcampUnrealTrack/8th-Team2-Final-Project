from blender_mcp_client import Client
c=Client()
try:
    c.call('blender_mesh_inspect',{'object_name':'LG_08_BroadleafTree'},'tree_balanced_before')
    c.call('blender_modifier_apply',{'source_object_name':'LG_08_BroadleafTree','output_object_name':'LG_08_BroadleafTree_Balanced03','modifier':{'type':'decimate','ratio':.4}},'tree_balanced_reduce')
    print(c.call('blender_mesh_inspect',{'object_name':'LG_08_BroadleafTree_Balanced03'},'tree_balanced_after'))
    c.call('blender_save_copy',{'path':'Cindehearth/Libregrove_20260908/stages/Libregrove_Selected03.blend','overwrite':False},'008_selected_stage_save')
finally:c.close()
