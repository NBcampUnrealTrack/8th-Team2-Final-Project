#include "Trade/NUTradeSubsystem.h"

#include "Audio/NUSFXSubsystem.h"
#include "Audio/NUSFXTags.h"
#include "CharacterState/NUPlayerState.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Field/Net/FieldRemoteCharacter.h"
#include "Field/Net/FieldSyncSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/NUInventoryComponent.h"
#include "SRevGame/GameMessage.h"
#include "TimerManager.h"
#include "Trade/UI/NUTradeUIController.h"
#include "UI/NUUIManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogNUTrade, Log, All);

namespace
{
	constexpr int32 NUTradeResultOk = 200;
}

void UNUTradeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Collection.InitializeDependency<UEmberServerManager>();
	if (UEmberServerManager* Server = GetServer())
	{
		Server->OnStateChanged.AddDynamic(this, &UNUTradeSubsystem::HandleServerStateChanged);
	}

#if !UE_BUILD_SHIPPING
	RegisterDebugCommands();
#endif
}

void UNUTradeSubsystem::Deinitialize()
{
#if !UE_BUILD_SHIPPING
	IConsoleManager& Console = IConsoleManager::Get();
	
	TArray<IConsoleObject*> StillRegistered;
	// nu. 으로 시작하는 콘솔 명령 수집
	Console.ForEachConsoleObjectThatStartsWith(
		FConsoleObjectVisitor::CreateLambda(
			[&StillRegistered](const TCHAR* /*Name*/, IConsoleObject* Object)
			{
				StillRegistered.Add(Object);
			}),
		TEXT("nu."));
	
	for (IConsoleCommand* Command : DebugCommands)
	{
		if (Command && StillRegistered.Contains(Command))
		{
			Console.UnregisterConsoleObject(Command);
		}
	}
	DebugCommands.Reset();
#endif

	if (UEmberServerManager* Server = GetServer())
	{
		Server->OnStateChanged.RemoveDynamic(this, &UNUTradeSubsystem::HandleServerStateChanged);
	}

	ResetSession();
	Super::Deinitialize();
}

void UNUTradeSubsystem::HandleServerStateChanged(EEmberConnState NewState)
{
	// 세션이 끊긴 채로 남은 거래는 무효 --- 창은 매니저(bCloseOnSessionEnd)가, 상태는 여기서 정리한다
	if (NewState != EEmberConnState::LoggedIn && IsTrading())
	{
		HandleTradeEnded(static_cast<int32>(ENUTradeEndReason::Disconnected));
	}
}

