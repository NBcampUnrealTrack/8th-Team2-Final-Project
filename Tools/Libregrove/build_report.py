import csv,hashlib,json,shutil
from pathlib import Path
from blender_mcp_client import ROOT,EVIDENCE

DOC=ROOT/'Docs/Libregrove/BlenderQA_20260908';OUT=DOC/'review_models';OUT.mkdir(exist_ok=True)
assets=json.loads((ROOT/'Docs/Libregrove/model_budgets.json').read_text(encoding='utf-8'))['assets']
baseline={r['name']:r['mesh']['result'] for r in json.loads((EVIDENCE/'baseline_summary.json').read_text(encoding='utf-8'))}
symmetry={r['name']:r['symmetry_nearest_vertex_only']['X'] for r in json.loads((EVIDENCE/'topology_analysis.json').read_text(encoding='utf-8'))}
rows=[];files=[]
for asset in assets:
    name=asset['id'];hpath=EVIDENCE/('review_handoff_'+name+'.json')
    if not hpath.exists():continue
    h=json.loads(hpath.read_text(encoding='utf-8'));q=json.loads((EVIDENCE/('export_qa_'+name+'_REVIEW16.json')).read_text(encoding='utf-8'))
    source=baseline[name]['geometry'];after=h['mesh_report']['geometry'];dims=h['measured_width_depth_height_cm']
    row={'id':name,'name_ko':asset['name'],'source_vertices':source['vertices'],'review_vertices':after['vertices'],'recommended_vertex_min':asset['mesh_vertices_range'][0],'recommended_vertex_max':asset['mesh_vertices_range'][1],'source_triangles':source['triangles'],'review_triangles':after['triangles'],'triangle_review_ceiling':asset['triangle_ceiling'],'export_render_vertices':q['export']['total_render_vertices'],'width_cm':round(dims[0],2),'depth_cm':round(dims[1],2),'height_cm':round(dims[2],2),'boundary_edges':after['boundary_edges'],'nonmanifold_edges':after['non_manifold_edges'],'degenerate_mesh_faces':after['degenerate_faces'],'uv_overlap_pairs':q['positive_overlap_pair_count'],'uv_zero_area_triangles':q['zero_area_uv_triangles_1e_12'],'uv_coverage_sum_percent':round(q['uv_triangle_area_sum']*100,2),'symmetric_nearest_vertices_within_1pct_percent':round(symmetry[name]['within_0_01']*100,2),'status':'REVIEW_REQUIRED_NOT_GAME_SIGNOFF'}
    row['output_object_name']=h['output_object']
    row['display_object_name']='REVIEW_'+name
    rows.append(row)
    if not h['uv_report']['uv_layers']:row['status']='UV_MISSING_BLOCKED'
    for kind,path in h['exports'].items():
        src=Path(path);dest=OUT/src.name
        if dest.exists():assert hashlib.sha256(dest.read_bytes()).digest()==hashlib.sha256(src.read_bytes()).digest()
        else:shutil.copy2(src,dest)
        files.append({'asset':name,'format':kind,'path':str(dest.relative_to(DOC)),'sha256':hashlib.sha256(dest.read_bytes()).hexdigest(),'bytes':dest.stat().st_size})
with (DOC/'모델별_검수표.csv').open('w',encoding='utf-8-sig',newline='') as f:
    writer=csv.DictWriter(f,fieldnames=list(rows[0]));writer.writeheader();writer.writerows(rows)
