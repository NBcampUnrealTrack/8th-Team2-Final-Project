#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interaction/Interface/NUInteractionOptionProviderInterface.h"
#include "NUDialogueProviderComponent.generated.h"

class UNUDialogueDefinition;

// 기본 대화 외에 기존 하단 대화창 위쪽 선택 영역에 표시할 추가 대화 주제 한 건
USTRUCT(BlueprintType)
struct PROJECTM_API FNUDialogueOptionSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Dialogue|Interaction")
	FName OptionId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Dialogue|Interaction")
	FText DisplayText;

	// 기본 대화는 0이며 추가 주제는 기본적으로 그 뒤에 표시한다. 
	// 값이 작을수록 먼저 표시
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Dialogue|Interaction", meta = (DisplayName = "정렬 순서"))
	int32 SortOrder = 100;

	// 기존 에셋 로드 호환용으로, 정렬에는 사용하지 않는다.
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "SortOrder를 사용하세요."))
	int32 Priority_DEPRECATED = 100;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Dialogue|Interaction")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Dialogue|Interaction")
	FText DisabledReason;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Dialogue|Interaction")
	TObjectPtr<UNUDialogueDefinition> DialogueDefinition;
};

// NPC의 시작 대사와 일반 대화 선택지를 제공하는 Provider
UCLASS(ClassGroup = (NU), meta = (BlueprintSpawnableComponent))
class PROJECTM_API UNUDialogueProviderComponent : public UActorComponent, public INUInteractionOptionProviderInterface
{
	GENERATED_BODY()

public:
	UNUDialogueProviderComponent();

	virtual void CollectInteractionOptions_Implementation(const FNUInteractionContext& Context, TArray<FNUInteractionOption>& OutOptions) const override;
	virtual bool CanExecuteInteraction_Implementation(const FNUInteractionContext& Context, const FNUInteractionOptionHandle& Handle) const override;
	virtual bool ExecuteInteraction_Implementation(const FNUInteractionContext& Context, const FNUInteractionOptionHandle& Handle) override;

	UNUDialogueDefinition* GetInteractionPromptDialogueDefinition() const;

	// NPC의 대표 일반 대화. 지정하면 프로젝트 공용 문구로 선택지를 자동 생성한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Dialogue|Interaction", meta = (
		DisplayName = "기본 대화",
		ToolTip = "NPC의 기본 일반 대화입니다. 지정하면 NU Interaction 프로젝트 설정의 공용 문구로 선택지가 자동 생성됩니다."))
	TObjectPtr<UNUDialogueDefinition> DefaultDialogueDefinition;

	// 선택지 수와 관계없이 기존 하단 대화창에 함께 표시할 "무슨 일로 왔나?" 같은 질문 대사
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Dialogue|Interaction", meta = (
		DisplayName = "상호작용 질문 대사",
		ToolTip = "NPC 상호작용 선택지와 함께 표시할 첫 질문 대사 Dialogue Data Asset입니다."))
	TObjectPtr<UNUDialogueDefinition> MultipleChoicePromptDialogueDefinition;

	// 소문 묻기처럼 기본 대화 외에 이 NPC가 제공하는 추가 대화 주제
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Dialogue|Interaction", meta = (
		DisplayName = "추가 대화 주제",
		ToolTip = "기본 대화 외에 선택 가능한 대화 주제가 있을 때만 추가합니다."))
	TArray<FNUDialogueOptionSpec> DialogueOptions;

protected:
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	const FNUDialogueOptionSpec* FindOption(const FNUInteractionOptionHandle& Handle) const;
	bool IsDefaultDialogueHandle(const FNUInteractionOptionHandle& Handle) const;
	bool IsContextValid(const FNUInteractionContext& Context) const;
};
