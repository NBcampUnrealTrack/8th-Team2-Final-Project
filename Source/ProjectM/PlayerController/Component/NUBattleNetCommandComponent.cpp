// Fill out your copyright notice in the Description page of Project Settings.

#include "NUBattleNetCommandComponent.h"

#include "Battle/Data/NUActiveSkillDefinition.h"
#include "Battle/Data/NUBattleDataSettings.h"
#include "Battle/Data/NUBattleDataSubsystem.h"
#include "Battle/Data/NUBattleSkillInfo.h"
#include "Battle/Data/NUJobSkillContext.h"
#include "Battle/Data/NUStatusEffectDefinition.h"
#include "Battle/Net/NUBattleNetActionQueueSubsystem.h"
#include "Battle/Net/NUBattleNetCommandSubsystem.h"
#include "Battle/Net/NUBattleNetStageSubsystem.h"
#include "Battle/Net/NUBattleNetSubsystem.h"
#include "Battle/Avatar/NUBattleAvatar.h"
#include "CharacterProgression/Component/NUPlayerSkillComponent.h"
#include "CharacterSelect/NUCharacterManager.h"
#include "CharacterState/NUPlayerState.h"
#include "CharacterState/NUSpiritState.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Inventory/NUInventoryComponent.h"
#include "Naia/Component/NUNaiaRosterComponent.h"
#include "NUGAS/Ability/NUPlayerBattleAbility.h"

DEFINE_LOG_CATEGORY_STATIC(LogNUBattleNetCommandUI, Log, All);

namespace
{
	// ReportSkillListDiagnostic 의 사유 번호
	// 스킬 메뉴가 비는 경우가 여럿인데 화면에서는 전부 똑같이 보여서, 어디서 걸렸는지 번호로 가름
	constexpr int32 SkillDiagOk               = 0;
	constexpr int32 SkillDiagCombatantMissing = 1;
	constexpr int32 SkillDiagNoInstanceId     = 2;
	constexpr int32 SkillDiagBlockedByStatus  = 3;
	constexpr int32 SkillDiagRosterMissing    = 4;
	constexpr int32 SkillDiagNotInParty       = 5;
	constexpr int32 SkillDiagNoSkills         = 6;
	constexpr int32 SkillDiagPlayerSkillsMissing = 7;
	constexpr int32 SkillDiagNotOffered       = 8;
	constexpr int32 SkillDiagSnapshotEmpty    = 9;
}

UNUBattleNetCommandComponent::UNUBattleNetCommandComponent()
{
}

void UNUBattleNetCommandComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UNUBattleNetSubsystem* BattleNet = GetBattleNet())
	{
		BattleNet->OnBattleStateChanged.AddUObject(this, &ThisClass::HandleBattleStateChanged);
	}
	if (UNUBattleNetCommandSubsystem* CommandNet = GetCommandNet())
	{
		CommandNet->OnCommandTurnChanged.AddUObject(this, &ThisClass::HandleCommandTurnChanged);
		CommandNet->OnCommandRejected.AddUObject(this, &ThisClass::HandleCommandRejected);
	}
	if (UNUBattleNetStageSubsystem* StageNet = GetStageNet())
	{
		StageNet->OnNetStageReadyChanged.AddUObject(this, &ThisClass::HandleStageReadyChanged);
	}
	if (UNUBattleNetActionQueueSubsystem* ActionQueue = GetActionQueueNet())
	{
		ActionQueue->OnPlaybackDrained.AddUObject(this, &ThisClass::HandlePlaybackDrained);
	}

	EvaluateSession();
}

void UNUBattleNetCommandComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UNUBattleNetSubsystem* BattleNet = GetBattleNet())
	{
		BattleNet->OnBattleStateChanged.RemoveAll(this);
	}
	if (UNUBattleNetCommandSubsystem* CommandNet = GetCommandNet())
	{
		CommandNet->OnCommandTurnChanged.RemoveAll(this);
		CommandNet->OnCommandRejected.RemoveAll(this);
	}
	if (UNUBattleNetStageSubsystem* StageNet = GetStageNet())
	{
		StageNet->OnNetStageReadyChanged.RemoveAll(this);
	}
	if (UNUBattleNetActionQueueSubsystem* ActionQueue = GetActionQueueNet())
	{
		ActionQueue->OnPlaybackDrained.RemoveAll(this);
	}

	SetSessionActive(false);
	Super::EndPlay(EndPlayReason);
}

