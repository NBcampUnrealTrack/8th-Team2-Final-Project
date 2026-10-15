// Fill out your copyright notice in the Description page of Project Settings.

#include "NUBattleCommandComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Battle/Data/NUBattleSkillInfo.h"
#include "Battle/Data/NUStatusEffectDefinition.h"
#include "Battle/Encounter/NUBattleEncounter.h"
#include "Battle/Interface/NUBattleEventInterface.h"
#include "CharacterState/NUSpiritState.h"
#include "CharacterState/NUPlayerState.h"
#include "CharacterProgression/Component/NUPlayerSkillComponent.h"
#include "Inventory/NUInventoryComponent.h"
#include "NUGAS/NUAbilitySystemComponent.h"
#include "NUGAS/NUGameplayTags.h"
#include "NUGAS/Ability/NUPlayerBattleAbility.h"
#include "NUGAS/Attribute/NUVitalAttributeSet.h"
#include "PlayerController/NUPlayerController.h"
#include "GameFramework/PlayerState.h"

UNUBattleCommandComponent::UNUBattleCommandComponent()
{
}

void UNUBattleCommandComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindEncounter();
	Super::EndPlay(EndPlayReason);
}

// SP 미리보기와 실제 명령 제출이 같은 어빌리티를 조회해야 한다.
FGameplayTag UNUBattleCommandComponent::ResolveAttackAbilityId() const
{
	if (IsValid(Encounter))
	{
		if (const INUBattleEventInterface* Handler = Encounter->GetBattleEventHandler())
		{
			const FGameplayTag Override = Handler->GetAttackAbilityOverride(Encounter, GetCurrentSourceCombatantId());
			if (Override.IsValid()) { return Override; }
		}
	}
	return NU::Ability::Battle::BasicAttack;
}

FText UNUBattleCommandComponent::GetAttackDisplayName() const
{
	if (!IsValid(Encounter)) { return Super::GetAttackDisplayName(); }

	FNUBattleCommandRequest Request;
	Request.SourceCombatantId = GetCurrentSourceCombatantId();
	Request.AbilityId = ResolveAttackAbilityId();
	const UNUBattleGameplayAbility* Ability = Encounter->FindBattleAbility(Request);
	return Request.AbilityId != NU::Ability::Battle::BasicAttack && Ability != nullptr ? Ability->GetDisplayName() : Super::GetAttackDisplayName();
}

FGameplayTag UNUBattleCommandComponent::ResolveSkillAbilityId() const
{
	if (IsValid(Encounter))
	{
		if (const INUBattleEventInterface* Handler = Encounter->GetBattleEventHandler())
		{
			const FGameplayTag Override = Handler->GetSkillAbilityOverride(Encounter, GetCurrentSourceCombatantId());
			if (Override.IsValid()) { return Override; }
		}
	}
	return NU::Ability::Battle::ActiveSkill;
}

bool UNUBattleCommandComponent::HasSkillActionOverride() const { return ResolveSkillAbilityId() != NU::Ability::Battle::ActiveSkill; }

bool UNUBattleCommandComponent::IsSkillActionOverrideAvailable() const
{
	if (!IsValid(Encounter) || !HasSkillActionOverride()) { return false; }
	FNUBattleCommandRequest Request;
	Request.SourceCombatantId = GetCurrentSourceCombatantId();
	Request.AbilityId = ResolveSkillAbilityId();
	const INUBattleEventInterface* Handler = Encounter->GetBattleEventHandler();
	return Encounter->FindBattleAbility(Request) != nullptr && (Handler == nullptr || Handler->IsAbilityAllowed(Encounter, Request.AbilityId));
}

ENUTargetPolicy UNUBattleCommandComponent::GetSkillActionOverrideTargetPolicy() const
{
	FNUBattleCommandRequest Request;
	Request.SourceCombatantId = GetCurrentSourceCombatantId();
	Request.AbilityId = ResolveSkillAbilityId();
	const UNUBattleGameplayAbility* Ability = IsValid(Encounter) ? Encounter->FindBattleAbility(Request) : nullptr;
	return Ability != nullptr ? Ability->GetTargetPolicy() : ENUTargetPolicy::Self;
}