#if !UE_BUILD_SHIPPING
void UNUTradeSubsystem::RegisterDebugCommands()
{
	IConsoleManager& Console = IConsoleManager::Get();

	// 서버 없이 거래 창을 띄운다 (상대는 더미)
	DebugCommands.Add(Console.RegisterConsoleCommand(
		TEXT("nu.Trade.Open"),
		TEXT("[개발] 더미 상대와 거래 창 열기: nu.Trade.Open [상대이름]"),
		FConsoleCommandWithArgsDelegate::CreateWeakLambda(this, [this](const TArray<FString>& Args)
		{
			EnsureUIController();
			PartnerIdx = 1000000001;
			PartnerName = Args.IsValidIndex(0) ? Args[0] : TEXT("테스트상대");
			MyOffer = FNUTradeOffer();
			PartnerOffer = FNUTradeOffer();
			MyOffer.Slots.SetNum(NUTrade::SlotCount);
			PartnerOffer.Slots.SetNum(NUTrade::SlotCount);
			SetState(ENUTradeState::Active);
			OnTradeOfferChanged.Broadcast();
		}),
		ECVF_Default));

	// 상대가 나에게 거래를 신청한 상황 (수락/거절 창 확인용)
	DebugCommands.Add(Console.RegisterConsoleCommand(
		TEXT("nu.Trade.Invite"),
		TEXT("[개발] 더미 상대의 거래 신청 받기: nu.Trade.Invite [상대이름]"),
		FConsoleCommandWithArgsDelegate::CreateWeakLambda(this, [this](const TArray<FString>& Args)
		{
			const FString Name = Args.IsValidIndex(0) ? Args[0] : TEXT("테스트상대");
			HandleTradeRequested(1000000001, Name, NUTrade::InviteExpireSec);
		}),
		ECVF_Default));

	// 상대가 아이템을 올린 상황을 흉내낸다 (내 인벤 칸을 상대 제안에 복사)
	DebugCommands.Add(Console.RegisterConsoleCommand(
		TEXT("nu.Trade.PartnerAdd"),
		TEXT("[개발] 상대 제안에 내 인벤 칸을 복사: nu.Trade.PartnerAdd <거래칸> <인벤칸>"),
		FConsoleCommandWithArgsDelegate::CreateWeakLambda(this, [this](const TArray<FString>& Args)
		{
			const int32 TradeSlot = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 0;
			const int32 InvSlotNo = Args.IsValidIndex(1) ? FCString::Atoi(*Args[1]) : 0;
			if (!PartnerOffer.Slots.IsValidIndex(TradeSlot))
			{
				return;
			}
			if (const UNUInventoryComponent* Inventory = GetLocalInventory())
			{
				if (const FNUItemInstance* Item = Inventory->FindBySlotNo(InvSlotNo))
				{
					PartnerOffer.Slots[TradeSlot].ItemRowName = Item->ItemRowName;
					PartnerOffer.Slots[TradeSlot].Count = Item->Count;
					PartnerOffer.Slots[TradeSlot].InvSlotNo = InvSlotNo;
					OnTradeOfferChanged.Broadcast();
				}
			}
		}),
		ECVF_Default));

	// 상대가 잠갔다고 가정 (확정 버튼 활성 조건 확인용)
	DebugCommands.Add(Console.RegisterConsoleCommand(
		TEXT("nu.Trade.PartnerLock"),
		TEXT("[개발] 상대 잠금 토글"),
		FConsoleCommandDelegate::CreateWeakLambda(this, [this]()
		{
			PartnerOffer.bLocked = !PartnerOffer.bLocked;
			UE_LOG(LogNUTrade, Log, TEXT("상대 잠금 = %s"), PartnerOffer.bLocked ? TEXT("ON") : TEXT("OFF"));
			OnTradeOfferChanged.Broadcast();
		}),
		ECVF_Default));
}
#endif

/////////////////////////////////////////////////////////////////
// 송신 --- 요청일 뿐이고, 상태는 서버 푸시로만 바뀐다
/////////////////////////////////////////////////////////////////

void UNUTradeSubsystem::RequestTrade(int64 TargetIdx)
{
	if (TargetIdx == 0 || IsTrading())
	{
		return;
	}

	UEmberServerManager* Server = GetServer();
	if (Server == nullptr || !Server->IsLoggedIn())
	{
		UE_LOG(LogNUTrade, Warning, TEXT("거래 신청 실패 --- 서버 미접속"));
		return;
	}

	// 내가 전투 중이면 애초에 거래를 시작할 수 없다
	if (IsInBattle())
	{
		ShowNotice(NSLOCTEXT("NUTrade", "RequestSelfInBattle", "전투 중에는 거래를 신청할 수 없습니다."));
		return;
	}

	// 상대가 전투 중이면 상대 클라가 자동으로 거절한다 --- 그 왕복이 "상대가 거래를 거절했습니다"로
	// 보여 직접 거절한 것과 구분이 안 된다. 주변 활동(SVR_Field_Activity)은 이미 받고 있으니
	// 보내기 전에 여기서 알린다. (AoI 밖 등으로 상태를 모르면 그냥 보낸다 --- 기존 거절 흐름이 받친다)
	if (const UWorld* World = GetWorld())
	{
		const UFieldSyncSubsystem* Sync = World->GetSubsystem<UFieldSyncSubsystem>();
		const AFieldRemoteCharacter* Target = IsValid(Sync) ? Sync->FindRemoteByCharacterIdx(TargetIdx) : nullptr;
		if (IsValid(Target) && Target->Activity == EFieldActivity::Battle)
		{
			UE_LOG(LogNUTrade, Log, TEXT("거래 신청 차단 --- 상대가 전투 중 [Idx=%lld]"), TargetIdx);
			ShowNotice(NSLOCTEXT("NUTrade", "TargetInBattle", "상대가 전투 중이라 거래를 신청할 수 없습니다."));
			return;
		}
	}

	EnsureUIController();
	PartnerIdx = TargetIdx;
	PartnerName.Empty(); // 이름은 SVR_Trade_Started로 온다

	Server->SendTradeRequest(TargetIdx);
	SetState(ENUTradeState::Requesting);
}

