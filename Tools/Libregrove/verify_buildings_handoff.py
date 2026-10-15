"""Read-only final delivery cross-check; append one new signoff report."""
import json
import package_buildings as p

def main():
    manifest=p.data(p.DEST/'manifest.json');models=p.DEST/'Models'
    assert manifest['total_assets']==6 and manifest['total_mesh_vertices']==1422 and manifest['total_triangles']==2128
    assert manifest['total_render_vertices']==4266
    new=[p.audit(s) for s in p.CATALOG]
    for item in manifest['assets']:
        folder=models/item['id']
        assert p.sha(folder/(item['id']+'_FINAL.blend'))==item['final_blend_sha256']
        if item.get('reused_verified_V5'):
            report=p.audit_cottage();assert report['passed']
            for exp in report['exports']:assert p.sha(folder/'Export'/exp['file'])==exp['sha256']
        else:
            audit=next(x for x in new if x['id']==item['id']);assert audit==item
            for exp in item['exports']:
                name=item['id']+'_'+exp['selection']+'.'+exp['format']
                assert p.sha(folder/'Export'/name)==exp['sha256']
    counts={'blend':len(list(models.rglob('*.blend'))),'glb_fbx':len(list(models.rglob('*.glb')))+len(list(models.rglob('*.fbx'))),'uv_svg':len(list(models.rglob('*.svg'))),'preview_png':len(list(models.rglob('*.png')))}
    assert counts=={'blend':6,'glb_fbx':36,'uv_svg':13,'preview_png':18},counts
    assert p.data(p.DEST/'verification/source_preservation_final.json')['passed']
    source=p.data(p.DEST/'source_baseline.json');assert p.sha(p.Path(source['file']))==source['file_sha256']
    selftest=p.data(p.STATE/'B6_SelfTest001/result.json');assert selftest['success'] and selftest['result']['passed']
    jobs=[]
    for request in p.STATE.rglob('request.json'):
        result=request.parent/'result.json';assert result.is_file(),str(request)
        j=p.data(result);assert j['state'] in ['finished','failed'];jobs.append({'id':request.parent.name,'state':j['state'],'success':j['success']})
    report={'status':'MODEL_UV_RIGID_DOOR_EXPORT_VERIFIED','counts':counts,'new_assets_independent_audit_passed':5,'cottage_existing_export_checks_revalidated':True,'combined_double_door_glb_passed':True,'raw_glb_uv_audit_passed':True,'export_reimport_pose_tolerance':1e-5,'original_disk_and_scene_preserved':True,'pending_worker_jobs':0,'jobs':jobs,'visual_review':{'inspected':'New assets Closed/Back and every opening door Open; V5 reviewed previously, unchanged hashes','remaining_notes':['Small dark roof-ridge end contacts remain in clay previews','Plain rear and side walls are intentional for texture-first production','Assembled solid components retain internal joint faces']},'no_game_engine_or_performance_signoff':True,'delivery_file_hashes':{f.relative_to(p.DEST).as_posix():p.sha(f) for f in sorted(models.rglob('*')) if f.is_file()}}
    p.newjson(p.DEST/'final_signoff.json',report)
    print(json.dumps({k:v for k,v in report.items() if k not in ['jobs','delivery_file_hashes']},indent=2))

if __name__=='__main__':main()
