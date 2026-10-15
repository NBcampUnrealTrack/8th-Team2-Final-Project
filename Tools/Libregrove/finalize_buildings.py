"""Typed-only corrected double-door GLB export and original scene preservation audit."""
import json
from pathlib import Path
import buildings_pipeline as p

def fix_double_glb():
    client=p.transport.Client('buildings',quiet=True)
    try:
        for ident in ['03_VillageHall','04_Workshop']:
            v=json.loads((p.DEST/f'verification/{ident}.json').read_text(encoding='utf-8'))
            source=Path(v['final_blend']);assert p.sha(source)==v['final_sha256']
            dest=p.EXCHANGE/ident/f'export_revision02/{ident}_assembly.glb'
            export_job=f'B6_{ident}_Export_assembly_glb_combined02'
            exported=p.unwrap(client.call('blender_building_export',{'job_id':export_job,**p.common(source),'selection':'assembly','output_path':str(dest)},export_job))
            job=f'B6_{ident}_Reimport_assembly_glb_combined02'
            checked=p.unwrap(client.call('blender_building_export_inspect',{'job_id':job,'export_path':str(dest),'expected_sha256':exported['sha256']},job))
            assert all(x['uv']['passed'] for x in checked['meshes'])
            p.newjson(p.DEST/f'verification/{ident}_export_override.json',{'selection':'assembly','format':'glb','path':str(dest),'job_id':job,'sha256':exported['sha256'],'reason':'Combined scene animation for both door leaves; prior split clips preserved as rejected candidates.'})
            print(ident+' COMBINED_GLB_READY',flush=True)
    finally:client.close()

def preserve_scene():
    client=p.transport.Client('main',quiet=True)
    try:
        status=p.unwrap(client.call('blender_bridge_status',{},'handoff_final_status'))
        assert status['blender']['runtime_supported'] and status['mcp']['bridge_compatibility']['compatible']
        scene=p.unwrap(client.call('blender_scene_inspect',{'include_hidden':True,'max_objects':2000},'handoff_final_scene'))
        assert not scene['truncated']
        source=json.loads((p.DEST/'source_baseline.json').read_text(encoding='utf-8'))
        current={o['name']:o.get('fingerprint') for o in scene['objects']}
        for ob in source['scene']['objects']:assert current[ob['name']]==ob.get('fingerprint')
        assert p.sha(Path(source['file']))==source['file_sha256']
        p.newjson(p.DEST/'verification/source_preservation_final.json',{'passed':True,'baseline_objects_preserved':len(source['scene']['objects']),'current_objects':len(scene['objects']),'original_blend_sha256':source['file_sha256'],'original_blend_not_saved_over':True})
        print('ORIGINAL_SCENE_AND_DISK_FILE_PRESERVED',flush=True)
    finally:client.close()

if __name__=='__main__':
    import sys
    fix_double_glb() if sys.argv[1]=='double' else preserve_scene()