void UNUTradeSubsystem::RespondTrade(bool bAccept)
{
	if (State != ENUTradeState::Invited)
	{
		return;
	}

	if (UEmberServerManager* Server = GetServer())
	{
		Server->SendTradeRespond(bAccept, PartnerIdx);
	}

	// 수락이면 SVR_Trade_Started를, 거절이면 SVR_Trade_Ended를 기다린다.
	// 거절은 상대에게만 푸시가 가므로 내 상태는 여기서 접는다
	if (!bAccept)
	{
		ResetSession();
		SetState(ENUTradeState::Idle);
	}
}

void UNUTradeSubsystem::SetTradeItem(int32 TradeSlot, int32 InvSlotNo)
{
	if (State != ENUTradeState::Active || !MyOffer.Slots.IsValidIndex(TradeSlot))
	{
		return;
	}

	// 잠금 중에는 제안을 바꿀 수 없다 --- 서버가 양쪽 잠금을 풀어준 뒤에 다시 올린다
	if (MyOffer.bLocked)
	{
		UE_LOG(LogNUTrade, Log, TEXT("잠금 중에는 제안을 바꿀 수 없음"));
		ShowNotice(NSLOCTEXT("NUTrade", "LockedNoChange", "잠금을 해제해야 아이템을 바꿀 수 있습니다."));
		return;
	}

	if (UEmberServerManager* Server = GetServer())
	{
		// 반영은 SVR_Trade_Updated로 온다 (서버가 진실)
		Server->SendTradeSetItem(TradeSlot, InvSlotNo);
	}
}

void UNUTradeSubsystem::SetLock(bool bLock)
{
	if (State != ENUTradeState::Active)
	{
		return;
	}

	if (UEmberServerManager* Server = GetServer())
	{
		Server->SendTradeLock(bLock);
	}

	// 내가 푼 해제인지 기억해둔다 --- 서버가 상대 때문에 풀어준 것과 갈라서 안내한다
	bUnlockRequestedByMe = !bLock;

	UE_LOG(LogNUTrade, Log, TEXT("잠금 요청 = %s"), bLock ? TEXT("ON") : TEXT("OFF"));
}

void UNUTradeSubsystem::ConfirmTrade()
{
	if (!CanConfirm())
	{
		return;
	}

	if (UEmberServerManager* Server = GetServer())
	{
		// offer_ver를 안 채우면 서버가 1810으로 거절한다
		Server->SendTradeConfirm(OfferVer);
	}
	UE_LOG(LogNUTrade, Log, TEXT("거래 확정 요청 [offer_ver=%d]"), OfferVer);
}

void UNUTradeSubsystem::CancelTrade()
{
	if (!IsTrading())
	{
		return;
	}

	// 신청 단계는 아직 거래가 아니라 서버에 보낼 것이 없다
	if (State == ENUTradeState::Invited)
	{
		RespondTrade(false);
		return;
	}

	// 종료 푸시가 왔을 때 "내가 눌렀는지"를 구분하려고 기억해둔다
	bCancelRequestedByMe = true;

	if (UEmberServerManager* Server = GetServer())
	{
		Server->SendTradeCancel();
	}
	UE_LOG(LogNUTrade, Log, TEXT("거래 취소 요청"));
}

/////////////////////////////////////////////////////////////////
// 수신 --- EmberServerManager가 넘겨준다
/////////////////////////////////////////////////////////////////