FText UNUBattleCommandComponent::GetSkillDisplayName() const
{
	if (!HasSkillActionOverride()) { return Super::GetSkillDisplayName(); }
	FNUBattleCommandRequest Request;
	Request.SourceCombatantId = GetCurrentSourceCombatantId();
	Request.AbilityId = ResolveSkillAbilityId();
	const UNUBattleGameplayAbility* Ability = IsValid(Encounter) ? Encounter->FindBattleAbility(Request) : nullptr;
	return Ability != nullptr ? Ability->GetDisplayName() : Super::GetSkillDisplayName();
}

void UNUBattleCommandComponent::SetEncounter(ANUBattleEncounter* NewEncounter)
{
	if (Encounter == NewEncounter)
	{
		if (IsValid(Encounter))
		{
			SetSessionActive(true);
			HandleBattlePhaseChanged(Encounter->GetPhase(), Encounter->GetTurnNumber());
		}
		return;
	}
	
	// 전투 객체가 바뀌면 같은 턴 번호라도 이전 전투의 선택을 재사용하면 안 됨
	UnbindEncounter();
	ResetSelection();
	Encounter = NewEncounter;
	
	if (!IsValid(Encounter))
	{
		SetSessionActive(false);
		return;
	}

	BindEncounter();
	
	// 세션 통지를 먼저 보내 PlayerController가 WBP를 만든 뒤 선택 변경이 broadcast 되게 함
	SetSessionActive(true);
	HandleBattlePhaseChanged(Encounter->GetPhase(), Encounter->GetTurnNumber());
}

void UNUBattleCommandComponent::BindEncounter()
{
	if (!IsValid(Encounter))
	{
		return;
	}
	
	Encounter->OnBattlePhaseChanged.RemoveAll(this);
	Encounter->OnBattlePhaseChanged.AddUObject(this, &ThisClass::HandleBattlePhaseChanged);
}

void UNUBattleCommandComponent::UnbindEncounter()
{
	if (IsValid(Encounter))
	{
		Encounter->OnBattlePhaseChanged.RemoveAll(this);
	}
	Encounter = nullptr;
}

void UNUBattleCommandComponent::HandleBattlePhaseChanged(ENUBattlePhase NewPhase, int32 NewTurnNumber)
{
	if (!IsValid(Encounter))
	{
		ResetSelection();
		return;
	}
	
	if (NewPhase == ENUBattlePhase::CommandSelection)
	{
		if (GetSelectionTurnNumber() != NewTurnNumber)
		{
			BeginCommandSelection(NewTurnNumber);
		}
		return;
	}
	
	ResetSelection();
}

bool UNUBattleCommandComponent::IsBattleActive() const
{
	return IsValid(Encounter);
}

void UNUBattleCommandComponent::RebuildControllableCombatants()
{
	ControllableCombatantIds.Reset();

	if (!IsValid(Encounter))
	{
		return;
	}

	APlayerState* LocalPlayerState = GetLocalPlayerState();
	if (IsValid(LocalPlayerState))
	{
		Encounter->GetCommandableCombatantIds(LocalPlayerState, ControllableCombatantIds);
	}
}

void UNUBattleCommandComponent::CollectLivingCombatantIds(ENUBattleTeam Team, TArray<int32>& OutIds) const
{
	OutIds.Reset();
	if (IsValid(Encounter))
	{
		Encounter->GetLivingCombatantIds(Team, OutIds);
	}
}

void UNUBattleCommandComponent::CollectDownedCombatantIds(ENUBattleTeam Team, TArray<int32>& OutIds) const
{
	OutIds.Reset();
	if (IsValid(Encounter))
	{
		Encounter->GetDownedCombatantIds(Team, OutIds);
	}
}

