// Fill out your copyright notice in the Description page of Project Settings.


#include "Party/UI/NUPartyListWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "CharacterSelect/NUCharacterManager.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Engine/GameInstance.h"
#include "Field/Net/FieldRemoteCharacter.h"
#include "Field/Net/FieldSyncSubsystem.h"
#include "Party/NUPartySubsystem.h"
#include "Party/UI/NUPartyMemberRow.h"
#include "PlayerController/NUPlayerController.h"
#include "UI/NUPlayerContextMenu.h"

void UNUPartyListWidget::NativeConstruct()
{
	Super::NativeConstruct();

	MemberBox = Cast<UVerticalBox>(GetWidgetFromName(TEXT("PartyMemberBox")));
	if (!IsValid(MemberBox))
	{
		UE_LOG(LogTemp, Error, TEXT("[NU] 파티 목록 WBP에 PartyMemberBox가 없습니다"));
		return;
	}

	if (!MemberRowClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[NU] 파티 목록 WBP에 MemberRowClass가 지정되지 않았습니다"));
		return;
	}

	// 프레임이 없으면 창 이동만 못 하고 목록 자체는 동작하므로 경고만
	PartyFrame = GetWidgetFromName(TEXT("PartyFrame"));
	FrameSlot = IsValid(PartyFrame) ? Cast<UCanvasPanelSlot>(PartyFrame->Slot) : nullptr;

	if (!IsValid(FrameSlot))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[NU] 파티 목록 WBP에 PartyFrame이 없습니다 - 창 이동 불가"));
	}

	if (UNUPartySubsystem* PartySubsystem = GetPartySubsystem())
	{
		PartySubsystem->OnPartyChanged.AddUObject(this, &ThisClass::HandlePartyChanged);

		// 필드에 늦게 들어오거나 전투에서 돌아올 때 이미 파티가 있을 수 있음
		RebuildRows(PartySubsystem->GetParty());
	}

	if (UFieldSyncSubsystem* Sync = GetFieldSync())
	{
		Sync->OnRemotePlayerSpawned.AddDynamic(this, &ThisClass::HandleRemoteSpawned);
		Sync->OnRemotePlayerDespawned.AddDynamic(this, &ThisClass::HandleRemoteDespawned);
	}
}

void UNUPartyListWidget::NativeDestruct()
{
	if (UNUPartySubsystem* PartySubsystem = GetPartySubsystem())
	{
		PartySubsystem->OnPartyChanged.RemoveAll(this);
	}

	if (UFieldSyncSubsystem* Sync = GetFieldSync())
	{
		Sync->OnRemotePlayerSpawned.RemoveAll(this);
		Sync->OnRemotePlayerDespawned.RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UNUPartyListWidget::HandlePartyChanged(const FNUPartySnapshot& Snapshot,
	ENUPartyUpdateReason Reason, int64 TargetIdx)
{
	RebuildRows(Snapshot);
}

void UNUPartyListWidget::RebuildRows(const FNUPartySnapshot& Snapshot)
{
	if (!IsValid(MemberBox) || !MemberRowClass)
	{
		return;
	}

	MemberBox->ClearChildren();
	Rows.Reset();

	// 파티가 없으면 목록 자체를 숨김
	if (!Snapshot.IsExistParty() || Snapshot.Members.Num() == 0)
	{
		bHasParty = false;
		RefreshVisibility();
		return;
	}
	
	UGameInstance* GI = GetGameInstance();
	UNUCharacterManager* Characters = GI ? GI->GetSubsystem<UNUCharacterManager>() : nullptr;
	int64 MyIdx = IsValid(Characters) ? Characters->GetSelectedCharacterIdx() : 0;

	UFieldSyncSubsystem* Sync = GetFieldSync();

	for (int32 Index = 0; Index < Snapshot.Members.Num(); ++Index)
	{
		const FNUPartyMember& Member = Snapshot.Members[Index];

		UNUPartyMemberRow* Row = CreateWidget<UNUPartyMemberRow>(this, MemberRowClass);
		if (!IsValid(Row))
		{
			continue;
		}

		Row->SetMember(Index + 1, Member, Member.Idx == MyIdx);
		Row->OnRowRightClicked.BindUObject(this, &ThisClass::HandleRowRightClicked);

		// 파티에 들어온 순간 상대가 이미 시야에 있을 수 있음
		if (Member.Idx == MyIdx)
		{
			// 내 캐릭터는 Remotes에 없으므로 항상 가까이 있는 것으로 설정
			Row->SetNearby(true);
		}
		else
		{
			// 다른 플레이어는 찾아서 근처에 있는지 설정
			AFieldRemoteCharacter* Remote = IsValid(Sync) ? Sync->FindRemoteByCharacterIdx(Member.Idx) : nullptr;
			Row->SetNearby(IsValid(Remote), IsValid(Remote) ? Remote->PlayerId : 0);
		}

		MemberBox->AddChild(Row);
		Rows.Add(Row);
	}

	bHasParty = true;
	RefreshVisibility();
}

void UNUPartyListWidget::SetSuppressed(bool bInSuppressed)
{
	if (bSuppressed == bInSuppressed)
	{
		return;
	}

	bSuppressed = bInSuppressed;
	RefreshVisibility();
}

void UNUPartyListWidget::RefreshVisibility()
{
	// 빈공간은 마우스 클릭 안받게 SelfHitTestInvisible 로
	SetVisibility(bHasParty && !bSuppressed
		? ESlateVisibility::SelfHitTestInvisible
		: ESlateVisibility::Collapsed);
}

void UNUPartyListWidget::HandleRemoteSpawned(AFieldRemoteCharacter* RemotePlayer)
{
	if (!IsValid(RemotePlayer))
	{
		return;
	}

	if (UNUPartyMemberRow* Row = FindRow(RemotePlayer->CharacterIdx))
	{
		Row->SetNearby(true, RemotePlayer->PlayerId);
	}
}

void UNUPartyListWidget::HandleRemoteDespawned(int64 PlayerId)
{
	// 사라진 뒤라 액터로는 character_idx를 알 수 없음
	// 들어올 때 저장해둔 Row의 PlayerId가 나간 플레이어와 일치하는지 찾아서 설정함
	for (UNUPartyMemberRow* Row : Rows)
	{
		if (IsValid(Row) && Row->GetNearbyPlayerId() == PlayerId)
		{
			Row->SetNearby(false);
			return;
		}
	}
}

FReply UNUPartyListWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || !IsValid(FrameSlot))
	{
		return FReply::Unhandled();
	}

	// 창이 메뉴를 두고 움직이면 어긋나므로 컨텍스트 메뉴를 먼저 닫음
	if (ANUPlayerController* PC = Cast<ANUPlayerController>(GetOwningPlayer()))
	{
		PC->ClosePlayerContextMenu();
	}

	bDragging = true;

	// 마우스를 잡아둬야 빠르게 끌어 커서가 창 밖으로 나가도 이동이 안 끊김
	return FReply::Handled().CaptureMouse(TakeWidget());
}

