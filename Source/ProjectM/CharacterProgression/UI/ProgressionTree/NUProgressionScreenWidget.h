#pragma once

#include "CoreMinimal.h"
#include "CharacterProgression/Data/NUProgressionNodeTypes.h"
#include "CharacterProgression/UI/NUProgressionWindowBase.h"
#include "NUProgressionScreenWidget.generated.h"

class UNUProgressionComponent;
class UNUProgressionGraphWidget;
class UNUConfirmDialog;
class UTextBlock;

DECLARE_DELEGATE_OneParam(FOnNUAdvancementSelectionRequested, int32)

// 성장 트리 화면을 열고 노드 선택과 획득 요청을 연결
UCLASS(Abstract)
class PROJECTM_API UNUProgressionScreenWidget : public UNUProgressionWindowBase
{
	GENERATED_BODY()

public:
	void InitializeScreen(
		int32 InGraphId,
		UNUProgressionComponent* InProgression,
		FSimpleDelegate InCloseRequested,
		FOnNUAdvancementSelectionRequested InAdvancementSelectionRequested);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual bool CanDragWindow() const override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UNUProgressionGraphWidget> GraphWidget;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> AvailablePointsText;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Progression|UI")
	FNUProgressionNodeDefinitionRow SelectedNode;

private:
	UFUNCTION()
	void HandleProgressionChanged();
	UFUNCTION()
	void HandleProgressionRequestStateChanged();
	UFUNCTION()
	void HandleProgressionRequestFailed(int32 ResultCode, const FString& Message);

	void HandleConfirmYes();
	void HandleConfirmNo();

	void HandleNodeSelected(const FNUProgressionNodeDefinitionRow& Node, bool bAcquireRequested);
	void ShowNodeConfirmation(const FNUProgressionNodeDefinitionRow& Node);
	void ClearPendingNode();
	void RefreshState();
	FText GetConfirmationMessage(const FNUProgressionNodeDefinitionRow& Node) const;
	static FText GetStatLabel(int32 StatId);

	// 화면이 열려 있는 동안 표시할 그래프와 플레이어 상태
	int32 GraphId = 0;
	TWeakObjectPtr<UNUProgressionComponent> Progression;
	FOnNUAdvancementSelectionRequested AdvancementSelectionRequested;
	FNUProgressionNodeDefinitionRow PendingNode;
	bool bHasPendingNode = false;

	UPROPERTY(Transient)
	TObjectPtr<UNUConfirmDialog> ActiveConfirmWidget;
};