bool UNUBattleNetCommandComponent::IsBattleActive() const
{
	const UNUBattleNetSubsystem* BattleNet = GetBattleNet();
	return IsValid(BattleNet) && BattleNet->IsInBattle();
}

void UNUBattleNetCommandComponent::RebuildControllableCombatants()
{
	ControllableCombatantIds.Reset();

	const UNUBattleNetCommandSubsystem* CommandNet = GetCommandNet();
	if (IsValid(CommandNet))
	{
		ControllableCombatantIds.Append(CommandNet->GetControllableCombatantIds());
	}
}

void UNUBattleNetCommandComponent::CollectLivingCombatantIds(ENUBattleTeam Team, TArray<int32>& OutIds) const
{
	OutIds.Reset();

	const UNUBattleNetSubsystem* BattleNet = GetBattleNet();
	if (!IsValid(BattleNet))
	{
		return;
	}

	const int32 Side = Team == ENUBattleTeam::Enemy ? 1 : 0;
	for (const TPair<int32, FNUServerCombatant>& Pair : BattleNet->GetCombatants())
	{
		if (Pair.Value.Side == Side && Pair.Value.IsAlive())
		{
			OutIds.Add(Pair.Key);
		}
	}
	OutIds.Sort();
}

void UNUBattleNetCommandComponent::RecordDownedNaia()
{
	const UNUBattleNetSubsystem* BattleNet = GetBattleNet();
	if (!IsValid(BattleNet))
	{
		return;
	}

	for (const TPair<int32, FNUServerCombatant>& Pair : BattleNet->GetCombatants())
	{
		// 적과 플레이어 캐릭터는 교체 대상이 아니므로 나이아 인스턴스가 있는 아군만 봄
		if (Pair.Value.Side == 1 || Pair.Value.NaiaInstanceId <= 0 || Pair.Value.IsAlive())
		{
			continue;
		}

		// 상태 통지가 한 턴에 여러 번 오므로 처음 본 것만 찍음
		if (DownedNaiaInstanceIds.Contains(Pair.Value.NaiaInstanceId))
		{
			continue;
		}

		DownedNaiaInstanceIds.Add(Pair.Value.NaiaInstanceId);

		UE_LOG(LogNUBattleNetCommandUI, Log,
			TEXT("나이아 %lld 쓰러짐 - 이번 전투에서는 다시 낼 수 없음"),
			Pair.Value.NaiaInstanceId);
	}
}

void UNUBattleNetCommandComponent::CollectSwappableNaiaSlots(TArray<int32>& OutSlots) const
{
	Super::CollectSwappableNaiaSlots(OutSlots);

	if (DownedNaiaInstanceIds.IsEmpty())
	{
		return;
	}

	// 베이스는 GAS 사망 태그로 가려내는데 넷 전투는 그 태그가 붙지 않음
	// 쓰러진 나이아는 교체로 빠진 뒤에도 파티 칸에 남아 있으므로 여기서 뺌
	const UNUNaiaRosterComponent* Roster = GetNaiaRosterComponent();
	const TArray<TObjectPtr<ANUSpiritState>>* Party =
		IsValid(Roster) ? &Roster->GetParty() : nullptr;
	if (Party == nullptr)
	{
		return;
	}

	OutSlots.RemoveAll([this, Party](int32 Slot)
	{
		const ANUSpiritState* Naia = Party->IsValidIndex(Slot) ? (*Party)[Slot].Get() : nullptr;

		return !IsValid(Naia) || DownedNaiaInstanceIds.Contains(Naia->GetInstanceId());
	});
}

void UNUBattleNetCommandComponent::CollectDownedCombatantIds(ENUBattleTeam Team, TArray<int32>& OutIds) const
{
	OutIds.Reset();

	const UNUBattleNetSubsystem* BattleNet = GetBattleNet();
	if (!IsValid(BattleNet))
	{
		return;
	}

	// 서버 미러를 봄 - 재생 큐의 표시용 사본이 아니라 서버가 확정한 값이어야 제출이 안 튕김
	const int32 Side = Team == ENUBattleTeam::Enemy ? 1 : 0;
	for (const TPair<int32, FNUServerCombatant>& Pair : BattleNet->GetCombatants())
	{
		if (Pair.Value.Side == Side && !Pair.Value.IsAlive())
		{
			OutIds.Add(Pair.Key);
		}
	}
	OutIds.Sort();
}

