// [DEPRECATED / 삭제 예정]
//
// Ember Chronicle 초기 프로토타입의 턴 전투 상태머신 헤더였습니다.
// 이 헤더는 ABattleUnit / UParryComponent / EBattleState / EBattlePresentationPhase /
// EBattleTeam / EParryResult / FBattleAction / FEncounterData / FSkillRow 를 참조했지만
// 그 타입들은 현재 프로젝트에 존재하지 않습니다. UHT 는 모듈의 모든 헤더를 파싱하므로
// 대응하는 .cpp 가 없어도 이 파일 하나로 ProjectMEditor 빌드가 실패합니다.
//
// 현재 전투 시스템의 대체 경로:
//   - 전투 진행/상태머신 : ANUBattleEncounter        (Battle/Encounter/NUBattleEncounter.h)
//   - 전투 생성          : UNUBattleEncounterSubsystem
//   - 페이즈             : ENUBattlePhase            (Battle/NUBattleTypes.h)
//   - 결과 / 전환 연출   : ENUBattleResult, ENUTransitionCue
//   - 패리               : UNUParryComponent         (PlayerController/Component/NUParryComponent.h)
//
// UCLASS / USTRUCT / UENUM 과 .generated.h include 를 모두 제거해 UHT 가 이 파일을
// 무시하도록 만들었습니다. 원본 내용은 Git 인덱스에 남아 있으므로 필요하면
//   git show :Source/ProjectM/Battle/BattleManager.h
// 로 확인할 수 있고, 정리는 `git rm Source/ProjectM/Battle/BattleManager.h` 로 하면 됩니다.

#pragma once
