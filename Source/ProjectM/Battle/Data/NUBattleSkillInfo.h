#pragma once

#include "CoreMinimal.h"
#include "Battle/NUBattleTypes.h"

// 정의표의 출처, 효과 실행은 각 표의 원래 정의/효과 행을 사용함
enum class ENUBattleSkillSource : uint8
{
	None,
	Naia,
	Job
};

// 명령 UI가 읽는 공통 정보의 사본, 조회 성공은 실행 가능하다는 뜻이 아님
// 피해 계수는 직업마다 기준 스탯이 달라 여기서 하나의 Power로 합치지 않음
struct PROJECTM_API FNUBattleSkillInfo
{
	int32 SkillId = 0;
	ENUBattleSkillSource Source = ENUBattleSkillSource::None;
	FText DisplayName;
	FText Description;
	int32 SPCost = 0;
	int32 AGCost = 0;
	float HPCostRatio = 0.f;
	int32 HitCount = 1;
	bool bUltimate = false;

	// 전투 하나에서 쓸 수 있는 횟수, 0 = 제한 없음, 직업 스킬 표의 MaxPerBattle 에서 옴
	int32 MaxUsesPerBattle = 0;

	// 피해 줄이 하나라도 있는지, 없으면 중앙으로 안 나가고 제자리에서 씀 (버프 · 디버프 · 회복)
	bool bDealsDamage = false;

	// 지금 표만으로 실제 발동까지 되는지
	bool bExecutable = false;

	// 나이아는 정의표가 값으로 들고, 직업 스킬은 TargetType 칸을 옮겨 담음
	// 표가 모르는 값을 보내면 비어 있음
	TOptional<ENUTargetPolicy> TargetPolicy;
};
