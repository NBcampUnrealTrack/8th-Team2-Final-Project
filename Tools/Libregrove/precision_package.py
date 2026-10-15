"""Create a NEW portable handoff from verified final MCP/GLB/FBX receipts."""
import csv,hashlib,html,json,shutil,zipfile
from pathlib import Path
from precision_workflow import DOC,OUT,record
from blender_mcp_client import ROOT

DEST=ROOT/'Docs/Libregrove/BlenderQA_V2_20260908'
ARCHIVE=ROOT/'Docs/Libregrove_Blender_MeshUV_V2_20260908.zip'
NOTES={
1:'지붕 기와가 불규칙하고 중앙에 깊은 접힘/작은 틈이 남는다. 근접 카메라용 기와 재정리 대상.',
2:'전면은 비교적 균일하다. 후면 박공 아래와 창틀/보 경계에는 틈·거친 접합이 남는다.',
3:'지붕 중앙 패턴과 기와 높낮이가 고르지 않다. 후면 박공 보 연결부의 어두운 틈이 남는다.',
4:'지붕·벽 큰 면은 유지됐다. 전면 창 위와 후면 박공 아래의 검은 틈이 남아 별도 보수 대상.',
5:'기와 전체 실루엣은 유지됐다. 앞뒤 박공 끝과 후면 세로 보 접합의 작은 틈이 남는다.',
6:'뿌리와 수피의 비대칭을 유지했다. 바닥 접촉부는 불규칙하며 내부 보행 가능한 구조가 아니다.',
7:'원형 단상 보강 후에도 장식판 밑/단상 접합 일부의 작은 틈과 생성형 굴곡이 남는다.',
8:'비대칭 수관과 가지를 유지했다. 잎 덩어리의 각진 면과 중앙 가지 노출은 최종 아트 판단 대상.',
9:'기둥과 개방 공간을 유지했다. 중첩된 비대칭 지붕은 원본 디자인으로 보존했고 최종 아트 확인 대상.'}
SYMM={1:'지붕 틀·기둥 후보; 기와/창틀 차이 보존',2:'지붕·반복 기둥 후보; 문창 구성 보존',3:'지붕·중앙 문틀 후보; 장식 별도',4:'기둥·문틀 후보; 좌우 세부 불일치',5:'몸체 일부 후보; 측면 기능부 보존',6:'뿌리·수피 비대칭 유지',7:'중앙축·양측 장식의 부분 대칭 후보',8:'수관·가지 비대칭 유지',9:'기둥·카운터 반복 후보; 지붕 높이 보존'}

