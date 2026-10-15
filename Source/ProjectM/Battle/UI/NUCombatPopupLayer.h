// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Battle/NUBattleTypes.h"
#include "Blueprint/UserWidget.h"
#include "NUCombatPopupLayer.generated.h"

class UCanvasPanel;
class UNUCombatPopupWidget;


// 전투 팝업 글자들을 담는 화면 전체 레이어
// 팝업마다 위젯을 만들어 전투 아바타 위에 띄우고, 수명이 끝나면 걷어냄
UCLASS(Abstract)
class PROJECTM_API UNUCombatPopupLayer : public UUserWidget
{
	GENERATED_BODY()

public:
	// 아바타 멀티캐스트가 도달한 뒤 PlayerController의 ShowCombatPopup를 호출하고
	// PC의 ShowCombatPopup는 이 함수를 호출함
	// BattleAvatar -> PlayerController -> CombatPopupLayer
	void AddPopup(AActor* AnchorActor, const FNUCombatPopupData& PopupData);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	// 같은 앵커에 이미 떠 있는 팝업 수, 겹치지 않게 밀어낼 때 사용
	int32 CountPopupsForAnchor(const AActor* AnchorActor) const;

	// 만료된 팝업을 제거함
	void RemoveExpiredPopups(float WorldTime);

	// 살아있는 팝업의 위치를 앵커 위로 갱신함
	void UpdatePopupPositions();

	// 팝업을 띄울 월드 위치, 전투 아바타의 머리 위 앵커를 그대로 사용함
	FVector ResolveAnchorWorldLocation(const AActor* AnchorActor) const;

private:
	// 팝업이 들어갈 캔버스
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> PopupCanvas;

	// WBP에서 지정하는 팝업 글자 클래스
	UPROPERTY(EditDefaultsOnly, Category = "NU|UI")
	TSubclassOf<UNUCombatPopupWidget> PopupClass;

	// 앵커 액터 바운드 위로 띄울 높이
	UPROPERTY(EditDefaultsOnly, Category = "NU|UI")
	float AnchorWorldPadding = 40.f;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UNUCombatPopupWidget>> ActivePopups;
};