AActor* UNUBattleNetCommandComponent::GetCombatantAvatar(int32 CombatantId) const
{
	const UNUBattleNetStageSubsystem* StageNet = GetStageNet();
	return IsValid(StageNet) ? StageNet->FindAvatar(CombatantId) : nullptr;
}

FText UNUBattleNetCommandComponent::GetCombatantDisplayName(int32 CombatantId) const
{
	const UNUBattleNetSubsystem* BattleNet = GetBattleNet();
	const FNUServerCombatant* Combatant =
		IsValid(BattleNet) ? BattleNet->FindCombatant(CombatantId) : nullptr;
	if (!Combatant)
	{
		return FText::GetEmpty();
	}

	// 서버가 표시용 이름(BattleCombatant.name)을 보내주므로 그것이 먼저
	if (!Combatant->Name.IsEmpty())
	{
		return FText::FromString(Combatant->Name);
	}

	// 아래는 서버가 이름을 안 채웠을 때의 대체 경로
	if (Combatant->Kind != 0)
	{
		return FindNaiaDisplayName(Combatant->NaiaId);
	}

	// 남의 캐릭터에 내 닉네임을 붙이면 안 되므로 내 전투원일 때만 대신 채움
	const UGameInstance* GameInstance = GetOwner() ? GetOwner()->GetGameInstance() : nullptr;
	const UNUCharacterManager* Manager =
		IsValid(GameInstance) ? GameInstance->GetSubsystem<UNUCharacterManager>() : nullptr;
	const int64 MyCharacterIdx = IsValid(Manager) ? Manager->GetSelectedCharacterIdx() : 0;

	return MyCharacterIdx != 0 && Combatant->OwnerCharacterIdx == MyCharacterIdx
		? GetPlayerCharacterDisplayName()
		: FText::GetEmpty();
}

float UNUBattleNetCommandComponent::GetCombatantSP(int32 CombatantId) const
{
	const UNUBattleNetSubsystem* BattleNet = GetBattleNet();
	const FNUServerCombatant* Combatant =
		IsValid(BattleNet) ? BattleNet->FindCombatant(CombatantId) : nullptr;
	return Combatant ? static_cast<float>(Combatant->SP) : 0.f;
}

bool UNUBattleNetCommandComponent::IsPlayerCombatant(int32 CombatantId) const
{
	const UNUBattleNetSubsystem* BattleNet = GetBattleNet();
	const FNUServerCombatant* Combatant =
		IsValid(BattleNet) ? BattleNet->FindCombatant(CombatantId) : nullptr;
	return Combatant && Combatant->Kind == 0;
}

