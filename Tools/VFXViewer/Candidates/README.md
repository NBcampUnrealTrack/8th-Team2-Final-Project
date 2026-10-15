# 공용 VFX 후보

VFX 뷰어가 바로가기로 떠도 이 폴더의 `*.json` 을 모두 읽어, 스킬 목록에 "후보 N" 으로 보여 준다.
형식과 사용법은 `Tools/VFXViewer/README.md` 의 "후보 비교", 예시는 `Tools/VFXViewer/compare_example.json`.

- 작업마다 파일 하나 (예: `naia_projectile.json`). 같은 파일을 여럿이 고치면 병합 충돌이 난다.
- 후보가 가리키는 이펙트 에셋도 같이 커밋한다. 없으면 팀원 뷰어에 "못 찾은 이름" 으로 뜬다.
- 고른 후보(적용 대기)는 각자 `Saved/VFXViewer/Pending.json` 에 남는다 (커밋 안 함).
- 뷰어 [적용] 으로 표에 들어간 뒤에는 이 폴더의 그 작업 파일을 지운다 (결과는 SkillVFXTable.csv · DT_SkillVFX 에 남음).
- 혼자 실험할 후보는 여기 말고 `Saved/VFXViewer/Candidates.json` 에 둔다.