void UNUTradeSubsystem::HandleTradeAck(int32 ResultCode)
{
	if (ResultCode == NUTradeResultOk)
	{
		return;
	}

	UE_LOG(LogNUTrade, Warning, TEXT("거래 요청 거절 [코드=%d]"), ResultCode);
	OnTradeRequestFailed.Broadcast(ResultCode);

	// 거래가 시작되기 전 단계(신청 중·초대 받음)에서 거절당했으면 여기서 풀어준다.
	// 거래 세션이 선 뒤(Active·Confirming)라면 서버가 종료 푸시로 정리하므로 건드리지 않는다.
	// Invited를 빠뜨리면 만료된 신청을 수락했을 때 그 상태가 영영 남아,
	// IsTrading()이 계속 참이라 재접속 전까지 거래 신청도 수신도 막힌다
	if (State == ENUTradeState::Requesting || State == ENUTradeState::Invited)
	{
		ResetSession();
		SetState(ENUTradeState::Idle);
	}
}

void UNUTradeSubsystem::HandleTradeRequested(int64 FromIdx, const FString& FromName, int32 ExpireSec)
{
	if (IsTrading())
	{
		return; // 이미 거래 중이면 서버가 걸러주지만 방어
	}

	// 전투 중엔 팝업을 띄울 수 없어 답하지 못한 채 만료된다 --- 바로 거절해 상대를 기다리게 두지 않는다.
	// 배너도 띄우지 않는다 --- 전투 중인 화면에 거래 알림이 끼어들면 방해만 된다 (신청자 쪽이 사전에 안내받는다)
	if (IsInBattle())
	{
		if (UEmberServerManager* Server = GetServer())
		{
			Server->SendTradeRespond(false, FromIdx);
		}
		UE_LOG(LogNUTrade, Log, TEXT("전투 중 거래 신청 자동 거절 [%s] --- 배너 없음"), *FromName);
		return;
	}

	EnsureUIController();

	PartnerIdx = FromIdx;
	PartnerName = FromName;
	UE_LOG(LogNUTrade, Log, TEXT("거래 신청 받음 [%s] --- %d초 안에 응답"), *FromName, ExpireSec);

	// 서버가 준 값을 그대로 쓴다 (가이드 기준 60초). 지나서 수락하면 1805로 거절당한다
	if (ExpireSec > 0)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			GI->GetTimerManager().SetTimer(
				InviteExpireTimer,
				FTimerDelegate::CreateUObject(this, &ThisClass::HandleInviteExpired),
				static_cast<float>(ExpireSec),
				false);
		}
	}

	SetState(ENUTradeState::Invited);
}

void UNUTradeSubsystem::HandleInviteExpired()
{
	if (State != ENUTradeState::Invited)
	{
		return;
	}

	// 서버도 같은 시각에 접었다 --- 거절을 보내면 1805만 돌아오므로 내 상태만 되돌린다.
	// 창은 Idle 전이를 보고 UI 컨트롤러가 닫는다
	UE_LOG(LogNUTrade, Log, TEXT("거래 신청 만료 [%s]"), *PartnerName);
	ShowNotice(NSLOCTEXT("NUTrade", "InviteExpired", "거래 신청 시간이 지났습니다."));

	ResetSession();
	SetState(ENUTradeState::Idle);
}

void UNUTradeSubsystem::DeclinePendingInvite()
{
	if (State != ENUTradeState::Invited)
	{
		return;
	}

	// 배너 없음 --- 전투가 시작되는 화면에 거래 알림을 얹지 않는다 (팝업이 닫히는 것으로 충분)
	UE_LOG(LogNUTrade, Log, TEXT("전투 진입으로 대기 중인 거래 신청 거절 [%s] --- 배너 없음"), *PartnerName);

	// RespondTrade가 송신과 상태 정리를 같이 해준다
	RespondTrade(false);
}

int32 UNUTradeSubsystem::GetInviteRemainSeconds() const
{
	if (State != ENUTradeState::Invited)
	{
		return 0;
	}

	const UGameInstance* GI = GetGameInstance();
	if (GI == nullptr || !InviteExpireTimer.IsValid())
	{
		return 0;
	}

	const float Remain = GI->GetTimerManager().GetTimerRemaining(InviteExpireTimer);
	return Remain > 0.0f ? FMath::CeilToInt(Remain) : 0;
}

