#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "GameplayTagContainer.h"
#include "NUWorldMarkerComponent.generated.h"

/** 월드의 깊이와 가림 판정을 따르는 아이콘. 용도별 표시는 태그로 지정한다. */
UCLASS(ClassGroup = (NU), meta = (BlueprintSpawnableComponent))
class PROJECTM_API UNUWorldMarkerComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	UNUWorldMarkerComponent();
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;

	// 표시할 아이콘 종류. 무효 태그를 넣으면 마커를 숨긴다
	UFUNCTION(BlueprintCallable, Category = "NU|Marker")
	void SetMarkerTag(FGameplayTag NewTag);

	UFUNCTION(BlueprintPure, Category = "NU|Marker")
	FGameplayTag GetMarkerTag() const { return MarkerTag; }

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

	// HeadMarkerGap 이 켜져 있으면 주인의 스프라이트 꼭대기 위로 높이를 맞춘다 (크기 보정·애니메이션 뒤에도 맞도록 가끔 다시 잰다)
	void PlaceAboveOwnerSprite();

	// 데디케이티드 서버에서는 마커를 만들지 않는다
	bool IsMarkerEnabled() const;

	// UI 설정(NU UI > 머리 위 표시)에 위젯이 지정돼 있으면 그 위젯·재질·크기로 바꾼다. 위젯을 만들기 전(BeginPlay 앞)에 부른다
	void ApplyHeadMarkerSettings();

	UPROPERTY(EditDefaultsOnly, Category = "NU|Marker")
	TObjectPtr<UMaterialInterface> MarkerMaterial;

private:
	FGameplayTag MarkerTag;
	float PlaceTimer = 0.f;
};