void UNUBattleNetCommandComponent::CollectOwnedActiveSkillIds(int32 CombatantId, TArray<int32>& OutSkillIds) const
{
	OutSkillIds.Reset();

	const UNUBattleNetSubsystem* BattleNet = GetBattleNet();
	const FNUServerCombatant* Combatant =
		IsValid(BattleNet) ? BattleNet->FindCombatant(CombatantId) : nullptr;

	if (!Combatant)
	{
		ReportSkillListDiagnostic(CombatantId, SkillDiagCombatantMissing,
			TEXT("서버 스냅샷에 이 전투원이 없음"));
		return;
	}

	// 스킬 메뉴가 있는 것은 캐릭터(0)와 나이아(1)뿐, 그 밖은 정상 상태라 로그를 남기지 않음
	if (Combatant->Kind != 0 && Combatant->Kind != 1)
	{
		return;
	}

	const UNUBattleDataSubsystem* BattleData =
		GetOwner() && GetOwner()->GetGameInstance()
			? GetOwner()->GetGameInstance()->GetSubsystem<UNUBattleDataSubsystem>()
			: nullptr;
	for (const FNUServerBattleStatus& Status : Combatant->Statuses)
	{
		const FNUStatusEffectDefinition* Definition =
			IsValid(BattleData) ? BattleData->FindStatusEffectDefinition(Status.StatusId) : nullptr;
		if (Definition && Definition->bBlockActiveSkill)
		{
			// 실명 같은 상태이상이 막은 것, 스킬을 안 배운 것과 다름 (캐릭터도 못 씀)
			ReportSkillListDiagnostic(CombatantId, SkillDiagBlockedByStatus,
				FString::Printf(TEXT("상태이상 %d(%s)가 액티브 스킬을 막음"),
					Status.StatusId, *Definition->KoreanName.ToString()));
			return;
		}
	}

	// 서버가 준 목록이 있으면 그것이 서버 검증(1910)의 기준이라 그대로 올림
	// 서버가 아직 못 돌리는 스킬도 거르지 않음 - 눌렀을 때 서버가 거절하면 사유가 로그로 남음
	if (!Combatant->ActiveSkillIds.IsEmpty())
	{
		OutSkillIds = Combatant->ActiveSkillIds;
		ReportSkillListDiagnostic(CombatantId, SkillDiagOk,
			FString::Printf(TEXT("서버 스냅샷의 액티브 스킬 %d개"), OutSkillIds.Num()));
		return;
	}

	FString OwnerDetail;

	if (Combatant->Kind == 0)
	{
		// 로컬 보유 스킬은 내 캐릭터의 것이라, 남의 캐릭터(파티원)는 이 경로로 채우지 않음
		const UGameInstance* OwnerGameInstance = GetOwner() ? GetOwner()->GetGameInstance() : nullptr;
		const UNUCharacterManager* OwnerManager =
			IsValid(OwnerGameInstance) ? OwnerGameInstance->GetSubsystem<UNUCharacterManager>() : nullptr;
		const int64 MyOwnerIdx = IsValid(OwnerManager) ? OwnerManager->GetSelectedCharacterIdx() : 0;
		if (MyOwnerIdx != 0 && Combatant->OwnerCharacterIdx != MyOwnerIdx)
		{
			ReportSkillListDiagnostic(CombatantId, SkillDiagSnapshotEmpty,
				TEXT("다른 플레이어의 캐릭터라 로컬 스킬 목록으로 채우지 않음"));
			return;
		}

		const ANUPlayerState* PlayerState = Cast<ANUPlayerState>(GetLocalPlayerState());
		const UNUPlayerSkillComponent* Skills =
			IsValid(PlayerState) ? PlayerState->GetPlayerSkillComponent() : nullptr;
		if (!IsValid(Skills))
		{
			ReportSkillListDiagnostic(CombatantId, SkillDiagPlayerSkillsMissing,
				TEXT("캐릭터의 스킬 보유 컴포넌트를 찾지 못함"));
			return;
		}

		OutSkillIds = Skills->GetOwnedActiveSkillIds();
		OwnerDetail = FString::Printf(TEXT("캐릭터의 액티브 스킬 %d개"), OutSkillIds.Num());
	}
	else
	{
		// 스킬 목록의 출처가 로스터라 이 값이 개체를 찾는 유일한 열쇠임
		// 서버가 안 채우면 메뉴가 통째로 비는데, 화면에서는 스킬을 안 배운 것과 구분이 안 됨
		if (Combatant->NaiaInstanceId == 0)
		{
			ReportSkillListDiagnostic(CombatantId, SkillDiagNoInstanceId,
				FString::Printf(TEXT("naia_instance_id가 0임 (NaiaId=%d)"), Combatant->NaiaId));
			return;
		}

		const UNUNaiaRosterComponent* Roster = GetNaiaRosterComponent();
		if (!IsValid(Roster))
		{
			ReportSkillListDiagnostic(CombatantId, SkillDiagRosterMissing,
				TEXT("나이아 로스터 컴포넌트를 찾지 못함"));
			return;
		}

		const ANUSpiritState* FoundNaia = nullptr;
		for (const TObjectPtr<ANUSpiritState>& PartyNaia : Roster->GetParty())
		{
			if (IsValid(PartyNaia) && PartyNaia->GetInstanceId() == Combatant->NaiaInstanceId)
			{
				FoundNaia = PartyNaia;
				break;
			}
		}

		if (!FoundNaia)
		{
			// 서버 전투원과 클라 파티가 서로 다른 개체를 가리키는 상태
			// 교체 반영이 밀렸거나 서버가 파티에 없는 개체를 전투에 올린 경우
			ReportSkillListDiagnostic(CombatantId, SkillDiagNotInParty,
				FString::Printf(TEXT("파티 %d마리 중에 인스턴스 %lld가 없음 (NaiaId=%d)"),
					Roster->GetParty().Num(), Combatant->NaiaInstanceId, Combatant->NaiaId));
			return;
		}

		OutSkillIds.Append(FoundNaia->GetActiveSkillIds());
		OwnerDetail = FString::Printf(TEXT("파티 나이아 %lld(NaiaId=%d)의 액티브 스킬 %d개"),
			Combatant->NaiaInstanceId, Combatant->NaiaId, OutSkillIds.Num());
	}

	// 여기부터는 서버 목록이 없어 로컬 상태로 만든 목록, 서버가 실행을 붙인 것만 메뉴에 올림 (설정이 비어 있으면 거르지 않음)
	const int32 OwnedCount = OutSkillIds.Num();
	const UNUBattleDataSettings* Settings = GetDefault<UNUBattleDataSettings>();
	if (Settings && !Settings->NetOfferedSkillIds.IsEmpty())
	{
		OutSkillIds.RemoveAll(
			[Settings](int32 SkillId)
			{
				return !Settings->NetOfferedSkillIds.Contains(SkillId);
			});
	}

	// 걸러서 비면 스킬을 안 배운 것과 화면에서 구분이 안 되므로 사유를 따로 남김
	if (OwnedCount > 0 && OutSkillIds.IsEmpty())
	{
		ReportSkillListDiagnostic(CombatantId, SkillDiagNotOffered,
			FString::Printf(TEXT("%s 중 넷에서 내놓는 것 0개"), *OwnerDetail));
		return;
	}

	// 서버 목록이 비었는데 로컬 목록으로 메뉴를 만든 것, 서버는 이 스킬을 1910 으로 거절할 수 있음
	// 포획한 나이아는 서버가 목록을 비워 두므로 정상이고, 옛 서버이거나 서버가 채우지 못한 경우도 여기로 옴
	ReportSkillListDiagnostic(CombatantId,
		OutSkillIds.IsEmpty() ? SkillDiagNoSkills : SkillDiagSnapshotEmpty,
		FString::Printf(TEXT("서버 스냅샷에 액티브 스킬이 없음, 로컬 목록으로 대신함 - %s, 메뉴에 올리는 것 %d개"),
			*OwnerDetail, OutSkillIds.Num()));
}

