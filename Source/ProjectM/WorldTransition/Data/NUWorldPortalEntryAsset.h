#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WorldTransition/Data/NUWorldPortalDefinition.h"
#include "NUWorldPortalEntryAsset.generated.h"

// 서버 장소 ID를 클라이언트 맵과 진입점에 연결한다.
UCLASS(BlueprintType)
class PROJECTM_API UNUWorldPortalEntryAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|WorldTransition", meta = (ShowOnlyInnerProperties))
	FNUWorldPortalEntryBinding Binding;
};
