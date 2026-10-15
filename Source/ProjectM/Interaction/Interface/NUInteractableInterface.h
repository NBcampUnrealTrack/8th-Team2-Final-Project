#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "NUInteractableInterface.generated.h"

UINTERFACE(BlueprintType)
class UNUInteractableInterface : public UInterface
{
	GENERATED_BODY()
};

// 액터의 상호작용 가능 여부 확인 및 상호작용 실행 인터페이스
class PROJECTM_API INUInteractableInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "NU|Interaction")
	bool CanInteract(AActor* Interactor) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "NU|Interaction")
	bool Interact(AActor* Interactor);
};