void UNUBattleNetCommandComponent::ReportSkillListDiagnostic(int32 CombatantId, int32 ReasonCode,
	const FString& Detail) const
{
	// 같은 전투원이 같은 사유로 계속 걸리는 동안은 한 번만 찍음
	if (LastSkillDiagnosticCombatantId == CombatantId && LastSkillDiagnosticReason == ReasonCode)
	{
		return;
	}

	LastSkillDiagnosticCombatantId = CombatantId;
	LastSkillDiagnosticReason = ReasonCode;

	if (ReasonCode == SkillDiagOk)
	{
		UE_LOG(LogNUBattleNetCommandUI, Log,
			TEXT("전투원 %d 스킬 목록: %s"), CombatantId, *Detail);
		return;
	}

	UE_LOG(LogNUBattleNetCommandUI, Warning,
		TEXT("전투원 %d 스킬 목록 확인 필요 [사유 %d]: %s"), CombatantId, ReasonCode, *Detail);
}

int32 UNUBattleNetCommandComponent::ApplyStatusSPCost(int32 CombatantId, int32 BaseCost) const
{
	if (BaseCost <= 0)
	{
		return BaseCost;
	}

	const UNUBattleNetSubsystem* BattleNet = GetBattleNet();
	const FNUServerCombatant* Combatant =
		IsValid(BattleNet) ? BattleNet->FindCombatant(CombatantId) : nullptr;
	const UNUBattleDataSubsystem* BattleData =
		GetOwner() && GetOwner()->GetGameInstance()
			? GetOwner()->GetGameInstance()->GetSubsystem<UNUBattleDataSubsystem>()
			: nullptr;
	
	if (!Combatant || !IsValid(BattleData))
	{
		return BaseCost;
	}

	float Rate = 0.f;
	for (const FNUServerBattleStatus& Status : Combatant->Statuses)
	{
		if (const FNUStatusEffectDefinition* Definition =
			BattleData->FindStatusEffectDefinition(Status.StatusId))
		{
			Rate += Definition->SPCostRate;
		}
	}

	return Rate > KINDA_SMALL_NUMBER
		? FMath::FloorToInt(BaseCost * (1.f + Rate))
		: BaseCost;
}

