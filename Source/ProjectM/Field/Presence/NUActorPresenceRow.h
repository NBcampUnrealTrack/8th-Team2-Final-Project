#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "Quest/Data/NUQuestStateCondition.h"
#include "NUActorPresenceRow.generated.h"

class UNUQuestBattleDefinition;

// 같은 PresenceId 중 조건이 모두 참인 행을 고른다. StepOrder가 가장 큰 행이 우선한다.
USTRUCT(BlueprintType)
struct PROJECTM_API FNUActorPresenceRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|ActorPresence")
	FName PresenceId;

	// 같은 PresenceId 안에서는 중복할 수 없다. CSV 행 순서에는 의존하지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|ActorPresence", meta = (ClampMin = "0"))
	int32 StepOrder = 0;

	// 모두 만족해야 한다. 비우면 늘 참인 기본 행이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|ActorPresence")
	TArray<FNUQuestStateCondition> Conditions;

	// 로컬 플레이어가 이 나이아(NaiaId)와 시작 계약을 맺었으면 참. Conditions와 함께 모두 만족해야 한다. 0이면 보지 않는다.
	// 계약한 나이아는 동행 나이아로 따라다니므로, 마을에 세워 둔 같은 나이아를 숨기는 행에 쓴다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|ActorPresence", meta = (ClampMin = "0"))
	int32 ContractedStarterNaiaId = 0;

	// 비우지 않으면 Conditions가 참인 동안 로컬 플레이어가 이 접근 트리거(ANUApproachTrigger)에 들어선 뒤부터 참이다.
	// 다가가면 나타나거나 달아나는 연출에 쓴다. 들어선 일은 이번 접속 동안만 기억한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|ActorPresence")
	FName ApproachActorId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|ActorPresence")
	bool bVisible = true;

	// NUActorPresenceSpot의 SpotId. 비우면 레벨에 처음 배치한 위치·방향을 유지한다. 숨는 행이면 숨긴 채로 옮겨 둔다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|ActorPresence")
	FName SpotId;

	// 켜면 자리 근처에 온 로컬 플레이어 주변을 맴돌고, 플레이어가 멀어지면 자리로 돌아온다. 나이아 모습 액터(ANUQuestNaiaActor)만 따른다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|ActorPresence")
	bool bHoverAroundPlayer = false;

	// 일반 대화·상점 등은 액터가 계속 담당한다. 켠 행만 퀘스트 대상과 전투를 교체한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|ActorPresence|Quest")
	bool bOverrideQuestInteraction = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|ActorPresence|Quest", meta = (EditCondition = "bOverrideQuestInteraction"))
	int32 QuestActorId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|ActorPresence|Quest", meta = (EditCondition = "bOverrideQuestInteraction", Categories = "NU.Quest.Objective"))
	FGameplayTag ObjectiveType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|ActorPresence|Quest", meta = (EditCondition = "bOverrideQuestInteraction"))
	TSoftObjectPtr<UNUQuestBattleDefinition> Battle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|ActorPresence")
	FString DesignNote;

	bool Validate(FString& OutError) const;
	// LocalStarterNaiaId는 로컬 플레이어가 시작 계약을 맺은 나이아. 아직 계약하지 않았으면 0.
	bool IsMet(TFunctionRef<const FNUQuestProgressState*(FName)> FindProgress, TFunctionRef<const UNUQuestDefinition*(FName)> FindDefinition, int32 LocalStarterNaiaId) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
