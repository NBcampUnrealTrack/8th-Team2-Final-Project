// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Map/NU_WorldMapWindowWidget.h"

#include "Audio/NUSFXTags.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"
#include "UI/NUUIManager.h"
#include "UI/NUUISettings.h"

UNU_WorldMapWindowWidget::UNU_WorldMapWindowWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCloseOnBattleStart = false;
}

UNU_WorldMapWindowWidget* UNU_WorldMapWindowWidget::Show(APlayerController* OwningPlayer)
{
	TSubclassOf<UNU_WorldMapWindowWidget> WindowClass = GetDefault<UNUUISettings>()->WorldMapWindowClass.LoadSynchronous();
	if (OwningPlayer == nullptr || WindowClass == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[지도] WorldMapWindowClass 미지정 --- 프로젝트 세팅 > 게임 > NU UI"));
		return nullptr;
	}

	UNU_WorldMapWindowWidget* Window = CreateWidget<UNU_WorldMapWindowWidget>(OwningPlayer, WindowClass);
	if (Window == nullptr)
	{
		return nullptr;
	}

	if (UNUUIManager* UI = UNUUIManager::Get(OwningPlayer))
	{
		FNUUIWindowParams Params;
		Params.Layer = NUUILayer::Window;
		Params.bDimBackground = true;
		Params.bBlockGameInput = true; // 화면을 덮는 창 --- 뒤에서 캐릭터가 걷지 않게
		Params.OpenSFX = NU::SFX::UI::MapOpen; // 종이 지도를 펼치고 접는 소리
		Params.CloseSFX = NU::SFX::UI::MapClose;
		UI->OpenWindow(Window, Params, FSimpleDelegate::CreateUObject(Window, &UNU_WorldMapWindowWidget::Close));
	}
	else
	{
		Window->AddToViewport(NUUILayer::Window);
	}
	return Window;
}

void UNU_WorldMapWindowWidget::Close()
{
	if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
	{
		UI->CloseWindow(this); // 미등록이면 무시 --- 아래 CloseSheet 와 겹쳐도 안전 (옵션창과 같은 순서)
	}
	CloseSheet();
}

void UNU_WorldMapWindowWidget::HandleEncounterChanged(ANUBattleEncounter* NewEncounter)
{
	Super::HandleEncounterChanged(NewEncounter);

	// 초기 전투 상태 통지는 NativeConstruct 도중에도 와서 바로 닫으면 구축 중인 위젯을 부순다 --- 다음 틱에 닫는다
	if (NewEncounter != nullptr && GetWorld() != nullptr)
	{
		GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]() { Close(); }));
	}
}
