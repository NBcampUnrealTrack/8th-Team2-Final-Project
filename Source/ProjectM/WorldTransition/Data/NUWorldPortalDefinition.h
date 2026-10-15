#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "WorldTransition/Data/NUWorldTransitionTypes.h"
#include "NUWorldPortalDefinition.generated.h"

// PortalTable.csv의 논리적 연결
USTRUCT(BlueprintType)
struct PROJECTM_API FNUWorldPortalDefinition : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal")
	int32 PortalId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal")
	FName PortalKey = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Connection")
	int32 SourceAreaId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Connection")
	int32 SourceLocationId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Connection")
	FName SourceEntryPointId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Connection")
	int32 DestinationAreaId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Connection")
	int32 DestinationLocationId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Connection")
	FName DestinationEntryPointId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal")
	FName PortalType = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal")
	ENUWorldPortalActivation ActivationMode = ENUWorldPortalActivation::Interaction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Connection")
	bool bTwoWay = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal")
	bool bLoadingScreen = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirements")
	int32 RequiredLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirements")
	int32 UnlockQuestId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirements")
	int32 RequiredItemId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Requirements")
	bool bEnabledByDefault = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal")
	FString Description;

	bool ValidateRow(FName RowName, FString& OutError) const;
	bool AreRequirementsMet(int32 PlayerLevel, bool bUnlockQuestCompleted, int32 RequiredItemCount) const;
	FText GetRequirementFailureText(int32 PlayerLevel, bool bUnlockQuestCompleted, int32 RequiredItemCount) const;
};

// 논리적 도착 지점과 클라이언트 배치의 연결. EntryPointId는 Destination에 보관한다.
USTRUCT(BlueprintType)
struct PROJECTM_API FNUWorldPortalEntryBinding
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EntryPoint")
	int32 AreaId = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EntryPoint")
	int32 LocationId = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EntryPoint")
	FNUWorldDestination Destination;

	// 실내에서 재접속했을 때 연결할 외부 출입구.
	UPROPERTY(EditAnywhere, Category = "EntryPoint")
	FNUWorldDestination ReturnDestination;
	UPROPERTY(EditAnywhere, Category = "EntryPoint")
	int32 ReturnLocationId = 0;
};

// 요청할 때 복사하여 보관한다. 테이블 변경과 출발 액터 해제에 영향을 받지 않는다.
struct PROJECTM_API FNUWorldPortalRoute
{
	// 서버 source_side: 정의의 Source 측은 0, Destination 측은 1
	int32 SourceSide = 0;
	FNUWorldPortalDefinition Definition;
	FNUWorldPortalEntryBinding Source;
	FNUWorldPortalEntryBinding Target;
};
