// Fill out your copyright notice in the Description page of Project Settings.


#include "Party/UI/NUPartyPromptWidget.h"

#include "Audio/NUSFXSubsystem.h"
#include "Audio/NUSFXTags.h"
#include "Components/Button.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Party/NUPartySubsystem.h"

void UNUPartyPromptWidget::Open(bool bIsInvite, const FString& FromName, int32 ExpireSec)
{
	bIsInvitePrompt = bIsInvite;

	// 시스템 채팅에 나가는 문장과 같은 표현을 사용함
	const FText Name = FText::FromString(FromName);
	PromptMessage = bIsInvite
		? FText::Format(NSLOCTEXT("NUParty", "PromptInvite", "{0}님이 파티에 초대했습니다."), Name)
		: FText::Format(NSLOCTEXT("NUParty", "PromptApply", "{0}님이 파티 가입을 신청했습니다."), Name);

	SetVisibility(ESlateVisibility::Visible);
	UNUSFXSubsystem::Play(this, NU::SFX::Social::PartyInvite);
	if (PromptMessageLine != nullptr)
	{
		PromptMessageLine->SetText(PromptMessage);
	}
	ExpireTotal = ExpireLeft = FMath::Max(ExpireSec, 0);
	if (PromptTimeBar != nullptr)
	{
		PromptTimeBar->SetPercent(1.f);
	}
	if (PromptRoot != nullptr)
	{
		PromptMotion.Add(PromptRoot, 0.f, 0.35f, FVector2D(-18.f, 0.f), 0.f, 0.96f);
		PromptMotion.Start();
	}
	OnPromptOpened(PromptMessage, ExpireSec);
}

void UNUPartyPromptWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (PromptAccept != nullptr)
	{
		PromptAccept->OnClicked.AddUniqueDynamic(this, &ThisClass::Accept);
	}
	if (PromptDecline != nullptr)
	{
		PromptDecline->OnClicked.AddUniqueDynamic(this, &ThisClass::Decline);
	}
}

void UNUPartyPromptWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	PromptMotion.Advance(InDeltaTime);
	// 남은 시간 막대는 표시용 --- 실제 만료는 서브시스템 타이머가 정한다
	if (ExpireTotal > 0.f && PromptTimeBar != nullptr && IsOpen())
	{
		ExpireLeft = FMath::Max(ExpireLeft - InDeltaTime, 0.f);
		PromptTimeBar->SetPercent(ExpireLeft / ExpireTotal);
	}
}

void UNUPartyPromptWidget::Close()
{
	if (!IsOpen())
	{
		return;
	}

	SetVisibility(ESlateVisibility::Collapsed);
	OnPromptClosed();
}

bool UNUPartyPromptWidget::IsOpen() const
{
	return GetVisibility() != ESlateVisibility::Collapsed;
}

void UNUPartyPromptWidget::Accept()
{
	if (UNUPartySubsystem* PartySubsystem = GetPartySubsystem())
	{
		// 창을 여기서 닫지 않음
		// Respond가 ClosePrompt를 거쳐 OnPartyPromptClosed로 돌아오고 PC가 닫음
		PartySubsystem->Respond(true);
	}
}

void UNUPartyPromptWidget::Decline()
{
	if (UNUPartySubsystem* PartySubsystem = GetPartySubsystem())
	{
		PartySubsystem->Respond(false);
	}
}

UNUPartySubsystem* UNUPartyPromptWidget::GetPartySubsystem() const
{
	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UNUPartySubsystem>() : nullptr;
}