void UNUBattleCommandComponent::CollectBattleUsableItems(TArray<FNUItemInstance>& OutItems) const
{
	Super::CollectBattleUsableItems(OutItems);

	// 로컬 전투는 아이템 효과를 GE 로 실행하므로 UseEffect 가 없으면 아무 일도 못 함
	// ExecuteItemCommand 가 같은 검사로 거절하는데, 
	// 목록에 남겨두면 고르고 턴만 날리게 되므로 애초에 안 보이게 함
	// 넷은 서버가 실행하므로 이 제약이 없음, 치료제·부활이 넷에서만 뜨는 이유
	const UNUInventoryComponent* Inventory = GetInventoryComponent();
	OutItems.RemoveAll([Inventory](const FNUItemInstance& Item)
	{
		const FNUItemDefRow* Def =
			IsValid(Inventory) ? Inventory->FindItemDef(Item.ItemRowName) : nullptr;
		if (Def == nullptr)
		{
			return true;
		}

		return Def->ItemType != ENUItemType::Commune && !Def->UseEffect;
	});
}

AActor* UNUBattleCommandComponent::GetCombatantAvatar(int32 CombatantId) const
{
	FNUBattleParticipant Participant;
	return GetParticipant(CombatantId, Participant) ? Participant.AvatarActor : nullptr;
}

FText UNUBattleCommandComponent::GetCombatantDisplayName(int32 CombatantId) const
{
	FNUBattleParticipant Participant;
	if (!GetParticipant(CombatantId, Participant))
	{
		return FText::GetEmpty();
	}

	// 나이아(아군 정령·적)는 정의에서, 플레이어 캐릭터는 고른 캐릭터 닉네임에서 가져옴
	if (const ANUSpiritState* Spirit = Cast<ANUSpiritState>(Participant.ASCOwner))
	{
		return FindNaiaDisplayName(Spirit->GetSpiritId());
	}

	// 정령 상태가 없는 적(배치·디버그 전투의 적)은 적 표의 나이아 번호로 --- 머리 위 상태 표시와 같은 이름
	if (Participant.Team == ENUBattleTeam::Enemy)
	{
		return FindNaiaDisplayName(FCString::Atoi(*Participant.EnemySpec.EnemyId.ToString()));
	}

	return GetPlayerCharacterDisplayName();
}

float UNUBattleCommandComponent::GetCombatantSP(int32 CombatantId) const
{
	const UNUAbilitySystemComponent* ASC = GetCombatantASC(CombatantId);
	return IsValid(ASC)
		? ASC->GetNumericAttribute(UNUVitalAttributeSet::GetSPAttribute())
		: 0.f;
}

bool UNUBattleCommandComponent::IsPlayerCombatant(int32 CombatantId) const
{
	return IsValid(Encounter) && Encounter->IsPlayerCombatant(CombatantId);
}

void UNUBattleCommandComponent::CollectOwnedActiveSkillIds(int32 CombatantId, TArray<int32>& OutSkillIds) const
{
	OutSkillIds.Reset();

	FNUBattleParticipant Participant;
	if (!GetParticipant(CombatantId, Participant))
	{
		return;
	}

	const UNUAbilitySystemComponent* ASC = GetCombatantASC(CombatantId);
	if (IsValid(ASC) && ASC->HasStatusEffectMatching(
		[](const FNUStatusEffectDefinition& Definition)
		{
			return Definition.bBlockActiveSkill;
		}))
	{
		return;
	}

	// 현재 전투원 소유자를 기준으로 읽음, 
	// 컨트롤러의 PlayerState를 읽으면 나이아 차례에도 플레이어 직업 스킬이 섞임
	if (const ANUPlayerState* PlayerState = Cast<ANUPlayerState>(Participant.ASCOwner))
	{
		if (const UNUPlayerSkillComponent* Skills = PlayerState->GetPlayerSkillComponent())
		{
			OutSkillIds = Skills->GetOwnedActiveSkillIds();
		}
		return;
	}

	const ANUSpiritState* Naia = Cast<ANUSpiritState>(Participant.ASCOwner);
	if (!IsValid(Naia))
	{
		return;
	}
	
	// 속성을 가려 막는 봉인은 목록을 비우지 않고 그 속성만 걸러냄 (2109 수중 감옥)
	// 실명처럼 전부 막는 것은 위에서 이미 막음
	for (int32 SkillId : Naia->GetActiveSkillIds())
	{
		if (!IsValid(Encounter) || !Encounter->IsSkillBlocked(CombatantId, SkillId))
		{
			OutSkillIds.Add(SkillId);
		}
	}
}

