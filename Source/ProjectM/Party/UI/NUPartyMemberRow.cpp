// Fill out your copyright notice in the Description page of Project Settings.


#include "Party/UI/NUPartyMemberRow.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UNUPartyMemberRow::SetMember(int32 InSlotNumber, const FNUPartyMember& Member, bool bInIsMe)
{
	SlotNumber = InSlotNumber;
	CharacterIdx = Member.Idx;
	MemberDisplayName = FText::FromString(Member.Name);
	bIsLeader = Member.bIsLeader;
	bIsMe = bInIsMe;

	if (RowNameText != nullptr)
	{
		RowNameText->SetText(MemberDisplayName);
	}
	if (RowLeaderMark != nullptr)
	{
		RowLeaderMark->SetVisibility(bIsLeader ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	OnMemberSet(SlotNumber, MemberDisplayName, bIsLeader, bIsMe);

	// 지금 서버가 체력을 주지 않으므로 1.0으로 세팅
	// 이후엔 조정 필요
	SetHealthPercent(1.0f);
}

void UNUPartyMemberRow::SetNearby(bool bInNearby, int64 InPlayerId)
{
	NearbyPlayerId = bInNearby ? InPlayerId : 0;

	if (bNearby == bInNearby)
	{
		return;   // 표현을 다시 그릴 이유가 없다
	}

	bNearby = bInNearby;
	if (RowBody != nullptr)
	{
		RowBody->SetRenderOpacity(bNearby || bIsMe ? 1.f : 0.55f);
	}
	OnNearbyChanged(bNearby);
}

void UNUPartyMemberRow::SetHealthPercent(float Percent)
{
	if (RowHealthBar != nullptr)
	{
		RowHealthBar->SetPercent(FMath::Clamp(Percent, 0.0f, 1.0f));
	}
	OnHealthChanged(FMath::Clamp(Percent, 0.0f, 1.0f));
}

int64 UNUPartyMemberRow::GetCharacterIdx() const
{
	return CharacterIdx;
}

int64 UNUPartyMemberRow::GetNearbyPlayerId() const
{
	return NearbyPlayerId;
}

bool UNUPartyMemberRow::IsMe() const
{
	return bIsMe;
}

FReply UNUPartyMemberRow::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		OnRowRightClicked.ExecuteIfBound(CharacterIdx);
		return FReply::Handled();
	}

	// 좌클릭은 넘김, 목록이 받아서 창 이동에 사용함
	return FReply::Unhandled();
}
