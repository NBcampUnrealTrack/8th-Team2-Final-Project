"""Create a new, verified per-model texturing handoff. Never overwrite prior packages."""
import csv,hashlib,html,json,shutil,zipfile
from pathlib import Path
from polish_v3 import ASSETS,DOC,OUT,ROOT,record

DEST=ROOT/'Docs/Libregrove/Export_Texturing_V3_20260908'
ARCHIVE=ROOT/'Docs/Libregrove_Texturing_V3_20260908.zip'
NOTES={
1:'전면 지붕 검은 삼각 음영을 유발한 접힌 내부 면 1개와 감량 후 완전히 겹친 면 1개 제거. 후면의 작은 기와 틈과 불규칙한 높낮이·접힘은 남음.',
2:'약 20% 감량 및 노멀 재정리. 후면 처마·박공의 깊은 접합선과 생성형 기와 불규칙성은 남음.',
3:'약 20% 감량 및 노멀 재정리. 기와 중간의 높낮이 차이와 후면 목재 접합 형태는 남음.',
4:'약 20% 감량. 면에 속하지 않는 에지 1개/고립 정점 2개 정리, 두 창 배면 부품을 2mm 분리해 창 위 검은 줄 보정. 후면 박공/처마 접합 일부의 작은 틈은 남음.',
5:'약 20% 감량. 앞뒤 박공의 가로 보와 후면 세로 보를 각각 2mm 분리해 검은 접촉 음영 보정.',
6:'V2 형상 유지, UV 여백 재패킹. 뿌리·수피 비대칭 보존. 보행 가능한 내부 공간이나 충돌체는 제작하지 않음.',
7:'약 20% 감량. 단상·장식판 일부의 접힘/틈이 남음. 모든 면 교차와 열린 경계를 제거한 수밀 모델은 아님.',
8:'추가 20% 감량 시험은 수관 표면 오차가 커서 제외. V2 형상 유지, UV 재패킹. 대량 배치 전 별도 LOD/엔진 성능 검증 필요.',
9:'V2 형상 유지, UV 재패킹. 노점 개방 공간과 이중 지붕 형태 보존.'}

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def load(p):return json.loads(Path(p).read_text(encoding='utf-8'))
def write(p,value):
    assert not p.exists(),str(p);p.parent.mkdir(parents=True,exist_ok=True);p.write_text(value,encoding='utf-8')
def json_write(p,value):write(p,json.dumps(value,ensure_ascii=False,indent=2))
def copy(source,dest):
    assert not dest.exists();dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,dest);assert sha(source)==sha(dest)
def table(headers,rows):return '\n'.join(['| '+' | '.join(headers)+' |','| '+' | '.join(['---']*len(headers))+' |']+['| '+' | '.join(map(str,r))+' |' for r in rows])