int32 UNUBattleCommandComponent::ApplyStatusSPCost(int32 CombatantId, int32 BaseCost) const
{
	const UNUAbilitySystemComponent* ASC = GetCombatantASC(CombatantId);
	return IsValid(ASC) ? ASC->ApplyStatusSPCost(BaseCost) : BaseCost;
}
int32 UNUBattleCommandComponent::ApplySkillSPCost(int32 CombatantId, int32 BaseCost) const
{
	const UNUAbilitySystemComponent* ASC = GetCombatantASC(CombatantId);
	return IsValid(ASC) ? ASC->ApplySkillSPCost(BaseCost) : BaseCost;
}

int32 UNUBattleCommandComponent::GetActiveSkillUseCount(int32 CombatantId, int32 SkillId) const
{
	const UNUAbilitySystemComponent* ASC = GetCombatantASC(CombatantId);
	return IsValid(ASC) ? ASC->GetActiveSkillUseCount(SkillId) : 0;
}

bool UNUBattleCommandComponent::GetPendingSPRule(FNUCommandSPRule& OutRule) const
{
	OutRule = FNUCommandSPRule();

	if (PendingSkillId > 0)
	{
		FNUBattleSkillInfo Info;
		if (!TryGetSkillInfo(PendingSkillId, Info))
		{
			return false;
		}

		OutRule.FixedCost = ResolveSkillSPCost(PendingSkillId, Info.SPCost);
		OutRule.FixedHitCount = Info.HitCount;
		return true;
	}

	const UNUPlayerBattleAbility* Ability = FindPendingAbility();
	if (!Ability)
	{
		// 도주처럼 SP 규칙이 없는 행동도 0 고정 비용으로 제출한다.
		return PendingRootAction == ENUBattleRootAction::Flee
			|| PendingRootAction == ENUBattleRootAction::Item;
	}

	OutRule.bRepeatable = Ability->IsRepeatable();
	OutRule.FixedCost = ApplyStatusSPCost(GetCurrentSourceCombatantId(), Ability->GetSPCost());
	OutRule.CostPerHit = FMath::Max(
		1,
		ApplyStatusSPCost(GetCurrentSourceCombatantId(), Ability->GetSPCostPerHit()));
	return true;
}

bool UNUBattleCommandComponent::SubmitPendingCommand(int32 SelectedTargetId)
{
	if (!IsValid(Encounter))
	{
		return false;
	}

	ANUPlayerController* PlayerController = Cast<ANUPlayerController>(GetOwner());
	const int32 SourceCombatantId = GetCurrentSourceCombatantId();
	if (!IsValid(PlayerController)
		|| !PlayerController->IsLocalController()
		|| SourceCombatantId == INDEX_NONE)
	{
		return false;
	}

	FNUBattleCommandRequest Request;
	Request.TurnNumber = GetSelectionTurnNumber();
	Request.SourceCombatantId = SourceCombatantId;
	Request.SelectedTargetId = SelectedTargetId;
	Request.SPSpend = PendingSPSpend;

	// 제출할 행동으로 판단해야함
	if (PendingRootAction == ENUBattleRootAction::Item)
	{
		if (!PendingItemInstanceId.IsValid())
		{
			UE_LOG(LogTemp, Warning, TEXT("아이템 명령인데 고른 아이템이 없어 제출하지 않음"));

			return false;
		}

		Request.CommandType = ENUBattleCommandType::Item;
		Request.InstanceId = PendingItemInstanceId;
	}
	else
	{
		Request.CommandType = ENUBattleCommandType::Ability;
		Request.SkillId = PendingSkillId;

		switch (PendingRootAction)
		{
		case ENUBattleRootAction::Attack:
			Request.AbilityId = ResolveAttackAbilityId();
			break;
		case ENUBattleRootAction::Skill:
			Request.AbilityId = ResolveSkillAbilityId();
			break;
		case ENUBattleRootAction::Flee:
			Request.AbilityId = NU::Ability::Battle::Flee;
			break;
		default:
			return false;
		}
	}

	PlayerController->SubmitBattleCommandFromComponent(Encounter->GetEncounterId(), Request);
	return true;
}

