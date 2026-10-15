#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/Interface/NUInteractableInterface.h"
#include "Battle/Encounter/NUBattleEncounterTypes.h"
#include "NUQuestActionActor.generated.h"

class USphereComponent;
class UWidgetComponent;
class UStaticMeshComponent;
class UNUQuestProviderComponent;
class UNUQuestMarkerComponent;
class UNUQuestComponent;
class UNUQuestBattleDefinition;
class UPaperFlipbookComponent;
class UPaperSprite;
class UPaperSpriteComponent;
struct FNUActorPresenceRow;

// 반복 작업과 퀘스트 전용 전투 진입을 배치 데이터로 연결한다.
UCLASS(Blueprintable)
class PROJECTM_API ANUQuestActionActor : public AActor, public INUInteractableInterface
{
	GENERATED_BODY()

public:
	ANUQuestActionActor();

	// 상호작용 가능 조건 확인 및 목표 진행 요청
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual bool Interact_Implementation(AActor* Interactor) override;

	void MoveBattleStone();
	void SetBattleBurrowed(bool bBurrowed, UPaperFlipbookComponent* Sprite);

	// 돌 옮기기로 옆에 밀어 둔 돌을 치운다. 전투 세션이 다음 턴 시작에 부른다.
	void ClearMovedBattleStone();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest")
	bool bMoveStoneDuringBattle = false;

	// 누르면 목표를 바로 채우지 않고 이 퀘스트의 의뢰서를 먼저 띄운다. 수락하면 목표를 채운 뒤 수락하고, 나중에를 고르면 아무것도 바뀌지 않는다.
	// 게시판 확인(Q4 마지막 목표)에서 공방 심부름(Q5)을 받는 데 쓴다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest")
	FName OfferQuestOnUse = NAME_None;

	// 목표 진행과 월드 표시를 담당하는 구성 요소.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Quest")
	TObjectPtr<UNUQuestProviderComponent> QuestProvider;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Quest")
	TObjectPtr<UStaticMeshComponent> Mesh;

	// 이 액터가 처리할 퀘스트와 목표.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest")
	FName QuestId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest")
	FName ObjectiveId;

	// 여러 수집 작업이 모두 끝나야 열리는 전투 진입점에 사용한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest")
	TArray<FName> RequiredObjectiveIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest")
	FNUBattleEncounterRequest EncounterRequest;

	// 반복 입력 간격과 설치 완료 후 메시 표시 여부.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest", meta = (ClampMin = "0"))
	float InteractionCooldown = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest")
	bool bShowInstalledState = false;

	// 지정한 작업만 2D 전후 상태를 쓴다. 목표 완료 여부는 저장된 퀘스트 진행에서 읽는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|State Sprite")
	TObjectPtr<UPaperSprite> IncompleteSprite;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|State Sprite")
	TObjectPtr<UPaperSprite> CompletedSprite;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Quest|State Sprite")
	TObjectPtr<UPaperSpriteComponent> StateSprite;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|State Sprite")
	FVector StateSpriteOffset = FVector::ZeroVector;

	// 목재를 옮긴 뒤처럼 완료 그림의 자리도 바뀌는 작업에 사용한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|State Sprite")
	FVector CompletedSpriteOffset = FVector::ZeroVector;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 명시적으로 퀘스트 역할을 지정한 등장 행만 전투를 교체한다.
	virtual void HandleActorPresence(const FNUActorPresenceRow* Step);

private:
	// 플레이어 준비 후 진행 이벤트를 구독하고 월드 표시를 갱신한다.
	void BindQuest();

	UFUNCTION()
	void RefreshState();
	void RefreshGrassRecovery();
	void RefreshStateSprite();

	void UpdateBurrowPresentation();
	void UpdateMovedStoneClear();
	void RestoreBurrowSprite();

	UPROPERTY(Transient)
	FNUBattleEncounterRequest InitialEncounterRequest;
	FName InitialQuestId;
	FName InitialObjectiveId;
	TArray<FName> InitialRequiredObjectiveIds;
	bool bActorPresenceOverrodeQuest = false;

	UNUQuestComponent* FindQuest(AActor* Interactor) const;

	UPROPERTY(VisibleAnywhere, Category = "NU|Quest")
	TObjectPtr<USphereComponent> InteractionVolume;

	UPROPERTY(VisibleAnywhere, Category = "NU|Quest")
	TObjectPtr<UNUQuestMarkerComponent> QuestMarker;

	TWeakObjectPtr<UNUQuestComponent> BoundQuest;

	UPROPERTY(Transient)
	TObjectPtr<UWidgetComponent> GrassAgitationWidget;

	FTimerHandle BindTimer;

	double NextInteractionTime = 0.0;
	FVector BattleStoneRestLocation = FVector::ZeroVector;
	bool bBattleBurrowed = false;
	// 매몰 판정과 분리된 전환 연출. 유지 상태에서는 타이머를 돌리지 않는다.
	TWeakObjectPtr<UPaperFlipbookComponent> BurrowSprite;
	FVector BurrowSpriteRestLocation = FVector::ZeroVector;
	FVector BurrowSpriteRestScale = FVector::OneVector;
	FVector BattleStoneRestScale = FVector::OneVector;
	FVector StoneSlideOffset = FVector::ZeroVector;
	FTimerHandle BurrowPresentationTimer;
	double BurrowPresentationStart = 0.0;
	bool bBurrowSpriteWasVisible = true;
	bool bStoneMovedForEmergence = false;
};
