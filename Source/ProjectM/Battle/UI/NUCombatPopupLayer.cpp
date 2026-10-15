// Fill out your copyright notice in the Description page of Project Settings.


#include "NUCombatPopupLayer.h"

#include "Battle/Avatar/NUBattleAvatar.h"
#include "Battle/UI/NUCombatPopupWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/World.h"


void UNUCombatPopupLayer::NativeConstruct()
{
	Super::NativeConstruct();

	PopupCanvas = Cast<UCanvasPanel>(GetWidgetFromName(TEXT("PopupCanvas")));
	if (!IsValid(PopupCanvas))
	{
		UE_LOG(LogTemp, Error, TEXT("[NU] 전투 팝업 레이어 WBP에 PopupCanvas가 없습니다"));
	}

	if (!PopupClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[NU] 전투 팝업 레이어 WBP에 PopupClass가 지정되지 않았습니다"));
	}
}

void UNUCombatPopupLayer::AddPopup(AActor* AnchorActor, const FNUCombatPopupData& PopupData)
{
	if (!IsValid(AnchorActor) || !IsValid(PopupCanvas) || !PopupClass)
	{
		return;
	}

	UNUCombatPopupWidget* Popup = CreateWidget<UNUCombatPopupWidget>(GetOwningPlayer(), PopupClass);
	if (!IsValid(Popup))
	{
		return;
	}

	// 같은 아바타에 이미 떠 있는 만큼 위로 밀어 올려 겹치지 않게 함
	int32 StackIndex = CountPopupsForAnchor(AnchorActor);

	if (UCanvasPanelSlot* PopupSlot = PopupCanvas->AddChildToCanvas(Popup))
	{
		// 글자 크기에 맞춰 자동, 앵커는 좌상단 기준으로 두고 위치만 매 프레임 갱신함
		PopupSlot->SetAutoSize(true);
		PopupSlot->SetAnchors(FAnchors(0.f, 0.f));

		// 투영 지점을 글자 가운데 아래로 맞춤
		PopupSlot->SetAlignment(FVector2D(0.5f, 1.f));
	}

	Popup->Setup(AnchorActor, PopupData, StackIndex);

	ActivePopups.Add(Popup);

	// 생성 직후 한번 그려줌, 위치 조정
	UpdatePopupPositions();
}

void UNUCombatPopupLayer::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (ActivePopups.IsEmpty()) return;

	UWorld* World = GetWorld();
	if (!IsValid(World)) return;

	RemoveExpiredPopups(World->GetTimeSeconds());

	// 아바타가 공격 모션으로 움직이고 전투 카메라도 전환되므로 매 프레임 따라가야 함
	UpdatePopupPositions();
}

int32 UNUCombatPopupLayer::CountPopupsForAnchor(const AActor* AnchorActor) const
{
	int32 Count = 0;

	for (const TObjectPtr<UNUCombatPopupWidget>& Popup : ActivePopups)
	{
		if (IsValid(Popup) && Popup->GetAnchorActor() == AnchorActor)
		{
			++Count;
		}
	}

	return Count;
}

void UNUCombatPopupLayer::RemoveExpiredPopups(float WorldTime)
{
	for (int32 Index = ActivePopups.Num() - 1; Index >= 0; --Index)
	{
		UNUCombatPopupWidget* Popup = ActivePopups[Index];

		// 이미 유효하지 않으면 배열에서 제거하고 넘어감
		if (!IsValid(Popup))
		{
			ActivePopups.RemoveAt(Index);
			continue;
		}

		// 만료되지 않았다면 넘어감
		if (!Popup->IsExpired(WorldTime)) continue;

		Popup->RemoveFromParent();
		ActivePopups.RemoveAt(Index);
	}
}

void UNUCombatPopupLayer::UpdatePopupPositions()
{
	for (const TObjectPtr<UNUCombatPopupWidget>& Popup : ActivePopups)
	{
		if (!IsValid(Popup)) continue;

		AActor* Anchor = Popup->GetAnchorActor();

		// 앵커역할을 하는 액터가 유효하지 않다면 숨기기
		// 수명이 정해져 있어서 알아서 사라짐
		if (!IsValid(Anchor))
		{
			Popup->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}

		FVector PopupWorldLocation = ResolveAnchorWorldLocation(Anchor);

		FVector2D ScreenPosition;
		bool bProjected = UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
			GetOwningPlayer(), PopupWorldLocation, ScreenPosition, true);

		// 화면 밖이면 그리지 않음
		if (!bProjected)
		{
			Popup->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}

		Popup->SetVisibility(ESlateVisibility::HitTestInvisible);

		if (UCanvasPanelSlot* PopupSlot = Cast<UCanvasPanelSlot>(Popup->Slot))
		{
			PopupSlot->SetPosition(ScreenPosition + Popup->GetStackOffset());
		}
	}
}

FVector UNUCombatPopupLayer::ResolveAnchorWorldLocation(const AActor* AnchorActor) const
{
	// 전투 아바타는 머리 위 앵커를 이미 갖고 있음, 체력바가 사용하는 위치
	if (const ANUBattleAvatar* Avatar = Cast<const ANUBattleAvatar>(AnchorActor))
	{
		if (const USceneComponent* AnchorComponent = Avatar->GetTargetIndicatorAnchor())
		{
			return AnchorComponent->GetComponentLocation() + FVector(0.f, 0.f, AnchorWorldPadding);
		}
	}

	// 그 외 액터는 바운드 위로 올림
	// 첫 인자(bOnlyCollidingComponents)를 true로 두면 안 됨
	// 전투 아바타는 Sprite와 VitalBarWidget이 모두 NoCollision이라 바운드가 비어
	// 원점(0,0,0)이 나오고 팝업이 화면 밖으로 가버림, 전투 위치는 Arena 위치라 현재 20000.f 에 있음
	FVector BoundsOrigin;
	FVector BoundsExtent;
	AnchorActor->GetActorBounds(false, BoundsOrigin, BoundsExtent);

	return BoundsOrigin + FVector(0.f, 0.f, BoundsExtent.Z + AnchorWorldPadding);
}
