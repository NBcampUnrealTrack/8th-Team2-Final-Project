import json
from pathlib import Path
root=Path(__file__).resolve().parent;v=root/'verification'
read=lambda p:json.loads(p.read_text(encoding='utf-8'))
scene=read(v/'final_scene_audit.json');runtime=read(v/'runtime_suite.json');states=read(v/'presented_state_qa.json');shots=read(v/'player_captures.json')
assert scene['passed'] and runtime['passed'] and states['passed']
assert len(shots)==6 and all(Path(x['file']).is_file() for x in shots)
models=[]
for p in sorted(v.glob('*_source.json')):
 name=p.stem.replace('_source','')
 if name=='RingPile':continue
 d=read(p);g=d['report']['geometry'];assert all(g[k]==0 for k in ['loose_vertices','non_manifold_edges','degenerate_faces'])
 models.append(dict(name=name,blend=d['blend'],object=d['object'],triangles=g['triangles'],open_boundary_edges=g['boundary_edges']))
balance=read(v/'account_balance.json')['data']
report=dict(status='APPLIED_AND_VERIFIED',user_art_approval=False,date='2026-09-05',map=scene['map'],pipeline='Tripo P2 native quad / typed Blender cleanup and visual inspection / UE pixel material',generation_count=24,accepted_bases=23,rejected=['RingPile: cup-like sealed centers; replaced by RingLinks'],credits_consumed=2640,remaining_credits=balance['balance'],scene=scene,gameplay=runtime,states=states,player_captures=shots,models=models,limitations=['Open boundaries on overlapping decorative patches, intentionally open chimney/shelf structures and buried rock undersides remain; not a claim that every source is watertight.','Gameplay floors, rails, stairs and structural gable backing remain purpose-built geometry.','PC performance/cook/package and human art approval are not included in this pass.'],backup='Saved/CindehearthPolishBackup/20260905_v003/L_Cindehearth.umap')
(root/'signoff.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
lines=['# 신데하스 레벨 폴리싱 v003','',
'2026-09-05 — `/Game/Maps/L_Cindehearth`에 적용하고 에디터 및 PIE에서 검증했습니다. 사용자 아트 승인은 아직 기록되지 않았습니다.','',
'Tripo P2 네이티브 쿼드로 24개를 생성해 23개 베이스를 채택했습니다. 431~3,218 삼각형 범위의 단순한 실루엣과 128/256 픽셀 텍스처 샘플링을 사용합니다. Blender 원본과 단계별 수정본은 별도 보존했습니다.','',
'건물 외벽·갱도 프레임·화덕·가구·작업 도구·적재 소품·묘지·나이아·벌목 장비·고리 집하장·할로우코어를 교체했습니다. 지붕 정렬, 산 능선 비례, 동굴 벽, 지형 배경 틈, 주야간 조명도 조정했습니다. 이전에 수정한 입체 소나무 95개는 균일 스케일을 유지합니다.','',
'플레이 카메라에서 캐릭터를 가리던 내부 갱도 아치 7개의 상부 프레임을 숨기고 측면 기둥을 유지했습니다. 주 갱도 입구와 곰굴 입구 아치는 유지합니다.','',
'Q5 벌목 장비를 가리던 소나무 1그루와 대응 줄기 충돌체를 숲 가장자리로 함께 이동해 발견 지점의 시야를 확보했습니다.','',
f"최종 장면: {scene['visible_polish_mesh_actors']}개 표시 메시 액터에 이번 패스 적용. 기존 상자·통 표시 0개, 누락 재질 0개. 구조용 바닥·계단·레일과 지붕 아래 벽체 일부는 목적에 맞게 제작된 기존 구조를 유지합니다.",'',
'## 검증','',
'- 실제 이동 입력: 레일길·묘지·애쉬슬로프·딥베인·파인할로우·고름의 집, 6개 경로 / 39개 지점 통과.',
'- Q0/7/8/9/11/12 상태, 표시 컴포넌트, 장부고 출입, 에필로그 종료 후 입력/카메라 복구 통과.',
'- Q7 문서 단서는 장부고에만 유지. 집하장은 실물 고리의 규모를 보여줍니다.',
'- 채택 메시 23종 모두 떠 있는 정점·비정상 면 접합·퇴화 면 검사값 0. 남은 열린 경계는 아래 모델별 수치를 참조하십시오.',
'- 일부 겹치는 장식 표면, 굴뚝·선반 구조, 매립한 암반 바닥의 열린 경계가 남아 있어 모든 메시가 완전 밀폐되었다는 의미는 아닙니다.',
'- 장부 선반 전면을 잘못 덮은 캡을 렌더에서 발견해 삭제했습니다. 최종 자산은 `ShelfOpen / CH_Polish_Shelf_Open006`입니다.',
'- 강제 재질 재컴파일 검사는 이전 에디터 충돌 원인이라 실행하지 않았습니다. 저장된 자산 로드, 생성 시점 기록, 실제 렌더로 확인했습니다.','',
'## 실제 플레이 화면','',
'[광장 낮](verification/player_square.png) · [광장 밤](verification/player_square_night.png) · [거주 구역](verification/player_homes.png) · [벌목장](verification/player_logging.png) · [집하장](verification/player_depot.png) · [할로우코어](verification/player_cocoon.png)','',
'[마을 전경](verification/final_village_day.png) · [갱도 입구](verification/final_mine.png) · [상세 검증 기록](signoff.json)','',
'## 제작 기록','',
'이번 패스 Tripo 사용 2,640 크레딧, 확인 시 잔액 1,780 / 동결 0. 거부한 첫 RingPile 생성도 비용에 포함됩니다.','',
'원본 레벨 백업: `Saved/CindehearthPolishBackup/20260905_v003/L_Cindehearth.umap`. 최종 레벨은 Q0·낮·PIE 종료 상태로 저장합니다.','',
'| 베이스 | 삼각형 | 남은 열린 경계 에지 |','|---|---:|---:|']
lines += [f"| {m['name']} | {m['triangles']} | {m['open_boundary_edges']} |" for m in models]
(root/'README.md').write_text('\n'.join(lines)+'\n',encoding='utf-8');print('SIGNOFF WRITTEN')