void UNUBattleCommandComponent::RequestNaiaSwap(int32 PartySlot)
{
	if (ANUPlayerController* PlayerController = Cast<ANUPlayerController>(GetOwner()))
	{
		PlayerController->ServerRequestNaiaSwap(PartySlot);
	}
}

void UNUBattleCommandComponent::HandleSubmissionResult(FGuid EncounterId, int32 TurnNumber,
	int32 SourceCombatantId, ENUBattleCommandSubmitResult Result)
{
	// 넷 중 하나라도 어긋나면 결과를 버리는데, 그러면 선택창이 제출 상태에서 안 빠져나옴
	// 화면에는 멈춘 것만 보이므로 어느 쪽이 어긋났는지 남겨야 함
	if (!IsValid(Encounter)
		|| Encounter->GetEncounterId() != EncounterId
		|| GetSelectionTurnNumber() != TurnNumber
		|| PendingSubmissionSourceId != SourceCombatantId)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("제출 결과를 버림 (전투원 %d, 결과 %d): 인카운터=%s 턴=%d/%d 대기중인전투원=%d"),
			SourceCombatantId,
			static_cast<int32>(Result),
			!IsValid(Encounter) ? TEXT("없음")
				: (Encounter->GetEncounterId() != EncounterId ? TEXT("다름") : TEXT("같음")),
			GetSelectionTurnNumber(),
			TurnNumber,
			PendingSubmissionSourceId);
		return;
	}

	if (Result == ENUBattleCommandSubmitResult::Accepted)
	{
		HandleSubmitAccepted();
	}
	else
	{
		HandleSubmitRejected(Result);
	}
}

bool UNUBattleCommandComponent::GetParticipant(int32 CombatantId, FNUBattleParticipant& OutParticipant) const
{
	return IsValid(Encounter)
		&& CombatantId != INDEX_NONE
		&& Encounter->GetParticipant(CombatantId, OutParticipant);
}

const UNUPlayerBattleAbility* UNUBattleCommandComponent::FindPendingAbility() const
{
	if (!IsValid(Encounter))
	{
		return nullptr;
	}

	FGameplayTag AbilityId;
	switch (PendingRootAction)
	{
	case ENUBattleRootAction::Attack:
		AbilityId = ResolveAttackAbilityId();
		break;
	case ENUBattleRootAction::Skill:
		AbilityId = ResolveSkillAbilityId();
		break;
	case ENUBattleRootAction::Flee:
		AbilityId = NU::Ability::Battle::Flee;
		break;
	default:
		return nullptr;
	}

	FNUBattleCommandRequest Request;
	Request.SourceCombatantId = GetCurrentSourceCombatantId();
	Request.AbilityId = AbilityId;
	Request.SkillId = PendingSkillId;

	return Cast<UNUPlayerBattleAbility>(Encounter->FindBattleAbility(Request));
}

const UNUAbilitySystemComponent* UNUBattleCommandComponent::GetCombatantASC(int32 CombatantId) const
{
	FNUBattleParticipant Participant;
	if (!GetParticipant(CombatantId, Participant))
	{
		return nullptr;
	}

	return Cast<UNUAbilitySystemComponent>(
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Participant.ASCOwner));
}
