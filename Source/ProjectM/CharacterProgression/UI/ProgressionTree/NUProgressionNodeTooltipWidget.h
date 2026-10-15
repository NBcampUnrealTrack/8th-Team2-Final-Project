#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CharacterProgression/Data/NUProgressionNodeTypes.h"
#include "NUProgressionNodeTooltipWidget.generated.h"

class UTextBlock;

// 성장 노드에 마우스를 올렸을 때 이름, 유형, 설명만 표시하는 작은 팝업
UCLASS(Abstract)
class PROJECTM_API UNUProgressionNodeTooltipWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetNodeDefinition(const FNUProgressionNodeDefinitionRow& InNode);

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> NodeNameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> NodeTypeText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> NodeDescriptionText;

private:
	void RefreshContent();
	bool TryResolveRewardContent(FText& OutName, FText& OutType, FText& OutDescription) const;
	static FText GetNodeTypeText(const FNUProgressionNodeDefinitionRow& Node);

	FNUProgressionNodeDefinitionRow NodeDefinition;
};