void UNUTradeSubsystem::ClearInviteExpireTimer()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		GI->GetTimerManager().ClearTimer(InviteExpireTimer);
	}
}

bool UNUTradeSubsystem::IsInBattle() const
{
	// 파티와 같은 값을 본다 --- 서버에 보고하는 활동 상태
	const UGameInstance* GI = GetGameInstance();
	const UWorld* World = GI ? GI->GetWorld() : nullptr;
	const UFieldSyncSubsystem* Sync = World ? World->GetSubsystem<UFieldSyncSubsystem>() : nullptr;

	return IsValid(Sync) && Sync->GetMyActivity() == EFieldActivity::Battle;
}

void UNUTradeSubsystem::HandleTradeStarted(int64 InTradeId, const FGrpcRevGameTradeSide& MeSide, const FGrpcRevGameTradeSide& OtherSide)
{
	EnsureUIController();

	TradeId = InTradeId;
	OfferVer = 0;

	// me는 항상 받는 사람 자신이라 좌우 배치를 고민할 필요가 없다
	PartnerIdx = static_cast<int64>(OtherSide.CharacterIdx.Value);
	PartnerName = OtherSide.CharacterName;

	ApplySide(MeSide, MyOffer);
	ApplySide(OtherSide, PartnerOffer);

	SetState(ENUTradeState::Active);
	OnTradeOfferChanged.Broadcast();
}

void UNUTradeSubsystem::HandleTradeUpdated(const FGrpcRevGameTradeSide& MeSide, const FGrpcRevGameTradeSide& OtherSide, int32 InOfferVer)
{
	// 확정에 그대로 실어 보낼 값 --- 갱신을 놓치면 확정이 1810으로 막힌다
	OfferVer = InOfferVer;

	const bool bWasMyLocked = MyOffer.bLocked;
	const bool bWasPartnerLocked = PartnerOffer.bLocked;

	ApplySide(MeSide, MyOffer);
	ApplySide(OtherSide, PartnerOffer);

	// 어느 쪽이든 제안을 바꾸면 서버가 양쪽 잠금·확정을 풀어 보낸다 --- 그대로 반영된다.
	// 화면에는 확정 버튼이 이유 없이 꺼진 것으로만 보이므로 왜인지 알린다
	if (bWasMyLocked && !MyOffer.bLocked)
	{
		if (bUnlockRequestedByMe)
		{
			bUnlockRequestedByMe = false; // 내가 푼 것이니 안내할 것이 없다
		}
		else
		{
			ShowNotice(NSLOCTEXT("NUTrade", "LockReleased",
				"제안이 바뀌어 양쪽 잠금이 풀렸습니다. 다시 확인해 주세요."));
		}
	}
	else if (!bWasPartnerLocked && PartnerOffer.bLocked)
	{
		ShowNotice(NSLOCTEXT("NUTrade", "PartnerLocked", "상대가 제안을 잠갔습니다."));
	}

	// 어느 쪽이든 잠그면 잠금 소리, 그 밖의 갱신(올리기·내리기·잠금 풀림)은 올리는 소리
	const bool bLocked = (!bWasMyLocked && MyOffer.bLocked) || (!bWasPartnerLocked && PartnerOffer.bLocked);
	UNUSFXSubsystem::Play(GetGameInstance(), bLocked ? NU::SFX::Trade::Lock : NU::SFX::Trade::Offer);

	OnTradeOfferChanged.Broadcast();
}

void UNUTradeSubsystem::HandleTradeEnded(int32 Reason)
{
	const ENUTradeEndReason EndReason = static_cast<ENUTradeEndReason>(Reason);
	UE_LOG(LogNUTrade, Log, TEXT("거래 종료 [사유=%d]"), Reason);

	// ResetSession이 플래그를 지우므로 그 전에 읽어 컨트롤러가 볼 수 있게 남긴다
	const bool bByMe = bCancelRequestedByMe;
	ResetSession();
	bCancelRequestedByMe = bByMe;

	SetState(ENUTradeState::Idle);

	// 상태를 먼저 접고 알린다 --- 창은 상태 변화로 이미 닫혔고, 이건 안내용이다.
	// 주고받은 아이템은 SVR_Item_Slot로 따로 오므로 여기서 인벤을 건드릴 것은 없다
	OnTradeEnded.Broadcast(EndReason);

	bCancelRequestedByMe = false; // 안내가 끝났으니 다음 거래를 위해 지운다
}