bool UNUBattleNetCommandComponent::GetPendingSPRule(FNUCommandSPRule& OutRule) const
{
	OutRule = FNUCommandSPRule();

	if (PendingSkillId > 0)
	{
		FNUBattleSkillInfo Info;
		if (!TryGetSkillInfo(PendingSkillId, Info))
		{
			return false;
		}

		// 스킬 SP 는 서버가 시트 값 그대로 차감함 (젖음도 스킬에는 안 곱함), 표시가 서버와 같아야 함
		// 기본 공격만 젖음이 붙음, 로컬 전투의 스킬 SP 규칙은 기획 확인 중이라 넷만 서버를 따름
		// 남은 SP 를 전부 쓰는 스킬(3203 나이아 개화)은 지금 SP 가 비용
		OutRule.FixedCost = NUJobSkill::FindSpendAllSPSkill(PendingSkillId)
			? FMath::FloorToInt(GetCurrentSourceSP())
			: Info.SPCost;
		OutRule.FixedHitCount = Info.HitCount;
		return true;
	}

	if (PendingRootAction == ENUBattleRootAction::Attack)
	{
		const UNUBattleDataSettings* Settings = GetDefault<UNUBattleDataSettings>();
		UClass* AbilityClass = Settings ? Settings->NetDefaultActionAbility.LoadSynchronous() : nullptr;
		const UNUPlayerBattleAbility* Ability =
			AbilityClass ? Cast<UNUPlayerBattleAbility>(AbilityClass->GetDefaultObject()) : nullptr;
		if (!Ability)
		{
			return false;
		}

		OutRule.bRepeatable = Ability->IsRepeatable();
		OutRule.FixedCost = ApplyStatusSPCost(GetCurrentSourceCombatantId(), Ability->GetSPCost());
		OutRule.CostPerHit = FMath::Max(
			1,
			ApplyStatusSPCost(GetCurrentSourceCombatantId(), Ability->GetSPCostPerHit()));
		return true;
	}

	// 도주와 포획은 SP를 사용하지 않음
	return PendingRootAction == ENUBattleRootAction::Flee
		|| PendingRootAction == ENUBattleRootAction::Item;
}

bool UNUBattleNetCommandComponent::SubmitPendingCommand(int32 SelectedTargetId)
{
	UNUBattleNetCommandSubsystem* CommandNet = GetCommandNet();
	if (!IsValid(CommandNet))
	{
		return false;
	}

	int32 ItemId = 0;
	bool bCommuneItem = false;

	// 제출할 행동으로 판단해야함
	// PendingItemInstanceId 가 남아있는 경우가 있어서 제출되지 않는 문제가 있었음
	if (PendingRootAction == ENUBattleRootAction::Item)
	{
		const UNUInventoryComponent* Inventory = GetInventoryComponent();
		const FNUItemInstance* Item = PendingItemInstanceId.IsValid() && IsValid(Inventory)
			? Inventory->FindInstance(PendingItemInstanceId)
			: nullptr;
		const FNUItemDefRow* Definition =
			Item && IsValid(Inventory) ? Inventory->FindItemDef(Item->ItemRowName) : nullptr;
		if (!Definition)
		{
			// 고르는 사이에 아이템이 없어진 경우, 조용히 실패하면 원인을 못 찾으므로 남김
			UE_LOG(LogNUBattleNetCommandUI, Warning,
				TEXT("아이템 명령을 낼 수 없음 - 고른 아이템이 인벤에 없음 (이미 소모됐을 수 있음)"));

			return false;
		}

		// 서버는 아이템을 숫자로만 받으므로 행 이름밖에 없는 아이템은 낼 수가 없음
		// 기획 표가 안 붙은 런타임 더미 정의가 이 경우라 로그를 남김
		if (Definition->NumericId <= 0)
		{
			UE_LOG(LogNUBattleNetCommandUI, Warning,
				TEXT("아이템 [%s] 에 NumericId 가 없어 서버로 보낼 수 없음"),
				*Item->ItemRowName.ToString());

			return false;
		}

		// 포획과 아이템 사용은 서버 행동 번호가 달라서(3 / 4) 종류로 가름
		ItemId = Definition->NumericId;
		bCommuneItem = Definition->ItemType == ENUItemType::Commune;
	}

	const int32 CombatantId = GetCurrentSourceCombatantId();
	const bool bSubmitted = CommandNet->SubmitCurrentCommand(
		PendingRootAction,
		PendingSkillId,
		SelectedTargetId,
		PendingSPSpend,
		ItemId,
		bCommuneItem
		);

	// 서버 응답을 기다리지 않고 미리 반영함 - 투입 SP는 서버가 그대로 차감하므로 예측이
	// 어긋날 일이 없고, BattleCommand 직접 응답이 늦게 와서 폐기되는 경우(가이드에 명시된
	// gRPC 두 채널의 순서 미보장 레이스)에도 이 값이 유일하게 믿을 수 있는 반영 경로가 됨
	// 거절되면 HandleCommandRejected가 서버 스냅샷 값으로 되돌림
	if (bSubmitted && PendingSPSpend > 0)
	{
		if (UNUBattleNetActionQueueSubsystem* ActionQueue = GetActionQueueNet())
		{
			const int32 CurrentSP = FMath::RoundToInt(GetCombatantSP(CombatantId));
			ActionQueue->ApplySP(CombatantId, FMath::Max(0, CurrentSP - PendingSPSpend));
		}
	}

	return bSubmitted;
}