def main():
    assert not DEST.exists() and not ARCHIVE.exists()
    hs=[load(DOC/(ASSETS[n]['id']+'_FINAL.json')) for n in range(1,10)]
    budgets={x['id']:x for x in load(ROOT/'Docs/Libregrove/model_budgets.json')['assets']}
    assembly=load(DOC/'clean_assembly.json');checks=load(DOC/'clean_assembly_checks.json')
    assert assembly['mesh_count']==9 and len(checks)==9 and all(c['local_geometry_and_uv_match'] for c in checks)
    original=load(ROOT/'Docs/Libregrove/BlenderQA_20260908/handoff_status.json')
    assert sha(original['original_file'])==original['original_file_sha256']
    for item in load(DOC/'immutable_sources.json'):assert sha(item['path'])==item['sha256']
    DEST.mkdir();copy(assembly['output_blend'],DEST/'Libregrove_Texturing_V3.blend')
    rows=[];cards=[];geometry_rows=[];qa_rows=[];surface_rows=[]
    for h in hs:
        name=h['id'];n=int(name[3:5]);budget=budgets[name];base=ASSETS[n]['final_inspection']['geometry'];g=h['geometry']
        glb=load(DOC/(name+'_FINAL_GLB_QA.json'));fbx=load(DOC/(name+'_FINAL_FBX_QA.json'))
        assert glb['uv']['passed'] and fbx['uv_audit']['passed'] and glb['triangle_count_matches'] and fbx['triangle_count_matches']
        assert glb['units_match_expected'] and fbx['dimensions_match'] and not glb['export']['images']
        assert h['fingerprint']==h['clean_fingerprint'],'Single-asset source geometry/UV fingerprint mismatch'
        assert glb['export']['total_triangles']==g['triangles']==fbx['report']['geometry']['triangles']
        assert all(g[key]==0 for key in ('non_manifold_edges','degenerate_faces','invalid_face_normals','non_finite_vertices','loose_vertices','loose_edges','zero_length_edges'))
        folder=DEST/'Models'/name
        copy(h['clean_blend'],folder/'Source'/(name+'.blend'))
        for ext,path in h['exports'].items():copy(path,folder/'Export'/(name+'.'+ext))
        copy(DOC/'uv_layouts'/(name+'_UV.svg'),folder/'UV'/(name+'_UV.svg'))
        for view in ('front_right','back_left'):
            p=OUT/'views'/(f'V3_{name}_FINAL_{view}.png');assert p.is_file()
            receipt=record(f'V3_{name}_FINAL_{view}');copy(p,folder/'Preview'/(view+'.png'))
        for suffix in ('_FINAL.json','_FINAL_GLB_QA.json','_FINAL_FBX_QA.json'):copy(DOC/(name+suffix),DEST/'QA'/(name+suffix))
        dims=h['expected_width_depth_height_cm'];comp=h['surface_comparison_to_v2'];reduction=(1-g['triangles']/base['triangles'])*100
        r={'id':name,'name_ko':budget['name'],'v2_vertices':base['vertices'],'final_vertices':g['vertices'],'recommended_vertex_min':budget['mesh_vertices_range'][0],'recommended_vertex_max':budget['mesh_vertices_range'][1],'v2_triangles':base['triangles'],'final_triangles':g['triangles'],'triangle_reduction_from_v2_percent':round(reduction,2),'initial_triangle_budget':budget['triangle_ceiling'],'over_initial_triangle_budget':g['triangles']>budget['triangle_ceiling'],'glb_render_vertices':glb['export']['total_render_vertices'],'width_cm':round(dims[0],3),'depth_cm':round(dims[1],3),'height_cm':round(dims[2],3),'boundary_edges':g['boundary_edges'],'nonmanifold_edges':0,'degenerate_faces':0,'uv_overlap_pairs':0,'uv_degenerate_triangles':0,'uv_area_percent':round(glb['uv']['area_sum']*100,2),'uv_margin_fraction':.002,'glb_fbx_qa_passed':True,'sampled_surface_max_error_to_v2_cm':round(max(comp[d]['max_distance_m'] for d in ('reference_to_output','output_to_reference'))*100,5),'material_slots':len(glb['export']['materials']),'texture_images':0}
        rows.append(r);geometry_rows.append([f'{n:02} {budget["name"]}',f'{base["vertices"]:,} → {g["vertices"]:,}',f'{base["triangles"]:,} → {g["triangles"]:,}',f'{r["recommended_vertex_min"]:,}–{r["recommended_vertex_max"]:,} / {r["initial_triangle_budget"]:,}',' × '.join(f'{v:.2f}' for v in dims),f'{r["glb_render_vertices"]:,}'])
        qa_rows.append([f'{n:02}',g['boundary_edges'],'0 / 0','0 / 0',f'{r["uv_area_percent"]:.2f}%','통과'])
        surface_rows.append([f'{n:02}',f'{comp["reference_to_output"]["max_distance_m"]*100:.4f}',f'{comp["reference_to_output"]["p99_distance_m"]*100:.4f}',f'{comp["output_to_reference"]["max_distance_m"]*100:.4f}'])
        text=f'# {budget["name"]}\n\n`Source/{name}.blend` 또는 `Export/{name}.fbx`로 텍스처링을 시작하세요. 두 형식 모두 단일 메시 `{h["clean_object"]}`이며 원점 (0,0,0), 적용 스케일 (1,1,1)입니다.\n\n- 실제 게임 월드 크기(가로×깊이×높이): '+ ' × '.join(f'{v:.2f}' for v in dims)+f' cm\n- 정점 {g["vertices"]:,}, 삼각형 {g["triangles"]:,}, GLB 분할 렌더 정점 {r["glb_render_vertices"]:,}\n- 권장 원래 정점 예산 {r["recommended_vertex_min"]:,}–{r["recommended_vertex_max"]:,}, 삼각형 예산 {r["initial_triangle_budget"]:,}\n- UV: 0–1, 겹침/퇴화 검사 통과, 패킹 margin fraction 0.002. UV SVG는 투명 배경 2048 좌표 가이드이며 텍스처가 아닙니다.\n- 기존 기본 재질만 보존했습니다. 생성/페인팅한 이미지 텍스처는 없습니다.\n\n{NOTES[n]}\n\n새 텍스처 파일은 `Textures/`에 저장하면 됩니다. 원본 Tripo UV나 V2 UV와 호환되는 텍스처 좌표는 아닙니다. 이 V3 메시에서 베이크/페인팅하고, 그 이후 임의 재삼각화·UV 재패킹을 피하세요.\n'
        write(folder/'README.md',text);write(folder/'Textures'/'README.txt','사용자가 직접 제작할 텍스처를 저장하는 폴더입니다. 현재 이미지 텍스처는 없습니다.\n')
        cards.append(f'<section><h2>{n:02} {html.escape(budget["name"])}</h2><p>{g["vertices"]:,} vertices · {g["triangles"]:,} triangles · {reduction:.1f}% reduction from V2</p><p>{html.escape(NOTES[n])}</p><div><img loading="lazy" src="Models/{name}/Preview/front_right.png"><img loading="lazy" src="Models/{name}/Preview/back_left.png"></div><p><a href="Models/{name}/README.md">모델 안내</a> · <a href="Models/{name}/UV/{name}_UV.svg">UV 도면</a> · <a href="Models/{name}/Export/{name}.fbx">FBX</a> · <a href="Models/{name}/Source/{name}.blend">Blender</a></p></section>')
    total_before=sum(r['v2_triangles'] for r in rows);total_after=sum(r['final_triangles'] for r in rows);over=sum(r['over_initial_triangle_budget'] for r in rows)
    body=(Path(__file__).with_name('polish_report_template.md')).read_text(encoding='utf-8')
    tokens={'BEFORE':f'{total_before:,}','AFTER':f'{total_after:,}','REDUCTION':f'{(1-total_after/total_before)*100:.2f}','OVER':str(over),'GEOMETRY_TABLE':table(['모델','V2 → V3 정점','V2 → V3 삼각형','권장 정점 / 삼각형 예산','실측 가로×깊이×높이 cm','GLB 렌더 정점'],geometry_rows),'QA_TABLE':table(['모델','열린 경계 에지','비다양체 / 퇴화면','UV 겹침 / 퇴화','UV 면적 합','GLB·FBX 재검사'],qa_rows),'SURFACE_TABLE':table(['모델','V2→V3 최대 cm','V2→V3 99% cm','V3→V2 최대 cm'],surface_rows),'VISUAL_NOTES':'\n'.join(f'- {n:02}: {NOTES[n]}' for n in range(1,10))}
    for key,value in tokens.items():body=body.replace('{{'+key+'}}',value)
    assert '{{' not in body;write(DEST/'README.md',body)
    with (DEST/'모델별_버텍스_크기_UV_검수표.csv').open('x',encoding='utf-8-sig',newline='') as f:
        w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
    for file in ('clean_assembly.json','clean_assembly_checks.json','immutable_sources.json'):copy(DOC/file,DEST/'QA'/file)
    json_write(DEST/'QA'/'typed_mcp_self_tests.json',record('V3_test002'))
    write(DEST/'미리보기.html','<!doctype html><html lang="ko"><meta charset="utf-8"><title>Libregrove V3 texturing handoff</title><style>body{max-width:1300px;margin:auto;padding:32px;background:#17231f;color:#ecf2e5;font:16px system-ui}section{border-top:1px solid #60746a;padding:24px 0}img{width:47%;margin:1%}a{color:#a5dddd}p{line-height:1.6}</style><h1>리베르그로브 V3 · 텍스처링 전달본</h1><p>9종 메시·UV·export 기술 검사 완료. 텍스처링 미실행. 엔진 성능/최종 아트 승인은 별도.</p><p><a href="README.md">작업 결과 및 주의사항</a> · <a href="모델별_버텍스_크기_UV_검수표.csv">버텍스·크기·UV 검수표</a></p>'+''.join(cards))
    files=[{'path':p.relative_to(DEST).as_posix(),'bytes':p.stat().st_size,'sha256':sha(p)} for p in sorted(DEST.rglob('*')) if p.is_file()]
    manifest={'status':'TEXTURE_FREE_MESH_UV_EXPORT_VERIFIED_ENGINE_ART_SIGNOFF_SEPARATE','blender_version':'5.0.0','asset_count':9,'export_count':18,'individual_clean_blends':9,'overview_blends':1,'textures_authored':False,'original_source_preserved':True,'original_file':original['original_file'],'original_sha256':original['original_file_sha256'],'triangle_total_v2':total_before,'triangle_total_v3':total_after,'over_initial_triangle_budget_assets':over,'assets':rows,'files':files}
    json_write(DEST/'manifest.json',manifest)
    with zipfile.ZipFile(ARCHIVE,'x',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
        for p in sorted(DEST.rglob('*')):
            if p.is_file():z.write(p,p.relative_to(DEST).as_posix())
    with zipfile.ZipFile(ARCHIVE) as z:assert z.testzip() is None;count=len(z.infolist())
    status={'folder':str(DEST),'zip':str(ARCHIVE),'file_count':count,'zip_bytes':ARCHIVE.stat().st_size,'zip_sha256':sha(ARCHIVE),'assets':9,'exports':18,'textures':0,'original_unchanged':True,'triangles_before':total_before,'triangles_after':total_after,'over_initial_triangle_budget_assets':over}
    json_write(DOC/'handoff_status.json',status);print(json.dumps(status,ensure_ascii=False),flush=True)

if __name__=='__main__':main()
