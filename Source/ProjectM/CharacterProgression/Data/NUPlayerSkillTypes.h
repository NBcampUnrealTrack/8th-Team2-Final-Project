#pragma once

#include "CoreMinimal.h"
#include "NUPlayerSkillTypes.generated.h"

// 플레이어가 스킬을 획득한 경로
UENUM(BlueprintType)
enum class ENUPlayerSkillGrantSource : uint8
{
	ProgressionNode,
	Job,
	Advancement,
	Debug
};

// 같은 스킬을 여러 경로에서 얻을 수 있으므로 스킬과 획득 경로를 함께 보존
USTRUCT(BlueprintType)
struct PROJECTM_API FNUOwnedActiveSkill
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "NU|Skill")
	int32 SkillId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Skill")
	ENUPlayerSkillGrantSource GrantSource = ENUPlayerSkillGrantSource::ProgressionNode;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Skill")
	int32 GrantSourceId = 0;

	bool operator==(const FNUOwnedActiveSkill& Other) const
	{
		return SkillId == Other.SkillId
			&& GrantSource == Other.GrantSource
			&& GrantSourceId == Other.GrantSourceId;
	}
};

// 패시브 실행과 분리해 보유 경로만 저장한다.
USTRUCT(BlueprintType)
struct PROJECTM_API FNUOwnedPassiveSkill
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "NU|Skill")
	int32 SkillId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Skill")
	ENUPlayerSkillGrantSource GrantSource = ENUPlayerSkillGrantSource::ProgressionNode;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Skill")
	int32 GrantSourceId = 0;

	bool operator==(const FNUOwnedPassiveSkill& Other) const
	{
		return SkillId == Other.SkillId
			&& GrantSource == Other.GrantSource
			&& GrantSourceId == Other.GrantSourceId;
	}
};

// 서버에서 확정한 플레이어 직업 스킬 보유 상태.
// 나이아와 같이 액티브/패시브 목록을 분리하되 실행 효과는 이 상태가 적용하지 않는다.
USTRUCT(BlueprintType)
struct PROJECTM_API FNUPlayerSkillState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "NU|Skill")
	TArray<FNUOwnedActiveSkill> OwnedActiveSkills;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Skill")
	TArray<FNUOwnedPassiveSkill> OwnedPassiveSkills;

	bool operator==(const FNUPlayerSkillState& Other) const
	{
		return OwnedActiveSkills == Other.OwnedActiveSkills
			&& OwnedPassiveSkills == Other.OwnedPassiveSkills;
	}

	bool operator!=(const FNUPlayerSkillState& Other) const
	{
		return !(*this == Other);
	}
};