void UNUBattleNetCommandComponent::RequestNaiaSwap(int32 PartySlot)
{
	if (UNUBattleNetSubsystem* BattleNet = GetBattleNet())
	{
		BattleNet->RequestNaiaSwap(PartySlot);
	}

	LastNaiaSwapTurn = INDEX_NONE;
}

UNUBattleNetSubsystem* UNUBattleNetCommandComponent::GetBattleNet() const
{
	const UGameInstance* GameInstance = GetOwner() ? GetOwner()->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UNUBattleNetSubsystem>() : nullptr;
}

UNUBattleNetCommandSubsystem* UNUBattleNetCommandComponent::GetCommandNet() const
{
	const UGameInstance* GameInstance = GetOwner() ? GetOwner()->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UNUBattleNetCommandSubsystem>() : nullptr;
}

UNUBattleNetStageSubsystem* UNUBattleNetCommandComponent::GetStageNet() const
{
	UWorld* World = GetWorld();
	return IsValid(World) ? World->GetSubsystem<UNUBattleNetStageSubsystem>() : nullptr;
}

UNUBattleNetActionQueueSubsystem* UNUBattleNetCommandComponent::GetActionQueueNet() const
{
	UWorld* World = GetWorld();
	return IsValid(World) ? World->GetSubsystem<UNUBattleNetActionQueueSubsystem>() : nullptr;
}

void UNUBattleNetCommandComponent::EvaluateSession()
{
	const UNUBattleNetStageSubsystem* StageNet = GetStageNet();
	const bool bShouldBeActive = IsBattleActive()
		&& IsValid(StageNet)
		&& StageNet->IsNetStageReady();

	if (!bShouldBeActive)
	{
		// 전투가 끝나면 쓰러진 기록도 끝남, 다음 전투에서는 그 나이아를 다시 낼 수 있다
		DownedNaiaInstanceIds.Reset();
		SetSessionActive(false);
		return;
	}

	const bool bWasActive = IsSessionActive();
	SetSessionActive(true);
	if (!bWasActive)
	{
		SyncCommandSelection();
	}
}