FReply UNUPartyListWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bDragging || !IsValid(FrameSlot) || !IsValid(PartyFrame))
	{
		return FReply::Unhandled();
	}

	// 절대 좌표는 뷰포트 원점이 달라 그대로 못 씀, 이동량만 누적함
	// 커서 delta는 화면 픽셀이고 슬롯 위치는 레이아웃 단위라 DPI 배율로 나눔
	float Scale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), KINDA_SMALL_NUMBER);
	FVector2D Delta = FVector2D(InMouseEvent.GetCursorDelta()) / Scale;

	FVector2D FrameSize = PartyFrame->GetCachedGeometry().GetLocalSize();
	FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportSize(this) / Scale;

	// 창을 화면 밖으로 완전히 빼면 다시 잡을 수 없으므로 안나가게 Clamp
	FVector2D NewPos = FrameSlot->GetPosition() + Delta;
	NewPos.X = FMath::Clamp(NewPos.X, 0.f, FMath::Max(0.f, ViewportSize.X - FrameSize.X));
	NewPos.Y = FMath::Clamp(NewPos.Y, 0.f, FMath::Max(0.f, ViewportSize.Y - FrameSize.Y));

	FrameSlot->SetPosition(NewPos);
	return FReply::Handled();
}

FReply UNUPartyListWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bDragging || InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}

	bDragging = false;
	return FReply::Handled().ReleaseMouseCapture();
}

void UNUPartyListWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	// 캡처를 잃으면 마우스 놓쳤다고 처리
	bDragging = false;

	Super::NativeOnMouseCaptureLost(CaptureLostEvent);
}

void UNUPartyListWidget::HandleRowRightClicked(int64 CharacterIdx)
{
	ANUPlayerController* PC = Cast<ANUPlayerController>(GetOwningPlayer());
	UNUPartySubsystem* PartySubsystem = GetPartySubsystem();
	if (!IsValid(PC) || !IsValid(PartySubsystem))
	{
		return;
	}

	const FNUPartySnapshot& Party = PartySubsystem->GetParty();
	const FString* Name = Party.FindMemberName(CharacterIdx);

	UGameInstance* GI = GetGameInstance();
	UNUCharacterManager* Characters = GI ? GI->GetSubsystem<UNUCharacterManager>() : nullptr;
	int64 MyIdx = IsValid(Characters) ? Characters->GetSelectedCharacterIdx() : 0;

	FNUPlayerContextTarget ContextTarget;
	ContextTarget.CharacterIdx = CharacterIdx;
	ContextTarget.DisplayName = Name ? FText::FromString(*Name) : FText::GetEmpty();
	ContextTarget.ActionMask = CharacterIdx == MyIdx
		? NUPlayerContext::PartySelfActions()
		: NUPlayerContext::PartyMemberActions(Party.IsPartyLeader(MyIdx));
	// 나일경우엔 파티탈퇴,취소 메뉴가뜨고 다른사람일땐 파티 컨텍스트 메뉴가 뜸

	// 시야 밖이면 0으로 남는다. 필드 액터가 필요한 항목만 이 값을 본다
	if (UFieldSyncSubsystem* Sync = GetFieldSync())
	{
		if (AFieldRemoteCharacter* Remote = Sync->FindRemoteByCharacterIdx(CharacterIdx))
		{
			ContextTarget.PlayerId = Remote->PlayerId;
		}
	}

	FVector2D MousePos = FVector2D::ZeroVector;
	PC->GetMousePosition(MousePos.X, MousePos.Y);

	PC->OpenPlayerContextMenu(ContextTarget, MousePos + FVector2D(12.f, 0.f));
}

UNUPartyMemberRow* UNUPartyListWidget::FindRow(int64 CharacterIdx) const
{
	if (CharacterIdx == 0)
	{
		return nullptr;
	}

	for (const TObjectPtr<UNUPartyMemberRow>& Row : Rows)
	{
		if (IsValid(Row) && Row->GetCharacterIdx() == CharacterIdx)
		{
			return Row.Get();
		}
	}

	return nullptr;
}

UNUPartySubsystem* UNUPartyListWidget::GetPartySubsystem() const
{
	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UNUPartySubsystem>() : nullptr;
}

UFieldSyncSubsystem* UNUPartyListWidget::GetFieldSync() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UFieldSyncSubsystem>() : nullptr;
}