(DOC/'review_manifest.json').write_text(json.dumps({'status':'INCOMPLETE_GAME_SIGNOFF_WITHHELD','assets':rows,'files':files},ensure_ascii=False,indent=2),encoding='utf-8')
lines=['# 리베르그로브 모델 검수·1차 정리 보고서','',
'2026-09-08 · Blender 5.0.0 / Revvy typed Blender Bridge 1.2.0 · bridge contract compatible=true','',
'## 판정','',
'**부분 완료 작업본 / 게임 투입 최종 승인 보류.** 9종 크기 보정·선별 보수, 8종 자동 UV 작업본이다. 공방은 UV 생성 실패로 UV 없는 보정본만 제공한다. 나머지도 남은 UV 겹침·퇴화 UV 삼각형·낮은 UV 공간 활용률·면 연결 및 지붕 틈 때문에 완전한 게임 레디 모델이나 최종 UV로 사용하면 안 된다. `quality.passed=true`는 도구의 제한된 기본 검사만 통과했다는 뜻이며 최종 승인으로 사용하지 않았다.','',
'## 처리한 사항','',
'- 원본 `D:/Users/Ndus_072/Desktop/mAP.blend` 및 9개 원본 오브젝트는 덮어쓰지 않았다. 모든 편집은 새 메시·새 경로를 사용했다.','- 백업: `C:/Users/Ndus_072/AppData/Local/Revvy/BlenderExchange/Libregrove_20260908/source/Libregrove_SOURCE_before_QA.blend`.','- 검사: 정점/에지/면/렌더 삼각형, 열린 경계, 3개 이상 면이 공유하는 비다양체 에지, 고립 요소, 0길이 에지, 퇴화면, 비정상 좌표/노멀, UV, 원본 fingerprint. 추가로 MCP가 반환한 정점·면 배열을 읽어 대칭 근접도와 연결 컴포넌트를 분석했다.','- 무차별 용접 대신 0.00001 Blender-unit 시험을 했고 일치 정점 쌍이 없어 감소 효과가 없음을 확인했다.','- 작은 구멍·기둥 단면을 정확한 경계 ID로 선별 보수했다. 전체 외곽이나 노점의 영업 공간을 일괄 메우지 않았다. 복잡한 일부 경계는 자동 보수를 제외했다.','- Naia 안식처 감량본과 나무의 완화 감량본을 채택했다. 기와/창틀이 손상되는 건물 Reduced02와 지나치게 각진 나무 Reduced02는 채택하지 않았다.','- 크기 보정 후 UV_Game 채널 생성. 엄격한 각도 분리 자동 펼침(1°, margin parameter 0.002)을 사용했다. 이는 손작업 seam 배치나 효율적인 제작용 UV 완료를 뜻하지 않는다.','',
'## 실제 치수·정점','',
'cm는 이전 제작명세의 게임 월드 기준(캐릭터 약 88cm, 설계 1m→45cm)이다. 현실 사람 크기 기준의 건축 실측 치수가 아니다. 높이를 명세에 맞추고 XYZ를 균등 배율로 조정해 가로·깊이를 찌그러뜨리지 않았다. 가로·깊이는 명세 표와 차이가 있다. 로컬 +X 정면/+Y 우측/+Z 위, 바닥 중앙 피벗. 문 개구부·플레이어 캡슐/내부 보행 크기는 이번에 검증하지 않았으며 건물들은 닫힌 문을 가진 외관 모델이다.','',
'| 모델 | 정점: 원본 → 작업본 | 삼각형: 원본 → 작업본 | 실측 가로×깊이×높이 cm | 초기 권장 정점 / 삼각형 상한 |','| --- | ---: | ---: | --- | --- |']
for r in rows:
    lines.append(f"| {r['id'][3:5]} {r['name_ko']} | {r['source_vertices']:,} → {r['review_vertices']:,} | {r['source_triangles']:,} → {r['review_triangles']:,} | {r['width_cm']} × {r['depth_cm']} × {r['height_cm']} | {r['recommended_vertex_min']:,}–{r['recommended_vertex_max']:,} / {r['triangle_review_ceiling']:,} |")
lines+=['','정점 수만으로 게임 사용 가능 여부를 단정하지 않았다. 초기 권장치를 넘는 모델은 면 단위 평면 정리·기와/보 반복 구조 정리·LOD 등 후속 작업 대상이다. GLB 렌더 정점은 UV/노멀 분할이 반영된 내보내기 실측값이며 실제 Unreal 빌드 정점/프레임 성능과 같다고 보장하지 않는다. Nanite, collision, LOD, lightmap, 재질/텍스처, 엔진 임포트·쿠킹·실기기 성능은 미검증이다.','',
'## UV·토폴로지 상세','',
'| 모델 | 열린 경계 / 비다양체 에지 | UV 겹침 삼각형 쌍 | 퇴화 UV 삼각형 | UV 면적 합계 | GLB 렌더 정점 |','| --- | ---: | ---: | ---: | ---: | ---: |']
for r in rows:
    overlap=r['uv_overlap_pairs'] if r['uv_overlap_pairs']>=0 else '미생성'
    degenerate=r['uv_zero_area_triangles'] if r['uv_zero_area_triangles']>=0 else '—'
    coverage=str(r['uv_coverage_sum_percent'])+'%' if r['uv_overlap_pairs']>=0 else '—'
    lines.append(f"| {r['id']} | {r['boundary_edges']:,} / {r['nonmanifold_edges']:,} | {overlap} | {degenerate} | {coverage} | {r['export_render_vertices']:,} |")