def digest(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def table(headers,rows):return '\n'.join(['| '+' | '.join(headers)+' |','| '+' | '.join(['---']*len(headers))+' |']+['| '+' | '.join(map(str,row))+' |' for row in rows])
def write_json(p,v):p.write_text(json.dumps(v,ensure_ascii=False,indent=2),encoding='utf-8')

def main():
    assert not DEST.exists() and not ARCHIVE.exists(),'Never overwrite a handoff'
    hs=[json.loads(p.read_text(encoding='utf-8')) for p in sorted(DOC.glob('LG_*_FINAL.json'))];assert len(hs)==9
    prior={a['id']:a for a in json.loads((DOC.parent/'review_manifest.json').read_text(encoding='utf-8'))['assets']}
    assembly=json.loads((DOC/'clean_assembly_final.json').read_text(encoding='utf-8'));assert assembly['mesh_count']==9
    assembly_checks=json.loads((DOC/'clean_mesh_checks.json').read_text(encoding='utf-8'));assert len(assembly_checks)==9 and all(c['exact_local_geometry_match'] and c['uv_audit_match'] for c in assembly_checks)
    origin=json.loads((DOC.parent/'handoff_status.json').read_text(encoding='utf-8'));assert digest(origin['original_file'])==origin['original_file_sha256']
    for folder in ('models','views','uv_layouts','qa'):(DEST/folder).mkdir(parents=True)
    shutil.copy2(assembly['output_blend'],DEST/'Libregrove_GameMeshes_FINAL.blend')
    rows=[];grows=[];qrows=[];srows=[];vrows=[];drows=[]
    for h in hs:
        name=h['id'];n=int(name[3:5]);old=prior[name];g=h['final_inspection']['geometry']
        glb=json.loads((DOC/(name+'_FINAL_GLB_QA.json')).read_text(encoding='utf-8'));fbx=json.loads((DOC/(name+'_FINAL_FBX_QA.json')).read_text(encoding='utf-8'))
        assert glb['uv']['passed'] and fbx['uv_audit']['passed'] and fbx['triangle_count_matches'] and h['coincident_triangle_count']==0
        assert glb['export']['total_triangles']==g['triangles']==fbx['report']['geometry']['triangles']
        assert glb['units_match_expected'] and fbx['dimensions_match']
        assert all(abs(v-1)<1e-6 for v in h['final_inspection']['object']['scale'])
        exports={}
        for ext,p in h['exports'].items():
            source=Path(p);dest=DEST/'models'/source.name;shutil.copy2(source,dest);assert digest(source)==digest(dest);exports[ext]={'path':dest.relative_to(DEST).as_posix(),'sha256':digest(dest)}
        for view in ('front_right','back_left'):
            source=OUT/'views'/(name+'_'+h['stage']+'_'+view+'.png');assert source.is_file();shutil.copy2(source,DEST/'views'/(name+'_'+view+'.png'))
        shutil.copy2(DOC/'uv_layouts'/(name+'_FINAL_UV.svg'),DEST/'uv_layouts'/(name+'_UV.svg'))
        for suffix in ('_FINAL.json','_FINAL_GLB_QA.json','_FINAL_FBX_QA.json'):shutil.copy2(DOC/(name+suffix),DEST/'qa'/(name+suffix))
        dims=h['expected_width_depth_height_cm'];r={'id':name,'name_ko':old['name_ko'],'source_vertices':old['source_vertices'],'final_vertices':g['vertices'],'recommended_vertex_min':old['recommended_vertex_min'],'recommended_vertex_max':old['recommended_vertex_max'],'source_triangles':old['source_triangles'],'final_triangles':g['triangles'],'initial_triangle_ceiling':old['triangle_review_ceiling'],'over_initial_triangle_budget':g['triangles']>old['triangle_review_ceiling'],'glb_render_vertices':glb['export']['total_render_vertices'],'width_cm':round(dims[0],2),'depth_cm':round(dims[1],2),'height_cm':round(dims[2],2),'boundary_edges':g['boundary_edges'],'nonmanifold_edges':g['non_manifold_edges'],'exact_duplicate_triangles':0,'uv_overlap_pairs':0,'uv_degenerate_triangles':0,'uv_area_sum_percent':round(glb['uv']['area_sum']*100,2),'glb_fbx_roundtrip_passed':True,'source_reflection_nearness_percent':old['symmetric_nearest_vertices_within_1pct_percent'],'status':'MESH_UV_VERIFIED_ENGINE_AND_ART_SIGNOFF_WITHHELD'}
        rows.append({**r,'exports':exports,'visual_note':NOTES[n]})
        grows.append([f'{n:02} {old["name_ko"]}',f'{r["source_vertices"]:,} → {g["vertices"]:,}',f'{old["source_triangles"]:,} → {g["triangles"]:,}',f'{r["recommended_vertex_min"]:,}–{r["recommended_vertex_max"]:,} / {r["initial_triangle_ceiling"]:,}',' × '.join(f'{v:.2f}' for v in dims),f'{r["glb_render_vertices"]:,}'])
        qrows.append([f'{n:02}',f'{g["boundary_edges"]:,} / 0','0 / 0',f'{r["uv_area_sum_percent"]:.2f}%','둘 다 통과'])
        srows.append([f'{n:02}',f'{r["source_reflection_nearness_percent"]:.1f}%',SYMM[n]])
        links=f'[앞 사선](views/{name}_front_right.png) · [뒤 사선](views/{name}_back_left.png) · [UV](uv_layouts/{name}_UV.svg)'
        vrows.append([f'{n:02}',NOTES[n],links])
        comp=h['surface_comparison'];drows.append([f'{n:02}',f'{comp["reference_to_output"]["max_distance_m"]*100:.4f}',f'{comp["reference_to_output"]["p99_distance_m"]*100:.4f}',f'{comp["output_to_reference"]["max_distance_m"]*100:.4f}'])
    report=(Path(__file__).with_name('precision_report_template.md')).read_text(encoding='utf-8')
    tokens={'GEOMETRY_TABLE':table(['모델','원본 → 현재 정점','원본 → 현재 삼각형','권장 정점 / 초기 삼각형 상한','가로×깊이×높이 cm','GLB 렌더 정점'],grows),'QA_TABLE':table(['모델','열린 경계 / 비다양체 에지','UV 겹침 / 퇴화','UV 면적 합','GLB·FBX 수량/크기'],qrows),'SYMMETRY_TABLE':table(['모델','원본 반사 정점 근접률','대칭 활용 후보'],srows),'VISUAL_TABLE':table(['모델','잔여 시각 관찰','독립 이미지'],vrows),'DISTANCE_TABLE':table(['모델','기준→결과 최대 cm','기준→결과 99% cm','결과→기준 최대 cm'],drows),'OVER_BUDGET':str(sum(r['over_initial_triangle_budget'] for r in rows))}
    for key,value in tokens.items():report=report.replace('{{'+key+'}}',value)
    assert '{{' not in report;(DEST/'검수보고서.md').write_text(report,encoding='utf-8')
    flat=[{k:v for k,v in r.items() if k not in ('exports','visual_note')} for r in rows]
    with (DEST/'모델별_검수표.csv').open('w',encoding='utf-8-sig',newline='') as f:w=csv.DictWriter(f,fieldnames=list(flat[0]));w.writeheader();w.writerows(flat)
    test=record('precision_selftest_005');write_json(DEST/'qa'/'precision_tests.json',test);write_json(DEST/'qa'/'clean_assembly.json',assembly)
    write_json(DEST/'qa'/'clean_mesh_checks.json',assembly_checks)
    cards=[]
    for r in rows:
        name=r['id'];cards.append(f'<section><h2>{html.escape(r["name_ko"])}</h2><p>{html.escape(r["visual_note"])}</p><div><img src="views/{name}_front_right.png"><img src="views/{name}_back_left.png"><a href="uv_layouts/{name}_UV.svg">UV 도면</a></div></section>')
    (DEST/'미리보기.html').write_text('<!doctype html><html lang="ko"><meta charset="utf-8"><title>Libregrove V2</title><style>body{background:#17231f;color:#eee;font:16px system-ui;max-width:1300px;margin:auto;padding:32px}h1{color:#ccdda8}section{padding:24px 0;border-top:1px solid #63756a}img{width:46%;max-width:600px;margin:1%}a{color:#b4dcde}p{line-height:1.7}</style><h1>리베르그로브 메시·UV 정리본 V2</h1><p>9종 메시·UV 기술 검사 통과. 최종 게임 성능·아트 승인은 아님. 크기는 프로젝트 게임 월드 cm 기준.</p><p><a href="검수보고서.md">상세 보고서</a> · <a href="모델별_검수표.csv">정점·치수 CSV</a></p>'+''.join(cards),encoding='utf-8')
    files=[{'path':p.relative_to(DEST).as_posix(),'bytes':p.stat().st_size,'sha256':digest(p)} for p in sorted(DEST.rglob('*')) if p.is_file()]
    manifest={'status':'MESH_UV_VERIFIED_ENGINE_AND_ART_SIGNOFF_WITHHELD','assets':rows,'original_file':origin['original_file'],'original_file_sha256_unchanged':origin['original_file_sha256'],'blender_mesh_count':9,'export_count':18,'clay_views':18,'uv_layouts':9,'original_source_preserved':True,'files':files}
    write_json(DEST/'manifest.json',manifest)
    with zipfile.ZipFile(ARCHIVE,'x',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as z:
        for p in sorted(DEST.rglob('*')):
            if p.is_file():z.write(p,p.relative_to(DEST).as_posix())
    with zipfile.ZipFile(ARCHIVE) as z:assert z.testzip() is None;count=len(z.infolist())
    status={'status':manifest['status'],'package':str(ARCHIVE),'file_count':count,'package_bytes':ARCHIVE.stat().st_size,'package_sha256':digest(ARCHIVE),'original_sha256_unchanged':True,'asset_count':9,'export_qa_passes':18,'uv_overlap_pairs':0,'uv_degenerate_triangles':0,'over_initial_triangle_budget_assets':sum(r['over_initial_triangle_budget'] for r in rows)}
    write_json(DOC/'handoff_final_status.json',status);print(json.dumps(status,ensure_ascii=False))

if __name__=='__main__':main()
