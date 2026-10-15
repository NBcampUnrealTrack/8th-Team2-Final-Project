#pragma once

#include "CoreMinimal.h"
#include "UI/Marker/NUWorldMarkerComponent.h"
#include "NUQuestMarkerComponent.generated.h"

class APawn;
class UNUQuestComponent;
class UNUQuestProviderComponent;

/**
* 퀘스트 단계를 마커 태그로 바꿔 표시하는 어댑터.
* 같은 액터의 UNUQuestProviderComponent에게 단계를 물어본다.
*/
UCLASS(ClassGroup = (NU), meta = (BlueprintSpawnableComponent))
class PROJECTM_API UNUQuestMarkerComponent : public UNUWorldMarkerComponent
{
	GENERATED_BODY()

public:
	// 표시 상태를 다시 계산한다. 퀘스트 진행이 바뀌면 자동으로 불린다
	UFUNCTION(BlueprintCallable, Category = "NU|Quest|Marker")
	void RefreshMarker();

	// 선행 조건을 충족하지 못한 퀘스트도 잠김 전용 마커로 안내한다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Marker")
	bool bShowLockedQuestMarker = false;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// 로컬 플레이어의 퀘스트 컴포넌트를 찾아 진행 변경을 구독
	// PlayerState 복제와 빙의가 BeginPlay보다 늦을 수 있어 성공할 때까지 재시도
	void TryBindQuestComponent();

	UFUNCTION()
	void HandleQuestProgressChanged();

	APawn* GetLocalPlayerPawn() const;

private:
	// 단계를 물어볼 상대. 같은 액터에서 찾는다
	UPROPERTY(Transient)
	TObjectPtr<UNUQuestProviderComponent> QuestProvider;

	UPROPERTY(Transient)
	TObjectPtr<UNUQuestComponent> BoundQuestComponent;

	// 퀘스트 컴포넌트를 못 찾았을 때 다시 시도하는 간격
	UPROPERTY(EditDefaultsOnly, Category = "NU|Quest|Marker", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float BindRetryInterval = 0.5f;

	FTimerHandle BindRetryTimer;
};
