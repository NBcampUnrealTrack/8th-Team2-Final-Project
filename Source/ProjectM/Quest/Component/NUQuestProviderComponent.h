#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Interaction/Interface/NUInteractionOptionProviderInterface.h"
#include "Quest/Data/NUQuestTypes.h"
#include "NUQuestProviderComponent.generated.h"

// 퀘스트 관련 상호작용 선택지를 제공하고 선택 결과를 퀘스트 진행에 연결하는 Provider

class UNUQuestComponent;
class UNUQuestDefinition;
class UNUQuestDialogueDefinition;

UCLASS(ClassGroup = (NU), meta = (BlueprintSpawnableComponent))
class PROJECTM_API UNUQuestProviderComponent : public UActorComponent, public INUInteractionOptionProviderInterface
{
	GENERATED_BODY()

public:	
	UNUQuestProviderComponent();

	virtual void CollectInteractionOptions_Implementation(const FNUInteractionContext& Context, TArray<FNUInteractionOption>& OutOptions) const override;
	virtual bool CanExecuteInteraction_Implementation(const FNUInteractionContext& Context, const FNUInteractionOptionHandle& Handle) const override;
	virtual bool ExecuteInteraction_Implementation(const FNUInteractionContext& Context, const FNUInteractionOptionHandle& Handle) override;

// -------------------------------------------------------------------------
// 상호작용
// -------------------------------------------------------------------------
public:
	// 액터의 Interact()에서 호출
	UFUNCTION(BlueprintCallable, Category = "NU|Quest|Interaction")
	bool HandleInteraction(AActor* Interactor);

	bool HandleDialogueFinished(AActor* Interactor, const FNUQuestInteractionOption& Option);

	// 수락 직후 이 액터와의 대화가 진행 가능한 목표면 선택지를 다시 고르지 않고 이어서 진행한다.
	bool ContinueAcceptedQuest(AActor* Interactor, FName QuestId);

	// 이 액터와 엮인 퀘스트들의 단계. 진행 중 목표 대상도 QuestId를 유지해 포함한다.
	// 상태를 바꾸지 않으므로 클라이언트에서도 호출할 수 있다.
	UFUNCTION(BlueprintPure, Category = "NU|Quest|Interaction")
	ENUQuestInteractionPhase GetOfferedQuestPhase(AActor* Interactor) const;

	// 진행 중인 퀘스트의 목표 대상인지
	UFUNCTION(BlueprintPure, Category = "NU|Quest|Interaction")
	bool IsActiveObjectiveTarget(AActor* Interactor) const;

	// 현재 노출 가능한 퀘스트 단계 중 가장 높은 우선순위. 마커와 대사 선택이 공유한다.
	// TurnIn > Offer > Target > InProgress > Locked > Completed > Idle
	UFUNCTION(BlueprintPure, Category = "NU|Quest|Interaction")
	ENUQuestInteractionPhase GetInteractionPhase(AActor* Interactor) const;

	// 이 액터가 지금 처리할 수 있는 퀘스트와 그 단계
	UFUNCTION(BlueprintPure, Category = "NU|Quest|Interaction")
	TArray<FNUQuestInteractionOption> CollectQuestInteractionOptions(AActor* Interactor) const;

	void ResolveInteractionTarget(AActor* Interactor, FNUQuestInteractionOption& OutOption) const;

	bool IsInteractionOptionValid(AActor* Interactor, const FNUQuestInteractionOption& Option) const;

	// NPC별 대사 오버라이드를 먼저 찾고, 없으면 CSV 퀘스트 정의의 공용 대사를 사용한다.
	UNUQuestDialogueDefinition* SelectDialogueDefinition(const FNUQuestInteractionOption& Option) const;

private:
	// QuestId 하나에 대한 단계. 제안용·완료용을 같은 규칙으로 평가한다
	ENUQuestInteractionPhase ResolvePhaseForQuest(const UNUQuestComponent* QuestComponent, FName QuestId) const;

	bool HandleQuestInteraction(AActor* Interactor, const FNUQuestInteractionOption& Option);
	bool GrantConfiguredTargetItem(AActor* Interactor) const;

	bool RequestDialogue(AActor* Interactor, const FNUQuestInteractionOption& Option) const;

	// 대사 표(NPCDialogue)에서 이 NPC의 이 단계 대사를 찾는다. NPC가 아니거나 없으면 None.
	FName FindTableDialogue(const FNUQuestInteractionOption& Option) const;

