#pragma once

#include "CoreMinimal.h"
#include "Interaction/Data/NUInteractionTypes.h"
#include "UObject/Interface.h"
#include "NUInteractionOptionProviderInterface.generated.h"

UINTERFACE(BlueprintType)
class PROJECTM_API UNUInteractionOptionProviderInterface : public UInterface
{
	GENERATED_BODY()
};

// NPC나 상호작용 오브젝트에 붙은 퀘스트, 대화, 상점 등의 기능 컴포넌트가
// 자신이 제공할 수 있는 선택지를 Router에 전달,
// 플레이어가 선택한 기능을 실행하기 위한 인터페이스
class PROJECTM_API INUInteractionOptionProviderInterface
{
	GENERATED_BODY()

public:
	// 기존 배열을 비우지 않고 이 Provider가 제공하는 선택지만 OutOptions에 추가
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "NU|Interaction|Provider")
	void CollectInteractionOptions(const FNUInteractionContext& Context, UPARAM(ref) TArray<FNUInteractionOption>& OutOptions) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "NU|Interaction|Provider")
	bool CanExecuteInteraction(const FNUInteractionContext& Context, const FNUInteractionOptionHandle& Handle) const;

	// Handle이 가리키는 대화, 퀘스트, 상점 등의 기능 실행
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "NU|Interaction|Provider")
	bool ExecuteInteraction(const FNUInteractionContext& Context, const FNUInteractionOptionHandle& Handle);
};
