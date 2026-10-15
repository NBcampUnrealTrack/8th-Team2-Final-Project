"""Exact source-preserving 2mm awning inset, typed mesh-edit copy only."""
import json,shutil
from pathlib import Path
from buildings_pipeline import transport,DEST,EXCHANGE,LIVE_EXCHANGE,unwrap,newjson,common
def main():
    plan=json.loads((DEST/'plans/02_Inn.json').read_text(encoding='utf-8'));source='LGV6_02_Inn_DoorAwning';output=source+'_Inset002'
    client=transport.Client('main',poll_interval=.25,quiet=True)
    try:
        scene=unwrap(client.call('blender_scene_inspect',{},'inn_inset_scene_before'));assert output not in {o['name'] for o in scene['objects']}
        before=unwrap(client.call('blender_mesh_inspect',{'object_name':source},'inn_inset_before'))
        location=list(before['object']['location']);location[0]-=.002
        client.call('blender_mesh_edit',{'source_object_name':source,'output_object_name':output,'create_copy':True,'operations':[{'type':'transform_object','location':location}]},'inn_inset_copy')
        after=unwrap(client.call('blender_mesh_inspect',{'object_name':output},'inn_inset_after'))
        check=unwrap(client.call('blender_mesh_inspect',{'object_name':source},'inn_inset_source_check'));assert check['fingerprint']==before['fingerprint']
        path=LIVE_EXCHANGE/'02_Inn_Parts_r2.blend';client.call('blender_scene_inspect',{},'inn_inset_before_save');client.call('blender_save_copy',{'path':str(path),'overwrite':False},'inn_inset_save')
        target=EXCHANGE/'source/02_Inn_Parts_r2.blend';assert not target.exists();shutil.copy2(path,target)
        plan['assembly'].update(**common(target),output_blend=str(EXCHANGE/'02_Inn/revision02/01_Assembly.blend'))
        for ref in plan['assembly']['static_parts']:
            if ref['object_name']==source:ref.update(object_name=output,expected_fingerprint=after['fingerprint'])
        plan['refinement']={'source':source,'copy':output,'offset_m':[-.002,0,0],'reason':'Original canopy exceeded target depth by1.073mm; inset2mm without altering source.'}
        newjson(DEST/'plans/02_Inn_r2.json',plan)
    finally:client.close()
if __name__=='__main__':main()
