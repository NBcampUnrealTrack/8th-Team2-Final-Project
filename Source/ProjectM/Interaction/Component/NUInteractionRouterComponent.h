#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interaction/Data/NUInteractionTypes.h"
#include "NUInteractionRouterComponent.generated.h"

// 액터의 Provider에서 선택지를 수집하고 실행할 Provider로 요청을 전달하는 컴포넌트
UCLASS(ClassGroup = (NU), meta = (BlueprintSpawnableComponent))
class PROJECTM_API UNUInteractionRouterComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNUInteractionRouterComponent();

	// 모든 Provider의 선택지를 수집한 뒤 중복을 제거하고 우선순위에 따라 정렬
	UFUNCTION(BlueprintCallable, Category = "NU|Interaction|Router")
	void CollectInteractionOptions(const FNUInteractionContext& Context, UPARAM(ref) TArray<FNUInteractionOption>& OutOptions) const;

	// 실제 기능 0개면 실패하고 Policy에 따라 단일 선택지를 실행하거나 UI에 제공합니다.
	UFUNCTION(BlueprintCallable, Category = "NU|Interaction|Router")
	ENUInteractionRouteResult RouteInteraction(const FNUInteractionContext& Context, UPARAM(ref) TArray<FNUInteractionOption>& OutOptions, ENUInteractionRoutePolicy Policy = ENUInteractionRoutePolicy::AutoExecuteSingle);

	// 대화창에서 선택한 Handle을 수집하고 해당 Provider에서 실행
	UFUNCTION(BlueprintCallable, Category = "NU|Interaction|Router")
	bool ExecuteInteractionOption(const FNUInteractionContext& Context, const FNUInteractionOptionHandle& Handle);
};