/////////////////////////////////////////////////////////////////
// 내부
/////////////////////////////////////////////////////////////////

void UNUTradeSubsystem::ApplySide(const FGrpcRevGameTradeSide& Side, FNUTradeOffer& OutOffer) const
{
	// 서버는 올린 칸만 보낸다 --- 6칸을 비워두고 trade_slot 자리에 꽂는다
	OutOffer.Slots.Reset();
	OutOffer.Slots.SetNum(NUTrade::SlotCount);

	const UNUInventoryComponent* Inventory = GetLocalInventory();
	for (const FGrpcRevGameTradeSlot& Slot : Side.Slots)
	{
		if (!OutOffer.Slots.IsValidIndex(Slot.TradeSlot) || Slot.ItemId == 0)
		{
			continue;
		}

		FNUTradeSlot& Target = OutOffer.Slots[Slot.TradeSlot];
		// 상대 인벤은 모르므로 서버가 준 숫자 id를 테이블 행 이름으로 푼다
		Target.ItemRowName = Inventory ? Inventory->FindRowNameByNumericId(Slot.ItemId) : NAME_None;
		Target.Count = Slot.ItemCount;
		Target.InvSlotNo = Slot.InvSlotNo;
	}

	OutOffer.bLocked = Side.Locked;
	OutOffer.bConfirmed = Side.Confirmed;
}

bool UNUTradeSubsystem::IsInventorySlotOffered(int32 InvSlotNo) const
{
	if (State != ENUTradeState::Active || InvSlotNo < 0)
	{
		return false;
	}

	for (const FNUTradeSlot& Slot : MyOffer.Slots)
	{
		if (!Slot.IsEmpty() && Slot.InvSlotNo == InvSlotNo)
		{
			return true;
		}
	}
	return false;
}

void UNUTradeSubsystem::SetState(ENUTradeState NewState)
{
	if (State == NewState)
	{
		return;
	}
	State = NewState;
	OnTradeStateChanged.Broadcast(NewState);
}

void UNUTradeSubsystem::ResetSession()
{
	MyOffer = FNUTradeOffer();
	PartnerOffer = FNUTradeOffer();
	TradeId = 0;
	PartnerIdx = 0;
	PartnerName.Empty();
	OfferVer = 0;
	bUnlockRequestedByMe = false;

	// 신청 단계에서만 도는 타이머 --- 어떤 경로로 세션이 끝나든 여기서 확실히 끊는다
	ClearInviteExpireTimer();
}

void UNUTradeSubsystem::EnsureUIController() const
{
	if (UNUUIManager* UI = GetUIManager())
	{
		UI->GetController<UNUTradeUIController>();
	}
}

void UNUTradeSubsystem::ShowNotice(const FText& Message) const
{
	if (UNUUIManager* UI = GetUIManager())
	{
		UI->ShowNotice(Message);
	}
}

UNUUIManager* UNUTradeSubsystem::GetUIManager() const
{
	UGameInstance* GI = GetGameInstance();
	APlayerController* PC = GI ? GI->GetFirstLocalPlayerController() : nullptr;
	return UNUUIManager::Get(PC);
}

UEmberServerManager* UNUTradeSubsystem::GetServer() const
{
	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UEmberServerManager>() : nullptr;
}

UNUInventoryComponent* UNUTradeSubsystem::GetLocalInventory() const
{
	const UGameInstance* GI = GetGameInstance();
	const UWorld* World = GI ? GI->GetWorld() : nullptr;
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	const ANUPlayerState* PS = PC ? PC->GetPlayerState<ANUPlayerState>() : nullptr;
	return PS ? PS->FindComponentByClass<UNUInventoryComponent>() : nullptr;
}