lines+=['','UV 겹침은 AABB 개수 추정이 아니라 GLB의 실제 삼각형 UV를 2D polygon clipping으로 교차 검사한 결과다. float32 공유 경계 오차를 제외하는 허용값이 있으며 절대적인 무오차 증명은 아니다. 면적 합은 겹침을 포함하므로 고유 점유율과 다르다. margin은 Blender 연산자 입력값이며 정확히 4px 간격이라고 주장하지 않는다. SVG는 GLB TEXCOORD_0의 실제 좌표로 생성했다.','',
'열린 경계 일부는 판재·접합부·의도된 열린 공간이지만 모두 의도된 경계라고 검증하지 않았다. 비정상 노멀 수 0은 모든 면 방향이 바깥쪽이라는 증명이 아니다. 모델은 전체 밀폐 메시가 아니다. 특히 주택/여관 등의 기와 틈과 생성 메시의 중첩 구조를 최종 아트에서 추가로 보수해야 한다.','',
'## 좌우 대칭 최적화 검토','',
'원본 X 반사(정면 -Y 기준 좌우) 후 가장 가까운 원본 정점까지 거리로 후보를 판별했다. 아래 1% 근접률은 정규화 크기 약 1의 모델에서 0.01 이내인 비율이며 표면의 완전한 대칭률이나 절감률은 아니다.','',
'| 모델 | 1% 이내 반사 정점 근접률 | 판단 |','| --- | ---: | --- |']
decisions={1:'지붕 큰 형태·기둥 후보. 기와와 창틀은 차이가 있어 전체 미러 불가',2:'지붕·기둥 반복 구조 후보. 문/창문 구성 보존 필요',3:'지붕·중앙 문틀 후보. 장식/후면은 별도 보존',4:'기둥·문틀 후보. 좌우 세부 형상 불일치',5:'몸체 일부 후보. 측면 축 장착부 기능 보존',6:'뿌리·수피 비대칭 유지; 전체 미러 제외',7:'대칭 후보가 가장 강함. 중앙축/양측 장식 분리 검토',8:'수관·가지 입체 비대칭 유지; 전체 미러 제외',9:'기둥·카운터 반복 구조 후보. 앞/뒤 지붕 높이 보존'}
for asset in assets:
    n=int(asset['id'][3:5]);rate=symmetry[asset['id']]['within_0_01']*100
    lines.append(f"| {asset['id']} | {rate:.1f}% | {decisions[n]} |")
lines+=['','Mirror 자체는 게임에 내보내는 삼각형을 반으로 줄이는 기능이 아니다. 반쪽 편집, UV 재사용, 반복 부품 제작에 유리하지만 적용 후 양쪽 면은 다시 존재한다. 무조건 전체 미러를 적용하지 않았다. 부분 bisect/symmetrize·선택 용접·평면 dissolve·UV seam 편집이 필요하다.','',
'## 남은 작업과 도구 제약','',
'현재 주 MCP의 mesh_edit는 전체 메시 변환/용접/노멀/삼각분할/고립 요소 제거만, modifier_apply는 제한된 설정만, uv_edit는 전체 Smart Project/Cube Project/Unwrap/Pack만 제공한다. 기존 Repair MCP는 정확한 면/경계 선택 삭제·메우기를 보완하지만 UV seam 표시·개별 UV 좌표/섬 편집·planar dissolve·bisect 후 mirror 제어는 없다.','',
'`blender-3d-modeling` 스킬의 Typed Tool Capability Contract 및 Critical Rules에 따라 없는 정밀 편집 기능을 임의 Blender Python/콘솔/UI 매크로로 우회하지 않았다. 필요한 선택 토폴로지·UV 편집 기능을 typed Blender MCP에 추가하거나 지원되는 편집 도구를 연결한 뒤 계속해야 한다. 최종 게임 투입과 UV 완성 판정은 그 작업 및 시각 재검수 후에 가능하다.','',
'## 파일 안내','',
'- `review_models/*_REVIEW.glb`, `*_REVIEW.fbx`: 수정 작업본. 최종 게임 에셋 아님. 개별 파일의 피벗은 바닥 중앙, 위치는 원점이다.','- `uv_layouts/*_REVIEW16.svg`: UV 검토도.','- `모델별_검수표.csv`, `review_manifest.json`: 수치·SHA-256·상태.','- `evidence/`: 도구 스키마, 원본/단계별/최종 검사, 고유 job_id별 완료/실패 기록. 실패한 시도는 채택하지 않았다.','- `Libregrove_검수작업본.blend`: 별도 저장된 작업 세션(작성 시점 파일이 제공된 경우). 원본/실험본을 포함하므로 REVIEW 컬렉션과 manifest의 정확한 오브젝트만 사용한다.','',
'중요: Blender의 save_copy는 현재 작업 파일 경로를 바꾸지 않는다. 원본을 보호하려면 원본 mAP.blend에 Ctrl+S하지 말고 제공된 검수 작업본을 열어 이어서 작업한다.','']
lines+=['','## 전달 작업본 시각 재검수','',
'9종을 각 2개 반대 각도, 총 18장의 클레이 렌더로 재검수했다. 주택/여관/회관/공방 지붕의 열린 틈, 물레방앗간 보 접합부 틈, 성역 단상 연결부 틈 등 잔여 결함을 확인했다. 노점 지붕 중첩과 수목의 각진 표면도 최종 아트 확인 대상이다. 상세 관찰과 이미지 링크는 [시각검수.md](시각검수.md)에 기록했다. 미검출이 무결함을 뜻하지 않으며 모든 모델의 게임용 최종 승인은 보류한다.','',
'실제 제공 .blend의 검토용 컬렉션은 `LG_REVIEW_20260908`이다. 그 안의 `REVIEW_LG_*`는 겹치지 않도록 배치한 독립 복제본이며, 개별 내보내기 원본은 manifest의 `output_object_name`을 따른다. `evidence/` 전체는 프로젝트 폴더에만 남겼으며 ZIP에는 포함하지 않았다.','']
(DOC/'검수보고서.md').write_text('\n'.join(lines),encoding='utf-8')
print(json.dumps({'assets':len(rows),'files':len(files),'report':str(DOC/'검수보고서.md')},ensure_ascii=False))