void UNUBattleNetCommandComponent::SyncCommandSelection()
{
	if (!IsSessionActive())
	{
		return;
	}

	// 이전 턴 타격이 아직 화면에서 재생 중이면 메뉴를 미룸
	// 재생이 끝나면 OnPlaybackDrained -> HandlePlaybackDrained가 이 함수를 다시 부름
	const UNUBattleNetActionQueueSubsystem* ActionQueue = GetActionQueueNet();
	if (IsValid(ActionQueue) && ActionQueue->IsPlaybackBusy())
	{
		UE_LOG(LogNUBattleNetCommandUI, Log,
			TEXT("명령 선택 대기 - 재생 큐가 아직 재생 중이라 메뉴를 미룸"));
		return;
	}

	const UNUBattleNetSubsystem* BattleNet = GetBattleNet();
	const UNUBattleNetCommandSubsystem* CommandNet = GetCommandNet();
	if (!IsValid(BattleNet) || !IsValid(CommandNet))
	{
		ResetSelection();
		return;
	}

	const int32 CombatantId = CommandNet->GetCurrentCombatantId();
	if (CombatantId == INDEX_NONE)
	{
		// 서버 Pending이 남아 있으면 다른 전투원(다른 플레이어)이 아직 명령을 안 낸 것
		// 그때만 "다른 플레이어를 기다리는 중" 문구를 보여줌.
		// Pending이 비었으면 수집이 끝나 연출 실행 단계로 넘어간 것이라, 
		// 로컬의 ActionExecution 페이즈처럼 선택 UI 자체를 숨겨야 함 - 안 그러면 타격 연출 내내 대기 문구가 떠 있게 됨
		if (BattleNet->GetPendingCombatantIds().IsEmpty())
		{
			UE_LOG(LogNUBattleNetCommandUI, Log,
				TEXT("명령 선택 숨김 - 수집이 끝나 실행 단계로 넘어감 (Turn=%d)"), BattleNet->GetTurn());
			ResetSelection();
			return;
		}

		UE_LOG(LogNUBattleNetCommandUI, Log,
			TEXT("명령 선택 대기 - 다른 전투원의 제출을 기다림 (Turn=%d)"), BattleNet->GetTurn());
		SelectionTurnNumber = BattleNet->GetTurn();
		SetSelectionPhase(ENUCommandSelectionPhase::WaitingForOthers);
		return;
	}

	// 같은 전투원·같은 턴이면 이미 열려 있는 선택을 다시 초기화하지 않음
	if (GetCurrentSourceCombatantId() != CombatantId
		|| GetSelectionTurnNumber() != BattleNet->GetTurn())
	{
		UE_LOG(LogNUBattleNetCommandUI, Log,
			TEXT("명령 선택 열림 Turn=%d Combatant=%d"), BattleNet->GetTurn(), CombatantId);
		BeginCommandSelection(BattleNet->GetTurn());
	}
}

void UNUBattleNetCommandComponent::HandleBattleStateChanged()
{
	RecordDownedNaia();

	const bool bWasActive = IsSessionActive();
	EvaluateSession();
	if (bWasActive && IsSessionActive())
	{
		BroadcastSelectionChanged();
	}
}

void UNUBattleNetCommandComponent::HandleStageReadyChanged(bool bReady)
{
	EvaluateSession();
}

void UNUBattleNetCommandComponent::HandleCommandTurnChanged(int32 CombatantId)
{
	if (!IsSessionActive())
	{
		return;
	}

	if (GetSelectionPhase() == ENUCommandSelectionPhase::Submitting)
	{
		HandleSubmitAccepted();
	}

	SyncCommandSelection();
}

void UNUBattleNetCommandComponent::HandleCommandRejected(int32 CombatantId, ENUBattleCommandSubmitResult Result)
{
	if (!IsSessionActive() || PendingSubmissionSourceId != CombatantId)
	{
		return;
	}

	// SubmitPendingCommand가 제출 시점에 미리 깎아둔 SP를 되돌림 - 서버가 거절했으니
	// 실제로는 안 깎였다. 미러(BattleNet)의 값은 거절 시 안 건드리므로 그대로 믿을 수 있음
	if (const UNUBattleNetSubsystem* BattleNet = GetBattleNet())
	{
		if (const FNUServerCombatant* Combatant = BattleNet->FindCombatant(CombatantId))
		{
			if (UNUBattleNetActionQueueSubsystem* ActionQueue = GetActionQueueNet())
			{
				ActionQueue->ApplySP(CombatantId, Combatant->SP);
			}
		}
	}

	HandleSubmitRejected(Result);
}

void UNUBattleNetCommandComponent::HandlePlaybackDrained()
{
	UE_LOG(LogNUBattleNetCommandUI, Log, TEXT("재생 큐 비워짐 - 명령 선택 재시도"));
	SyncCommandSelection();
}
