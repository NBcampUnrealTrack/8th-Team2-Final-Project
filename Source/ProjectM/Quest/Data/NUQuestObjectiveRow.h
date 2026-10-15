#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Quest/Data/NUQuestTypes.h"
#include "NUQuestObjectiveRow.generated.h"

/** 목표 하나당 한 행. 신규 ID는 양의 int32이며 기존 단일 목표의 Quest_N_Objective ID도 보존한다. */
USTRUCT(BlueprintType)
struct PROJECTM_API FNUQuestObjectiveRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Identity", meta = (ClampMin = "1"))
	int32 QuestId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Display")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Objective", meta = (Categories = "NU.Quest.Objective"))
	FGameplayTag ObjectiveType;

	// 목표 타입에 해당하는 대상의 정수 ID. 0은 대상 제한 없음이며 900100001~900100020는 임시 테스트 ID이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Objective", meta = (ClampMin = "0"))
	int32 TargetId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Objective", meta = (ClampMin = "1"))
	int32 RequiredCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Objective")
	bool bOptional = false;

	// 준비 단계 목표. 끝나기 전까지만 목표 창·수락 창·일지에 보이고, 끝나기 전에는 뒤 목표 줄을 공개하지 않는다. 진행 판정과 대상 마커는 그대로다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Display")
	bool bHideInObjectiveList = false;

	// 같은 QuestId의 목표만 참조한다. 0은 선행 목표 없음이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Objective")
	int32 PrerequisiteObjectiveId = 0;

	bool ValidateRow(FName RowName, FString& OutError) const;
	FNUQuestObjectiveSpec MakeObjective(FName RowName) const;
	virtual void OnPostDataImport(const UDataTable* InDataTable, FName InRowName, TArray<FString>& OutCollectedImportProblems) override;
};
