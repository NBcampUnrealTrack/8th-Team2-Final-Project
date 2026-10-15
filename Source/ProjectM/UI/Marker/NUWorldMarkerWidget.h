#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "NUWorldMarkerWidget.generated.h"

class UImage;
class UTexture2D;
class UWidget;

/**
* 액터 위에 뜨는 마커 위젯의 베이스.
* 어떤 아이콘을 그릴지는 태그별 텍스처 맵으로 WBP에서 정한다.
*/
UCLASS()
class PROJECTM_API UNUWorldMarkerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UNUWorldMarkerWidget(const FObjectInitializer& ObjectInitializer);

	// 태그가 바뀔 때만 호출된다
	void SetMarkerTag(FGameplayTag NewTag);

	// 지도 가장자리에 붙여 그릴 때: 꼭지(MarkerEdgePointer)를 실제 방향(화면 위 = 0도, 시계 방향)으로 돌려 바깥쪽에 둔다.
	// 꼭지가 없는 WBP(머리 위 마커)에는 아무 일도 없다
	void SetEdgeDirection(bool bOnEdge, float AngleDegrees = 0.f);

	UFUNCTION(BlueprintPure, Category = "NU|Marker")
	FGameplayTag GetMarkerTag() const { return MarkerTag; }

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Marker")
	TMap<FGameplayTag, TObjectPtr<UTexture2D>> MarkerTextures;

	// 태그별 색상으로, 등록되지 않은 태그는 흰색을 사용한다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Marker")
	TMap<FGameplayTag, FLinearColor> MarkerTints;

	// 이 태그일 때만 MarkerPulse(퍼지는 고리)를 켠다 --- 보고 가능·목표 지점처럼 지금 가야 할 곳
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Marker")
	FGameplayTagContainer PulseTags;

	// 받을 퀘스트(Offer)에도 고리를 켠다 --- 머리 위 표시처럼 눈에 띄어야 하는 곳에서만 (지도는 끔)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Marker")
	bool bPulseOnOffer = false;

	// 가장자리 꼭지를 마커 중심에서 얼마나 떨어뜨릴지 (마커 로컬 단위)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Marker")
	float EdgePointerDistance = 17.f;

	// MarkerFloat 이 위아래로 떠다니는 폭(위젯 픽셀)과 한 번 오르내리는 시간(초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Marker")
	float FloatAmplitude = 5.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Marker")
	float FloatPeriod = 2.4f;

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Marker", meta = (DisplayName = "On Marker Tag Changed"))
	void BP_OnMarkerTagChanged(FGameplayTag NewTag);

private:
	void RefreshMarkerImage();

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> MarkerImage;

	// 선택: 고리(시간 머티리얼 그림). PulseTags 에 든 태그일 때만 보인다
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> MarkerPulse;

	// 선택: 가장자리 방향 꼭지. SetEdgeDirection 으로만 보인다
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> MarkerEdgePointer;

	// 선택: 머리 위 표시를 통째로 담는 묶음. 천천히 위아래로 떠다니고, 표시가 바뀌면 톡 튀어나온다 (지도 마커에는 없다)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> MarkerFloat;

	FGameplayTag MarkerTag;
	float FloatTime = 0.f;
	float PopTime = -1.f;
};