	// 고르면 들려줄 대사가 있는지. 대사 표와 대사 에셋을 RequestDialogue와 같은 순서로 본다.
	bool HasDialogueFor(const FNUQuestInteractionOption& Option) const;

	bool IsProviderOptionVisible(AActor* Interactor, const FNUQuestInteractionOption& Option) const;
	bool ResolveProviderOption(AActor* Interactor, const FNUInteractionOptionHandle& Handle, FNUQuestInteractionOption& OutOption) const;
	FNUInteractionOptionHandle MakeProviderHandle(const FNUQuestInteractionOption& Option) const;

	// 퀘스트를 변경할 때 사용
	// 서버 권한에서만 유효
	UNUQuestComponent* FindQuestComponent(AActor* Interactor) const;

	// 퀘스트 조회만 할 때 사용
	// 권한을 검사하지 않아, 클라이언트 표시 경로에서 사용 가능
	UNUQuestComponent* FindQuestComponentForQuery(AActor* Interactor) const;

	const class UNUQuestDataSubsystem* FindQuestDataSubsystem() const;

	// QuestId로 퀘스트 정의 조회
	const UNUQuestDefinition* FindQuestDefinitionById(FName QuestId) const;

	// 퀘스트 제공, UI 제안, 반복 처리
	bool HandleOfferedQuest(UNUQuestComponent& QuestComponent, const UNUQuestDefinition& Definition);

	// 목표를 다 채운 퀘스트의 완료 보고 처리
	bool HandleTurnInQuest(UNUQuestComponent& QuestComponent, const UNUQuestDefinition& Definition);

	// 퀘스트 목표 이벤트 전달
	bool SendObjectiveEvent(UNUQuestComponent& QuestComponent) const;
	FName GetObjectiveSourceInstanceId() const;

// -------------------------------------------------------------------------
// 퀘스트 설정
// -------------------------------------------------------------------------
public:
	// 이 액터의 ID. 퀘스트의 OfferActorId / TurnInActorId / 목표 TargetId가 이 값을 가리킨다.
	// 비어 있으면 퀘스트와 아무 관계도 맺지 않는다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Interaction")
	FName QuestActorId = NAME_None;

	// 상호작용할 때 발행할 목표 타입
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Interaction", meta = (Categories = "NU.Quest.Objective"))
	FGameplayTag ObjectiveType;

	// 한 번의 상호작용으로 증가할 수치
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Interaction", meta = (ClampMin = "1", UIMin = "1"))
	int32 EventAmount = 1;

	// 기존 에셋 로드 호환용. 정렬에는 사용하지 않는다.
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "상호작용 그룹 정렬 정책으로 대체되었습니다."))
	int32 InteractionOptionPriority_DEPRECATED = 400;

	// 진행 중인 퀘스트 목표로 이 액터를 상호작용할 때 표시할 메뉴 문구
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Interaction")
	FText ObjectiveInteractionLabel;

	// 이 액터가 Target 단계일 때만 사용하는 개별 대사
	// 비어 있으면 CSV 퀘스트 정의의 공용 Target 대사를 사용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Interaction|Dialogue")
	TObjectPtr<UNUQuestDialogueDefinition> TargetDialogueOverride;

	// 같은 퀘스트라도 NPC마다 다른 단계 대사를 사용할 때 QuestId로 지정한다.
	// 지정한 에셋에 현재 단계 대사가 없으면 CSV 퀘스트 정의의 공용 대사로 폴백한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Interaction|Dialogue")
	TMap<FName, TObjectPtr<UNUQuestDialogueDefinition>> QuestDialogueOverrides;

	// 동일 퀘스트에서 같은 NPC와 여러 번 대화할 때 목표별 대사를 선택한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Interaction|Dialogue")
	TMap<FName, TObjectPtr<UNUQuestDialogueDefinition>> ObjectiveDialogueOverrides;

	// 같은 BP를 여러 번 배치했을 때 각 배치 액터가 목표 진행에 한 번만 반영
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Interaction")
	bool bCountEachActorOnce = false;

	// CollectItem 목표 오브젝트가 대화 종료 뒤 실제 인벤토리 아이템을 지급할 때 사용
	// 비워 두면 기존처럼 ObjectiveType/QuestActorId 이벤트만 보낸다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Interaction|Target Item")
	FName GrantedItemRowName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Quest|Interaction|Target Item",
		meta = (ClampMin = "1", UIMin = "1"))
	int32 GrantedItemCount = 1;
};
