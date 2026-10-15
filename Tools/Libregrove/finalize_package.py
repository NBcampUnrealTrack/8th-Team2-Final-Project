import hashlib,json,shutil,zipfile
from pathlib import Path
from blender_mcp_client import ROOT,EVIDENCE

DOC=ROOT/'Docs/Libregrove/BlenderQA_20260908'
BASE=Path('C:/Users/Ndus_072/AppData/Local/Revvy/BlenderExchange/Cindehearth/Libregrove_20260908')
verification=[]
for path in sorted(EVIDENCE.glob('review_handoff_*.json')):
    h=json.loads(path.read_text(encoding='utf-8'));q=json.loads((EVIDENCE/('export_qa_'+h['id']+'_REVIEW16.json')).read_text(encoding='utf-8'))
    w,d,z=h['measured_width_depth_height_cm'];expected=[d/100,z/100,w/100];actual=q['export']['dimensions_m_xyz_y_up']
    error=max(abs(a-b) for a,b in zip(expected,actual));assert error<1e-5
    uv_status='MISSING' if q['positive_overlap_pair_count']<0 else ('OVERLAPS_REQUIRE_REPAIR' if q['positive_overlap_pair_count']>0 else 'NO_POSITIVE_OVERLAP_DETECTED_NOT_FINAL_SIGNOFF')
    verification.append({'asset':h['id'],'glb_unit_dimensions_match':True,'max_error_m':error,'uv_status':uv_status})
assert len(verification)==9
views=list((BASE/'review_views').glob('*.png'));assert len(views)==18,len(views)
target=DOC/'review_views';target.mkdir(exist_ok=True)
for src in views:
    dest=target/src.name
    if dest.exists():assert hashlib.sha256(dest.read_bytes()).digest()==hashlib.sha256(src.read_bytes()).digest()
    else:shutil.copy2(src,dest)
workspace=DOC/'Libregrove_검수작업본.blend';srcblend=BASE/'Libregrove_ReviewWorkspace18.blend'
assert hashlib.sha256(workspace.read_bytes()).digest()==hashlib.sha256(srcblend.read_bytes()).digest()
original=Path('D:/Users/Ndus_072/Desktop/mAP.blend')
state={'status':'PARTIAL_COMPLETE_REQUIRES_TYPED_TOPOLOGY_AND_UV_EDITING','original_file':str(original),'original_file_bytes':original.stat().st_size,'original_file_mtime_unix':original.stat().st_mtime,'original_file_sha256':hashlib.sha256(original.read_bytes()).hexdigest(),'original_object_fingerprints_unchanged':9,'dimension_assertions_passed':36,'glb_unit_checks':verification,'view_count':len(views),'workspace_sha256':hashlib.sha256(workspace.read_bytes()).hexdigest(),'uv_generated_count':8,'uv_failed_assets':['LG_04_Workshop'],'uv_overlap_assets':['LG_01_Cottage_A','LG_02_Inn','LG_03_VillageHall','LG_05_WatermillHouse','LG_07_Sanctuary'],'no_unreal_import_or_performance_signoff':True}
(DOC/'handoff_status.json').write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8')
zip_path=ROOT/'Docs/Libregrove_Blender_QA_PARTIAL_20260908.zip';assert not zip_path.exists()
include=[DOC/'검수보고서.md',DOC/'시각검수.md',DOC/'모델별_검수표.csv',DOC/'review_manifest.json',DOC/'handoff_status.json',workspace]+list((DOC/'review_models').glob('*'))+list(target.glob('*.png'))+list((DOC/'uv_layouts').glob('*_REVIEW16.svg'))
with zipfile.ZipFile(zip_path,'x',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as package:
    for path in include:package.write(path,'Libregrove_Blender_QA_PARTIAL/'+str(path.relative_to(DOC)).replace('\\','/'))
result={'path':str(zip_path),'files':len(include),'bytes':zip_path.stat().st_size,'sha256':hashlib.sha256(zip_path.read_bytes()).hexdigest()}
(DOC/'package_receipt.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps(result,ensure_ascii=False))
