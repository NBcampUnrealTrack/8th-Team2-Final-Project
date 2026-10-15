#pragma once

#include "CoreMinimal.h"
#include "Field/NPC/Data/NUNPCDefinition.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NUNPCDataSubsystem.generated.h"

class UDataTable;

// NpcId로 NPC 정적 정의를 조회한다.
// NPC별 런타임 상태와 퀘스트 진행 상태는 소유하지 않는다.
UCLASS()
class PROJECTM_API UNUNPCDataSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	const FNUNPCDefinition* FindDefinition(int32 NpcId) const;

	// UI와 Blueprint Provider가 정적 정의를 복사해 사용할 때 호출한다.
	UFUNCTION(BlueprintPure, Category = "NU|NPC|Data")
	bool TryGetDefinition(int32 NpcId, FNUNPCDefinition& OutDefinition) const;

	UFUNCTION(BlueprintPure, Category = "NU|NPC|Data")
	bool HasDefinition(int32 NpcId) const;

	UFUNCTION(BlueprintPure, Category = "NU|NPC|Data")
	int32 GetDefinitionCount() const;

private:
	void LoadDefinitionTable();
	void BuildNpcIdIndex();

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> NPCDefinitionTable = nullptr;

	TMap<int32, FName> NpcIdToRowName;
};
