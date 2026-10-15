// Fill out your copyright notice in the Description page of Project Settings.


#include "NUBattleCommandComponentBase.h"

#include "Audio/NUSFXSubsystem.h"
#include "Audio/NUSFXTags.h"
#include "Battle/NUBattleTargetIndicator.h"
#include "Battle/Avatar/NUBattleAvatar.h"
#include "Battle/Data/NUActiveSkillDefinition.h"
#include "Battle/Data/NUBattleDataSubsystem.h"
#include "Battle/Data/NUJobSkillContext.h"
#include "Battle/Data/NUBattleSkillInfo.h"
#include "Battle/Data/NUStatusEffectDefinition.h"
#include "CharacterSelect/NUCharacterManager.h"
#include "CharacterState/NUPlayerState.h"
#include "CharacterState/NUSpiritState.h"
#include "Engine/GameInstance.h"
#include "Naia/Component/NUNaiaRosterComponent.h"
#include "Naia/Data/NUNaiaDataSubsystem.h"
#include "Naia/Data/NUNaiaDefinition.h"
#include "NUGAS/NUGameplayTags.h"
#include "Engine/World.h"
#include "PlayerController/NUPlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Inventory/NUInventoryComponent.h"
#include "Inventory/NUInventoryTypes.h"
#include "NUGAS/NUAbilitySystemComponent.h"
#include "NUGAS/Ability/NUBattleGameplayAbility.h"
#include "NUGAS/Attribute/NUVitalAttributeSet.h"

namespace
{
	constexpr ENUBattleRootAction RootActionOrder[] =
	{
		ENUBattleRootAction::Attack,
		ENUBattleRootAction::Skill,
		ENUBattleRootAction::Item,
		ENUBattleRootAction::NaiaSwap,
		ENUBattleRootAction::Flee
	};
}

UNUBattleCommandComponentBase::UNUBattleCommandComponentBase()
{
	PrimaryComponentTick.bCanEverTick = false;

	SetIsReplicated(false);
}

void UNUBattleCommandComponentBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetSessionActive(false);
	DestroyTargetIndicator();
	ResetSelection();
	Super::EndPlay(EndPlayReason);
}

void UNUBattleCommandComponentBase::SetSessionActive(bool bActive)
{
	if (bSessionActive == bActive)
	{
		return;
	}
	
	bSessionActive = bActive;
	
	if (!bSessionActive)
	{
		ResetSelection();
	}
	OnBattleCommandSessionChanged.Broadcast(this, bSessionActive);
}

void UNUBattleCommandComponentBase::BeginCommandSelection(int32 TurnNumber)
{
	ANUPlayerController* PlayerController = Cast<ANUPlayerController>(GetOwner());
	if (!IsValid(PlayerController)
		|| !PlayerController->IsLocalController()
		|| !IsBattleActive())
	{
		ResetSelection();
		return;
	}
	
	// 같은 턴에 다시 불려도(다음 전투원, 서버 순서 재동기) 이미 선택 중이면 시작음을 또 내지 않음
	const bool bTurnStart = SelectionPhase == ENUCommandSelectionPhase::Inactive
		|| SelectionPhase == ENUCommandSelectionPhase::WaitingForOthers;

	SelectionTurnNumber = TurnNumber;
	
	ControllableCombatantIds.Reset();
	SelectableTargetIds.Reset();
	
	// 타겟
	CurrentCombatantIndex = INDEX_NONE;
	CurrentTargetIndex = INDEX_NONE;
	
	// 아이템
	PendingItemInstanceId.Invalidate();
	SelectableItems.Reset();
	CurrentItemIndex = INDEX_NONE;
	
	// 스킬
	SelectableSkillIds.Reset();
	CurrentSkillIndex = INDEX_NONE;
	PendingSkillId = 0;
	
	PendingRootAction = ENUBattleRootAction::Attack;
	PendingSubmissionSourceId = INDEX_NONE;
	
	RebuildControllableCombatants();
	
	// 내가 조종할 수 있는 전투원이 없다면 다른 플레이어 기다리기
	//  -> 전투중에 내 전투원이 모두 기절하거나 사망
	if (ControllableCombatantIds.IsEmpty())
	{
		SetSelectionPhase(ENUCommandSelectionPhase::WaitingForOthers);
		return;
	}
	
	if (bTurnStart)
	{
		UNUSFXSubsystem::Play(this, NU::SFX::Battle::TurnStart);
	}

	CurrentCombatantIndex = 0;
	OpenRootMenuForCurrentCombatant();
}

void UNUBattleCommandComponentBase::OpenRootMenuForCurrentCombatant()
{
	// 취소로 돌아온 경우 예약이 남아 게이지가 줄어든 채로 보이므로 되돌림
	// 아래에서 전투원을 못 찾으면 아바타도 못 찾으니 먼저 처리해야 함
	ClearPendingSPSpend();

	// 현재 전투원 Index가 유효하지 않다면 다른 플레이어 기다리기 상태로 전환
	if (!ControllableCombatantIds.IsValidIndex(CurrentCombatantIndex))
	{
		SetSelectionPhase(ENUCommandSelectionPhase::WaitingForOthers);
		return;
	}
	
	// 타겟
	SelectableTargetIds.Reset();
	CurrentTargetIndex = INDEX_NONE;
	bTargetsDowned = false;
	
	// 아이템
	SelectableItems.Reset();
	CurrentItemIndex = INDEX_NONE;
	PendingItemInstanceId.Invalidate();	
	
	// 스킬
	SelectableSkillIds.Reset();
	CurrentSkillIndex = INDEX_NONE;
	PendingSkillId = 0;
	
	// 예약
	PendingRootAction = ENUBattleRootAction::Attack;
	PendingSubmissionSourceId = INDEX_NONE;

	int32 SourceCombatantId = GetCurrentSourceCombatantId();
	if (SourceCombatantId != LastPresentedSourceCombatantId)
	{
		LastPresentedSourceCombatantId = SourceCombatantId;
		SelectedRootAction = FindFirstAvailableRootAction();
	}

	SetSelectionPhase(ENUCommandSelectionPhase::RootMenu);
}

void UNUBattleCommandComponentBase::AdvanceToNextCombatant()
{
	++CurrentCombatantIndex;
	
	// 선택한 전투원의 RootMenu 열어주기
	if (ControllableCombatantIds.IsValidIndex(CurrentCombatantIndex))
	{
		OpenRootMenuForCurrentCombatant();
		return;
	}
	
	// 더이상 명령을 내릴 전투원이 없다면 대기
	SetSelectionPhase(ENUCommandSelectionPhase::WaitingForOthers);
}

void UNUBattleCommandComponentBase::NavigateSelection(const FVector2D& Navigation)
{
	switch (SelectionPhase)
	{
	case ENUCommandSelectionPhase::RootMenu:
		if (FMath::Abs(Navigation.Y) > 0.5f)
		{
			MoveRootSelection(Navigation.Y < 0.f ? 1 : -1);
		}
		break;

	// 대상은 ↑↓ (Y축), 투입 SP는 ←→ (X축)
	case ENUCommandSelectionPhase::TargetSelection:
		if (FMath::Abs(Navigation.X) >= FMath::Abs(Navigation.Y))
		{
			if (Navigation.X > 0.5f)
			{
				AdjustSPSpend(1);
			}
			else if (Navigation.X < -0.5f)
			{
				AdjustSPSpend(-1);
			}
		}
		else
		{
			if (Navigation.Y < -0.5f)
			{
				SelectNextTarget();
			}
			else if (Navigation.Y > 0.5f)
			{
				SelectPreviousTarget();
			}
		}
		break;

	case ENUCommandSelectionPhase::ItemList:
		if (FMath::Abs(Navigation.X) >= FMath::Abs(Navigation.Y))
		{
			if (Navigation.X > 0.5f)
			{
				SelectNextItem();
			}
			else if (Navigation.X < -0.5f)
			{
				SelectPreviousItem();
			}
		}
		else
		{
			if (Navigation.Y < -0.5f)
			{
				SelectNextItem();
			}
			else if (Navigation.Y > 0.5f)
			{
				SelectPreviousItem();
			}
		}
		break;

	case ENUCommandSelectionPhase::SkillList:
		if (FMath::Abs(Navigation.X) >= FMath::Abs(Navigation.Y))
		{
			if (Navigation.X > 0.5f)
			{
				SelectNextSkill();
			}
			else if (Navigation.X < -0.5f)
			{
				SelectPreviousSkill();
			}
		}
		else
		{
			if (Navigation.Y < -0.5f)
			{
				SelectNextSkill();
			}
			else if (Navigation.Y > 0.5f)
			{
				SelectPreviousSkill();
			}
		}
		break;

		// 아이템 목록과 같이 네 방향 모두로 순회함
	case ENUCommandSelectionPhase::NaiaList:
		if (FMath::Abs(Navigation.X) >= FMath::Abs(Navigation.Y))
		{
			if (Navigation.X > 0.5f)
			{
				SelectNextNaia();
			}
			else if (Navigation.X < -0.5f)
			{
				SelectPreviousNaia();
			}
		}
		else
		{
			if (Navigation.Y < -0.5f)
			{
				SelectNextNaia();
			}
			else if (Navigation.Y > 0.5f)
			{
				SelectPreviousNaia();
			}
		}
		break;

	default:
		break;
	}
}

void UNUBattleCommandComponentBase::ConfirmSelection()
{
	switch (SelectionPhase)
	{
	case ENUCommandSelectionPhase::RootMenu:
		SelectRootAction(SelectedRootAction);
		break;

	case ENUCommandSelectionPhase::TargetSelection:
		ConfirmTarget();
		break;

	case ENUCommandSelectionPhase::ItemList:
		ConfirmItem();
		break;

	case ENUCommandSelectionPhase::SkillList:
		ConfirmSkill();
		break;

		// 아이템과 달리 대상 선택 단계가 없어 여기서 바로 확정됨
	case ENUCommandSelectionPhase::NaiaList:
		ConfirmNaia();
		break;

	default:
		break;
	}
}

void UNUBattleCommandComponentBase::SelectRootAction(ENUBattleRootAction Action)
{
	if (SelectionPhase != ENUCommandSelectionPhase::RootMenu) return;
	
	// 사용 불가능한 액션이면 알리기
	if (!IsRootActionAvailable(Action))
	{
		UNUSFXSubsystem::Play(this, NU::SFX::UI::Error);
		OnActionUnavailable.Broadcast(Action);
		return;
	}

	SelectedRootAction = Action;
	BroadcastSelectionChanged();
	
	switch (Action)
	{
	case ENUBattleRootAction::Attack:
		BeginAttackTargetSelection();
		break;
		
	case ENUBattleRootAction::Item:
		BeginItemSelection();
		break;

		// 턴을 소비하지 않는 자유 행동, 명령을 제출하지 않고 목록만 열음
	case ENUBattleRootAction::NaiaSwap:
		BeginNaiaSelection();
		break;
		
	case ENUBattleRootAction::Flee:
		PendingItemInstanceId.Invalidate();
		PendingSkillId = 0;
		PendingRootAction = ENUBattleRootAction::Flee;
		BeginTargetSelection(ENUTargetPolicy::Self);
		break;
		
	case ENUBattleRootAction::Skill:
		if (HasSkillActionOverride())
		{
			PendingItemInstanceId.Invalidate();
			PendingSkillId = 0;
			PendingRootAction = ENUBattleRootAction::Skill;
			PendingSPSpend = 0;
			AdjustSPSpend(0);
			BeginTargetSelection(GetSkillActionOverrideTargetPolicy());
		}
		else { BeginSkillSelection(); }
		break;

	default:
		UNUSFXSubsystem::Play(this, NU::SFX::UI::Error);
		OnActionUnavailable.Broadcast(Action);
		break;
	}
}

void UNUBattleCommandComponentBase::MoveRootSelection(int32 Direction)
{
	if (Direction == 0) return;

	int32 ActionCount = UE_ARRAY_COUNT(RootActionOrder);
	int32 CurrentIndex = 0;

	for (int32 Index = 0; Index < ActionCount; ++Index)
	{
		if (RootActionOrder[Index] == SelectedRootAction)
		{
			CurrentIndex = Index;
			break;
		}
	}

	for (int32 Step = 1; Step <= ActionCount; ++Step)
	{
		int32 CandidateIndex =
			(CurrentIndex + Direction * Step + ActionCount * Step)
			% ActionCount;

		ENUBattleRootAction Candidate = RootActionOrder[CandidateIndex];
		if (IsRootActionAvailable(Candidate))
		{
			// 고를 수 있는 항목이 하나뿐이면 제자리라 소리를 내지 않음
			if (Candidate != SelectedRootAction)
			{
				UNUSFXSubsystem::Play(this, NU::SFX::UI::Cursor);
			}
			SelectedRootAction = Candidate;
			BroadcastSelectionChanged();
			return;
		}
	}
}

ENUBattleRootAction UNUBattleCommandComponentBase::FindFirstAvailableRootAction() const
{
	for (const ENUBattleRootAction Action : RootActionOrder)
	{
		if (IsRootActionAvailable(Action))
		{
			return Action;
		}
	}

	return ENUBattleRootAction::Attack;
}

void UNUBattleCommandComponentBase::BeginTargetSelection(ENUTargetPolicy Policy, bool bDownedTargets)
{
	if (!IsBattleActive()) return;

	// 대상 선택으로 들어오는 길이 전부 이 함수를 지나므로 여기서 한 번만 세움
	bTargetsDowned = bDownedTargets;
	
	// 타겟 선택이 필요없다면 바로 제출
	if (!RequiresTargetSelection(Policy))
	{
		// 생략하면 서버가 고름 - 부활은 첫 쓰러진 아군
		SubmitCurrentCommand(INDEX_NONE);
		return;
	}
	
	if (bDownedTargets)
	{
		CollectDownedCombatantIds(GetPolicyTeam(Policy), SelectableTargetIds);
	}
	else
	{
		CollectLivingCombatantIds(GetPolicyTeam(Policy), SelectableTargetIds);
	}
	
	if (SelectableTargetIds.IsEmpty())
	{
		// 고를 대상이 없어 되돌아 나감, 화면에서는 아무 일도 안 일어난 것처럼 보여서 남김
		// 부활 아이템을 아무도 안 쓰러졌을 때 고른 경우
		UE_LOG(LogTemp, Log, TEXT("대상이 없어 선택으로 못 넘어감 (정책 %d, 쓰러진 대상 %s)"),
			static_cast<int32>(Policy), bDownedTargets ? TEXT("true") : TEXT("false"));

		UNUSFXSubsystem::Play(this, NU::SFX::UI::Error);
		OnActionUnavailable.Broadcast(SelectedRootAction);
		return;
	}
	
	UNUSFXSubsystem::Play(this, NU::SFX::UI::Confirm);
	CurrentTargetIndex = 0;
	SetSelectionPhase(ENUCommandSelectionPhase::TargetSelection);
}

void UNUBattleCommandComponentBase::BeginAttackTargetSelection()
{
	// Ability 제출 정보 확정하고 선택으로 넘어감
	PendingItemInstanceId.Invalidate();
	PendingSkillId = 0;
	PendingRootAction = ENUBattleRootAction::Attack;

	// 1타 에서 시작해 ←/→ 로 늘린다
	PendingSPSpend = 0;
	AdjustSPSpend(0);

	BeginTargetSelection(ENUTargetPolicy::SingleEnemy);
}

void UNUBattleCommandComponentBase::AdjustSPSpend(int32 Direction)
{
	FNUCommandSPRule Rule;
	if (!GetPendingSPRule(Rule))
	{
		PendingSPSpend = 0;
		PushPendingSPSpendToAvatar();
		BroadcastSelectionChanged();
		return;
	}
	if (!Rule.bRepeatable)
	{
		PendingSPSpend = FMath::Max(0, Rule.FixedCost);
		PushPendingSPSpendToAvatar();
		BroadcastSelectionChanged();
		return;
	}
	const int32 PerHit = FMath::Max(1, Rule.CostPerHit);
	const int32 MaxHits = FMath::FloorToInt(GetCurrentSourceSP()) / PerHit;
	const int32 MinHits = FMath::Min(1, MaxHits);
	const int32 CurrentHits = FMath::Clamp(PendingSPSpend / PerHit, MinHits, MaxHits);
	const int32 NewHits = FMath::Clamp(CurrentHits + Direction, MinHits, MaxHits);
	if (NewHits != CurrentHits)
	{
		UNUSFXSubsystem::Play(this, NU::SFX::UI::Step);
	}
	PendingSPSpend = NewHits * PerHit;
	PushPendingSPSpendToAvatar();
	BroadcastSelectionChanged();
}

float UNUBattleCommandComponentBase::GetCurrentSourceSP() const
{
	return GetCombatantSP(GetCurrentSourceCombatantId());
}

// 상태이상 보정이 붙은 스킬 SP 소모량
// 목록 라벨과 예정 소모량이 같은 값을 봐야 함
int32 UNUBattleCommandComponentBase::GetAdjustedSkillSPCost(int32 SkillId) const
{
	FNUBattleSkillInfo Info;
	return TryGetSkillInfo(SkillId, Info)
		? ResolveSkillSPCost(SkillId, Info.SPCost)
		: 0;
}

int32 UNUBattleCommandComponentBase::ResolveSkillSPCost(int32 SkillId, int32 TableCost) const
{
	if (NUJobSkill::FindSpendAllSPSkill(SkillId))
	{
		return FMath::FloorToInt(GetCurrentSourceSP());
	}

	return ApplySkillSPCost(GetCurrentSourceCombatantId(), TableCost);
}

// 상태이상 보정이 붙은 타당 SP 소모량
int32 UNUBattleCommandComponentBase::GetAdjustedSPCostPerHit() const
{
	FNUCommandSPRule Rule;
	return GetPendingSPRule(Rule) && Rule.bRepeatable ? FMath::Max(1, Rule.CostPerHit) : 0;
}

void UNUBattleCommandComponentBase::PushPendingSPSpendToAvatar()
{
	// 아직 서버에 안 보낸 값이라 전투원 아바타 UI에 표시만
	if (ANUBattleAvatar* Avatar = Cast<ANUBattleAvatar>(GetCurrentSourceAvatar()))
	{
		Avatar->SetPendingSPSpend(static_cast<float>(PendingSPSpend));
	}
}

// 예약을 취소하고 게이지도 되돌림
// 루트 메뉴로 돌아갈 때, 그리고 제출이 받아들여져 서버가 실제로 차감했을 때 부름
void UNUBattleCommandComponentBase::ClearPendingSPSpend()
{
	PendingSPSpend = 0;
	PushPendingSPSpendToAvatar();
}

int32 UNUBattleCommandComponentBase::GetPendingSPSpend() const
{
	return PendingSPSpend;
}

int32 UNUBattleCommandComponentBase::GetPendingHitCount() const
{
	FNUCommandSPRule Rule;
	if (!GetPendingSPRule(Rule)) return 1;
	if (!Rule.bRepeatable) return FMath::Max(1, Rule.FixedHitCount);
	return FMath::Max(0, PendingSPSpend) / FMath::Max(1, Rule.CostPerHit);
}

int32 UNUBattleCommandComponentBase::GetMaxHitCount() const
{
	FNUCommandSPRule Rule;
	if (!GetPendingSPRule(Rule)) return 1;
	if (!Rule.bRepeatable) return FMath::Max(1, Rule.FixedHitCount);
	return FMath::FloorToInt(GetCurrentSourceSP()) / FMath::Max(1, Rule.CostPerHit);
}

void UNUBattleCommandComponentBase::BeginItemSelection()
{
	if (!IsBattleActive()) return;
	
	CollectBattleUsableItems(SelectableItems);
	
	if (SelectableItems.IsEmpty())
	{
		UNUSFXSubsystem::Play(this, NU::SFX::UI::Error);
		OnActionUnavailable.Broadcast(ENUBattleRootAction::Item);
		return;
	}
	
	UNUSFXSubsystem::Play(this, NU::SFX::UI::Confirm);
	PendingRootAction = ENUBattleRootAction::Item;
	PendingSkillId = 0;
	CurrentItemIndex = 0;
	SetSelectionPhase(ENUCommandSelectionPhase::ItemList);
}

void UNUBattleCommandComponentBase::BeginItemTargetSelection(const FNUItemInstance& Item)
{
	UNUInventoryComponent* Inventory = GetInventoryComponent();
	const FNUItemDefRow* Def = IsValid(Inventory) ? Inventory->FindItemDef(Item.ItemRowName) : nullptr;
	if (!Def) return;
	
	// 제출정보 미리 확정하고 타겟 선택으로 넘어감
	PendingItemInstanceId = Item.InstanceId;
	PendingSkillId = 0;
	PendingRootAction = ENUBattleRootAction::Item;

	// 아이템은 SP 소모 X
	PendingSPSpend = 0;

	// 부활만 대상이 반대, 대상 정책(SingleAlly)은 그대로 두고 누구를 모을지만 바꿈
	BeginTargetSelection(Def->BattleTargetPolicy, Def->Effect1_Type == ENUItemEffectType::Revive);
}

void UNUBattleCommandComponentBase::BeginSkillTargetSelection(int32 SkillId)
{
	if (!IsBattleActive())
	{
		return;
	}

	// 발동 전이라 정의에서 직접 읽어야 함
	FNUBattleSkillInfo Info;
	if (!TryGetSkillInfo(SkillId, Info))
	{
		UE_LOG(LogTemp, Warning, TEXT("스킬 정의가 없어 대상 선택을 시작할 수 없습니다 : %d"), SkillId);
		return;
	}

	// 표가 대상 범위를 안 보낸 줄, 누구에게 나갈지 모르면 고르게 할 수 없음
	// 지어내면 단일 스킬이 전체 공격으로 둔갑함
	if (!Info.TargetPolicy.IsSet())
	{
		UE_LOG(LogTemp, Warning, TEXT("스킬 %d 의 대상 범위를 몰라 대상 선택을 시작할 수 없습니다"), SkillId);
		UNUSFXSubsystem::Play(this, NU::SFX::UI::Error);
		OnActionUnavailable.Broadcast(ENUBattleRootAction::Skill);
		return;
	}

	// 목록을 만들 때 CollectUsableSkills 가 이미 걸러내지만 여기는 밖에서도 부를 수 있음
	// 필살기(AG 없음)와 자해 스킬(지불 자리 없음)이 여기로 들어오면 턴만 버림
	// 스킬을 쓸 수 있는지를 서버가 정하는 전투는 아래 로컬 검사를 건너뜀, 서버가 거절하면 사유가 로그로 남음
	const bool bServerDriven = IsSkillAvailabilityServerDriven();

	if (!Info.bExecutable && bServerDriven)
	{
		UE_LOG(LogTemp, Log, TEXT("스킬 %d 는 로컬 표로는 못 돌지만 서버 목록에 있어 그대로 진행합니다"), SkillId);
	}
	else if (!Info.bExecutable)
	{
		UE_LOG(LogTemp, Warning, TEXT("스킬 %d 는 아직 표만으로 못 돌아 대상 선택을 시작하지 않습니다"), SkillId);
		UNUSFXSubsystem::Play(this, NU::SFX::UI::Error);
		OnActionUnavailable.Broadcast(ENUBattleRootAction::Skill);
		return;
	}

	// 목록에서 이미 뺐지만 밖에서도 부를 수 있음, 받을 아군이 없으면 턴과 SP 만 버림
	if (!bServerDriven && Info.Source == ENUBattleSkillSource::Job && !HasJobSkillRecipient(SkillId))
	{
		UE_LOG(LogTemp, Warning, TEXT("스킬 %d 는 받을 아군이 없어 대상 선택을 시작하지 않습니다"), SkillId);
		UNUSFXSubsystem::Play(this, NU::SFX::UI::Error);
		OnActionUnavailable.Broadcast(ENUBattleRootAction::Skill);
		return;
	}

	// 같은 이유로 전투당 횟수를 다 쓴 스킬도 시작하지 않음
	if (!bServerDriven && IsSkillUsesExhausted(Info))
	{
		UE_LOG(LogTemp, Warning, TEXT("스킬 %d 는 이번 전투에서 쓸 수 있는 횟수를 다 써 대상 선택을 시작하지 않습니다"), SkillId);
		UNUSFXSubsystem::Play(this, NU::SFX::UI::Error);
		OnActionUnavailable.Broadcast(ENUBattleRootAction::Skill);
		return;
	}

	PendingItemInstanceId.Invalidate();
	PendingSkillId = SkillId;

	// 실행기는 스킬 전체가 태그 하나를 공유하고, 어느 행인지는 SkillId가 정함
	PendingRootAction = ENUBattleRootAction::Skill;

	// 스킬은 고정 비용이라 조절할 것이 없음, AdjustSPSpend가 정의에서 읽어 채움
	PendingSPSpend = 0;
	AdjustSPSpend(0);

	BeginTargetSelection(Info.TargetPolicy.GetValue());
}

void UNUBattleCommandComponentBase::BeginSkillSelection()
{
	if (!IsBattleActive()) return;

	CollectUsableSkills(SelectableSkillIds);

	if (SelectableSkillIds.IsEmpty())
	{
		UNUSFXSubsystem::Play(this, NU::SFX::UI::Error);
		OnActionUnavailable.Broadcast(ENUBattleRootAction::Skill);
		return;
	}

	UNUSFXSubsystem::Play(this, NU::SFX::UI::Confirm);
	PendingItemInstanceId.Invalidate();
	PendingRootAction = ENUBattleRootAction::Attack;
	PendingSkillId = 0;

	CurrentSkillIndex = 0;
	SetSelectionPhase(ENUCommandSelectionPhase::SkillList);
}

void UNUBattleCommandComponentBase::SelectPreviousSkill()
{
	if (SelectionPhase != ENUCommandSelectionPhase::SkillList
		|| SelectableSkillIds.IsEmpty())
	{
		return;
	}

	int32 SkillCount = SelectableSkillIds.Num();
	CurrentSkillIndex = (CurrentSkillIndex - 1 + SkillCount) % SkillCount;

	if (SkillCount > 1)
	{
		UNUSFXSubsystem::Play(this, NU::SFX::UI::Cursor);
	}
	BroadcastSelectionChanged();
}

void UNUBattleCommandComponentBase::SelectNextSkill()
{
	if (SelectionPhase != ENUCommandSelectionPhase::SkillList
		|| SelectableSkillIds.IsEmpty())
	{
		return;
	}

	CurrentSkillIndex = (CurrentSkillIndex + 1) % SelectableSkillIds.Num();

	if (SelectableSkillIds.Num() > 1)
	{
		UNUSFXSubsystem::Play(this, NU::SFX::UI::Cursor);
	}
	BroadcastSelectionChanged();
}

void UNUBattleCommandComponentBase::ConfirmSkill()
{
	if (SelectionPhase != ENUCommandSelectionPhase::SkillList
		|| !SelectableSkillIds.IsValidIndex(CurrentSkillIndex))
	{
		return;
	}

	BeginSkillTargetSelection(SelectableSkillIds[CurrentSkillIndex]);
}

void UNUBattleCommandComponentBase::CollectUsableSkills(TArray<int32>& OutSkillIds) const
{
	TArray<int32> OwnedSkillIds;
	CollectOwnedActiveSkillIds(GetCurrentSourceCombatantId(), OwnedSkillIds);
	const bool bServerDriven = IsSkillAvailabilityServerDriven();
	OutSkillIds.Reset();
	for (const int32 SkillId : OwnedSkillIds)
	{
		FNUBattleSkillInfo Info;
		if (TryGetSkillInfo(SkillId, Info))
		{
			// 표에는 있는데 아직 못 도는 것, 골라봐야 턴과 SP 만 버리므로 안 올림
			// 아래 "행이 없음" 과는 다른 이유라 로그를 분리함
			// 서버가 목록을 정하는 전투는 서버가 못 돌리면 거절하므로 올림
			if (!bServerDriven && !Info.bExecutable)
			{
				static TSet<int32> LoggedBlocked;
				if (!LoggedBlocked.Contains(SkillId))
				{
					LoggedBlocked.Add(SkillId);
					UE_LOG(LogTemp, Warning,
						TEXT("스킬 %d(%s) 는 아직 표만으로 못 돌아 메뉴에서 제외됩니다"),
						SkillId, *Info.DisplayName.ToString());
				}
				continue;
			}

			// 아군에게 거는 스킬인데 받을 사람이 없으면 턴과 SP 만 버림 (솔로의 3413 생명 전이 · 3428 가시 방벽 · 3429 생명의 결계)
			if (!bServerDriven && Info.Source == ENUBattleSkillSource::Job && !HasJobSkillRecipient(SkillId))
			{
				continue;
			}

			// 전투당 횟수를 다 쓴 스킬 (3310 역방향 · 3328 대역전 · 3329 은자의 등불), 다 쓰면 스킬 루트도 같이 꺼짐
			if (!bServerDriven && IsSkillUsesExhausted(Info))
			{
				continue;
			}

			OutSkillIds.Add(SkillId);
			continue;
		}

		// 보유 목록에는 있는데 표에 행이 없는 경우
		// 메뉴에서 조용히 빠져서 화면에서는 "스킬을 안 배웠다"와 구분이 안 됨
		// 이 함수는 메뉴 활성 여부를 묻느라 매 프레임 불릴 수 있어 번호당 한 번만 남김
		static TSet<int32> LoggedMisses;
		if (!LoggedMisses.Contains(SkillId))
		{
			LoggedMisses.Add(SkillId);
			UE_LOG(LogTemp, Warning,
				TEXT("스킬 %d 의 액티브/필살기 정의를 찾지 못해 메뉴에서 제외됩니다"), SkillId);
		}
	}
}

bool UNUBattleCommandComponentBase::HasJobSkillRecipient(int32 SkillId) const
{
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UNUBattleDataSubsystem* Subsystem =
		IsValid(GameInstance) ? GameInstance->GetSubsystem<UNUBattleDataSubsystem>() : nullptr;

	if (!IsValid(Subsystem))
	{
		return true;
	}

	// 자신을 뺀 살아 있는 같은 편을 캐릭터와 나이아로 가름
	const int32 SourceCombatantId = GetCurrentSourceCombatantId();

	TArray<int32> AllyIds;
	CollectLivingCombatantIds(ENUBattleTeam::Ally, AllyIds);

	bool bOtherCharacterAlive = false;
	bool bNaiaAlive = false;

	for (const int32 AllyId : AllyIds)
	{
		if (AllyId == SourceCombatantId)
		{
			continue;
		}

		if (IsPlayerCombatant(AllyId))
		{
			bOtherCharacterAlive = true;
		}
		else
		{
			bNaiaAlive = true;
		}
	}

	return Subsystem->HasJobSkillRecipient(SkillId, bOtherCharacterAlive, bNaiaAlive);
}

bool UNUBattleCommandComponentBase::IsSkillUsesExhausted(const FNUBattleSkillInfo& Info) const
{
	return Info.MaxUsesPerBattle > 0
		&& GetActiveSkillUseCount(GetCurrentSourceCombatantId(), Info.SkillId) >= Info.MaxUsesPerBattle;
}

bool UNUBattleCommandComponentBase::TryGetSkillInfo(int32 SkillId, FNUBattleSkillInfo& OutInfo) const
{
	OutInfo = FNUBattleSkillInfo();
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UNUBattleDataSubsystem* Subsystem =
		IsValid(GameInstance) ? GameInstance->GetSubsystem<UNUBattleDataSubsystem>() : nullptr;
	
	return IsValid(Subsystem) && Subsystem->TryGetBattleSkillInfo(SkillId, OutInfo);
}

const FNUActiveSkillDefinition* UNUBattleCommandComponentBase::FindSkillDefinition(int32 SkillId) const
{
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UNUBattleDataSubsystem* Subsystem =
		IsValid(GameInstance) ? GameInstance->GetSubsystem<UNUBattleDataSubsystem>() : nullptr;

	return IsValid(Subsystem) ? Subsystem->FindActiveSkillDefinition(SkillId) : nullptr;
}

FText UNUBattleCommandComponentBase::FindNaiaDisplayName(int32 NaiaId) const
{
	if (NaiaId <= 0)
	{
		return FText::GetEmpty();
	}

	const UGameInstance* GameInstance = GetOwner() ? GetOwner()->GetGameInstance() : nullptr;
	const UNUNaiaDataSubsystem* Subsystem =
		IsValid(GameInstance) ? GameInstance->GetSubsystem<UNUNaiaDataSubsystem>() : nullptr;
	const FNUNaiaDefinition* Definition =
		IsValid(Subsystem) ? Subsystem->FindDefinitionById(NaiaId) : nullptr;

	return Definition && !Definition->KoreanName.IsEmpty()
		? Definition->KoreanName
		: FText::GetEmpty();
}

FText UNUBattleCommandComponentBase::GetPlayerCharacterDisplayName() const
{
	const UGameInstance* GameInstance = GetOwner() ? GetOwner()->GetGameInstance() : nullptr;
	const UNUCharacterManager* Manager =
		IsValid(GameInstance) ? GameInstance->GetSubsystem<UNUCharacterManager>() : nullptr;

	FNUCharacterSummary Selected;
	if (!IsValid(Manager) || !Manager->GetSelectedCharacter(Selected) || Selected.Name.IsEmpty())
	{
		return FText::GetEmpty();
	}

	return FText::FromString(Selected.Name);
}

int32 UNUBattleCommandComponentBase::GetCurrentSkillId() const
{
	return SelectableSkillIds.IsValidIndex(CurrentSkillIndex)
		? SelectableSkillIds[CurrentSkillIndex]
		: 0;
}

FText UNUBattleCommandComponentBase::GetCurrentSkillDisplayText() const
{
	FNUBattleSkillInfo Info;
	if (!TryGetSkillInfo(GetCurrentSkillId(), Info))
	{
		return FText::GetEmpty();
	}

	// UI 표시 또한 상태이상 보정된 값으로 표시
	FText Result = FText::Format(NSLOCTEXT("NU", "SkillEntry", "{0}  SP {1}"),
		Info.DisplayName, GetAdjustedSkillSPCost(GetCurrentSkillId()));
	
	if (Info.HPCostRatio > 0.f)
	{
		Result = FText::Format(NSLOCTEXT("NU", "SkillEntryHP", "{0}  최대 HP {1}%"),
			Result, FText::AsNumber(Info.HPCostRatio * 100.f));
	}
	if (Info.AGCost > 0)
	{
		Result = FText::Format(NSLOCTEXT("NU", "SkillEntryAG", "{0}  AG {1}"), Result, Info.AGCost);
	}
	return Result;
}

FText UNUBattleCommandComponentBase::GetSkillPositionText() const
{
	if (SelectableSkillIds.IsEmpty())
	{
		return FText::GetEmpty();
	}

	return FText::Format(NSLOCTEXT("NU", "SkillPos", "{0} / {1}"),
		CurrentSkillIndex + 1, SelectableSkillIds.Num());
}

FText UNUBattleCommandComponentBase::GetCurrentSkillDescription() const
{
	FNUBattleSkillInfo Info;
	return TryGetSkillInfo(GetCurrentSkillId(), Info) ? Info.Description : FText::GetEmpty();
}

void UNUBattleCommandComponentBase::SelectPreviousTarget()
{
	if (SelectionPhase != ENUCommandSelectionPhase::TargetSelection
		|| SelectableTargetIds.IsEmpty())
	{
		return;
	}
	
	int32 TargetCount = SelectableTargetIds.Num();
	CurrentTargetIndex = (CurrentTargetIndex - 1 + TargetCount) % TargetCount;
	
	if (TargetCount > 1)
	{
		UNUSFXSubsystem::Play(this, NU::SFX::UI::Cursor);
	}
	BroadcastSelectionChanged();
}

void UNUBattleCommandComponentBase::SelectNextTarget()
{
	if (SelectionPhase != ENUCommandSelectionPhase::TargetSelection
	|| SelectableTargetIds.IsEmpty())
	{
		return;
	}
	
	CurrentTargetIndex = (CurrentTargetIndex + 1) % SelectableTargetIds.Num();
	
	if (SelectableTargetIds.Num() > 1)
	{
		UNUSFXSubsystem::Play(this, NU::SFX::UI::Cursor);
	}
	BroadcastSelectionChanged();
}

void UNUBattleCommandComponentBase::SelectPreviousItem()
{
	if (SelectionPhase != ENUCommandSelectionPhase::ItemList
		|| SelectableItems.IsEmpty())
	{
		return;
	}
	
	int32 TargetCount = SelectableItems.Num();
	CurrentItemIndex = (CurrentItemIndex - 1 + TargetCount) % TargetCount;
	
	if (TargetCount > 1)
	{
		UNUSFXSubsystem::Play(this, NU::SFX::UI::Cursor);
	}
	BroadcastSelectionChanged();
}

void UNUBattleCommandComponentBase::SelectNextItem()
{
	if (SelectionPhase != ENUCommandSelectionPhase::ItemList
	|| SelectableItems.IsEmpty())
	{
		return;
	}
	
	CurrentItemIndex = (CurrentItemIndex + 1) % SelectableItems.Num();
	
	if (SelectableItems.Num() > 1)
	{
		UNUSFXSubsystem::Play(this, NU::SFX::UI::Cursor);
	}
	BroadcastSelectionChanged();
}

void UNUBattleCommandComponentBase::ConfirmTarget()
{
	if (SelectionPhase != ENUCommandSelectionPhase::TargetSelection
		|| !SelectableTargetIds.IsValidIndex(CurrentTargetIndex))
	{
		return;
	}
	
	SubmitCurrentCommand(SelectableTargetIds[CurrentTargetIndex]);
}

void UNUBattleCommandComponentBase::CancelSelection()
{
	switch (SelectionPhase)
	{
	case ENUCommandSelectionPhase::TargetSelection:
	case ENUCommandSelectionPhase::SkillList:
	case ENUCommandSelectionPhase::ItemList:
	case ENUCommandSelectionPhase::NaiaList:
		UNUSFXSubsystem::Play(this, NU::SFX::UI::Cancel);
		OpenRootMenuForCurrentCombatant();
		break;

	case ENUCommandSelectionPhase::Submitting:
	case ENUCommandSelectionPhase::WaitingForOthers:
	case ENUCommandSelectionPhase::RootMenu:
	case ENUCommandSelectionPhase::Inactive:
	default:
		break;
	}
}

void UNUBattleCommandComponentBase::ConfirmItem()
{
	if (SelectionPhase != ENUCommandSelectionPhase::ItemList
		|| !SelectableItems.IsValidIndex(CurrentItemIndex))
	{
		return;
	}
	
	BeginItemTargetSelection(SelectableItems[CurrentItemIndex]);
}

UNUNaiaRosterComponent* UNUBattleCommandComponentBase::GetNaiaRosterComponent() const
{
	ANUPlayerState* PS = Cast<ANUPlayerState>(GetLocalPlayerState());

	return IsValid(PS) ? PS->GetNaiaRosterComponent() : nullptr;
}

void UNUBattleCommandComponentBase::CollectSwappableNaiaSlots(TArray<int32>& OutSlots) const
{
	OutSlots.Reset();

	const UNUNaiaRosterComponent* Roster = GetNaiaRosterComponent();
	if (!IsValid(Roster)) return;

	// 0번은 이미 활성화된 나이아라 제외하고 탐색
	const TArray<TObjectPtr<ANUSpiritState>>& Party = Roster->GetParty();
	for (int32 Slot = 1; Slot < Party.Num(); ++Slot)
	{
		const ANUSpiritState* Naia = Party[Slot].Get();
		if (!IsValid(Naia)) continue;

		// 사망 상태인 나이아도 제외
		const UNUAbilitySystemComponent* ASC = Naia->GetNUASC();
		if (!IsValid(ASC) || ASC->GetTagCount(NU::State::Dead) > 0) continue;

		OutSlots.Add(Slot);
	}
}

void UNUBattleCommandComponentBase::BeginNaiaSelection()
{
	if (!IsBattleActive() || SelectionPhase != ENUCommandSelectionPhase::RootMenu)
	{
		return;
	}

	CollectSwappableNaiaSlots(SelectableNaiaSlots);

	if (SelectableNaiaSlots.IsEmpty())
	{
		UNUSFXSubsystem::Play(this, NU::SFX::UI::Error);
		OnActionUnavailable.Broadcast(ENUBattleRootAction::NaiaSwap);
		return;
	}

	UNUSFXSubsystem::Play(this, NU::SFX::UI::Confirm);
	CurrentNaiaIndex = 0;
	SetSelectionPhase(ENUCommandSelectionPhase::NaiaList);
}

void UNUBattleCommandComponentBase::SelectNextNaia()
{
	if (SelectionPhase != ENUCommandSelectionPhase::NaiaList || SelectableNaiaSlots.IsEmpty())
	{
		return;
	}

	CurrentNaiaIndex = (CurrentNaiaIndex + 1) % SelectableNaiaSlots.Num();
	if (SelectableNaiaSlots.Num() > 1)
	{
		UNUSFXSubsystem::Play(this, NU::SFX::UI::Cursor);
	}
	BroadcastSelectionChanged();
}

void UNUBattleCommandComponentBase::SelectPreviousNaia()
{
	if (SelectionPhase != ENUCommandSelectionPhase::NaiaList || SelectableNaiaSlots.IsEmpty())
	{
		return;
	}

	CurrentNaiaIndex = (CurrentNaiaIndex - 1 + SelectableNaiaSlots.Num()) % SelectableNaiaSlots.Num();
	if (SelectableNaiaSlots.Num() > 1)
	{
		UNUSFXSubsystem::Play(this, NU::SFX::UI::Cursor);
	}
	BroadcastSelectionChanged();
}

void UNUBattleCommandComponentBase::ConfirmNaia()
{
	if (SelectionPhase != ENUCommandSelectionPhase::NaiaList
		|| !SelectableNaiaSlots.IsValidIndex(CurrentNaiaIndex))
	{
		return;
	}

	// 나이아 교체 시 교체한 턴 기록
	LastNaiaSwapTurn = GetSelectionTurnNumber();
	RequestNaiaSwap(SelectableNaiaSlots[CurrentNaiaIndex]);

	OpenRootMenuForCurrentCombatant();
}

void UNUBattleCommandComponentBase::HandleNaiaSwapResult(ENUNaiaSwapResult Result)
{
	if (Result == ENUNaiaSwapResult::Success) return;

	// 이번 턴에 이미 사용한게 아니라면 다시 교체 시도할 수 있게 되돌림
	if (Result != ENUNaiaSwapResult::AlreadySwappedThisTurn)
	{
		LastNaiaSwapTurn = INDEX_NONE;
	}

	UNUSFXSubsystem::Play(this, NU::SFX::UI::Error);
	OnNaiaSwapFailed.Broadcast(Result);
}

FText UNUBattleCommandComponentBase::GetCurrentNaiaDisplayText() const
{
	const UNUNaiaRosterComponent* Roster = GetNaiaRosterComponent();
	if (!IsValid(Roster) || !SelectableNaiaSlots.IsValidIndex(CurrentNaiaIndex))
	{
		return FText::GetEmpty();
	}

	const TArray<TObjectPtr<ANUSpiritState>>& Party = Roster->GetParty();
	const int32 Slot = SelectableNaiaSlots[CurrentNaiaIndex];
	const ANUSpiritState* Naia = Party.IsValidIndex(Slot) ? Party[Slot].Get() : nullptr;
	if (!IsValid(Naia))
	{
		return FText::GetEmpty();
	}

	// 이름은 나이아 정의에서, 없으면 Id로 대신 표기해 안 채운 데이터가 눈에 띄게
	FText DisplayName = FindNaiaDisplayName(Naia->GetSpiritId());
	if (DisplayName.IsEmpty())
	{
		DisplayName = FText::AsNumber(Naia->GetSpiritId());
	}

	const UNUAbilitySystemComponent* ASC = Naia->GetNUASC();
	const UNUVitalAttributeSet* Vital = IsValid(ASC) ? ASC->GetVitalAttributeSet() : nullptr;
	if (!Vital)
	{
		return DisplayName;
	}

	return FText::Format(
		NSLOCTEXT("NUBattle", "NaiaSwapEntry", "{0} Lv.{1}  HP {2}/{3}"),
		DisplayName,
		FText::AsNumber(Naia->GetCurrentLevel()),
		FText::AsNumber(FMath::CeilToInt(Vital->GetHealth())),
		FText::AsNumber(FMath::CeilToInt(Vital->GetMaxHealth())));
}

FText UNUBattleCommandComponentBase::GetNaiaPositionText() const
{
	if (SelectableNaiaSlots.IsEmpty())
	{
		return FText::GetEmpty();
	}

	return FText::Format(
		NSLOCTEXT("NUBattle", "NaiaSwapPosition", "{0} / {1}"),
		FText::AsNumber(CurrentNaiaIndex + 1),
		FText::AsNumber(SelectableNaiaSlots.Num()));
}

void UNUBattleCommandComponentBase::SubmitCurrentCommand(int32 SelectedTargetId)
{
	const int32 SourceCombatantId = GetCurrentSourceCombatantId();
	if (!IsBattleActive() || SourceCombatantId == INDEX_NONE)
	{
		return;
	}

	PendingSubmissionSourceId = SourceCombatantId;

	// 제출 상태는 반드시 보내기 전에 설정
	// 로컬 전투는 Standalone 이라 SubmitPendingCommand 안에서 RPC·판정·결과 통지·다음 전투원
	// 메뉴 열기까지 전부 동기로 끝난다. 보낸 뒤에 세우면 그 사이 열린 다음 전투원의 선택을
	// 덮어써서 아무도 풀 수 없는 제출 상태에 갇힌다 (PendingSubmissionSourceId 는 이미 비어 있음)
	// 넷 전투는 응답이 나중에 와서 이 순서가 드러나지 않는다
	SetSelectionPhase(ENUCommandSelectionPhase::Submitting);

	if (!SubmitPendingCommand(SelectedTargetId))
	{
		PendingSubmissionSourceId = INDEX_NONE;
		UNUSFXSubsystem::Play(this, NU::SFX::UI::Error);
		OpenRootMenuForCurrentCombatant();
		return;
	}

	UNUSFXSubsystem::PlayCommon(this, NU::SFX::Battle::Commit);
}

void UNUBattleCommandComponentBase::HandleSubmitAccepted()
{
	if (PendingSubmissionSourceId == INDEX_NONE) return;

	// 아래에서 PendingItemInstanceId 를 비우므로 그 전에 예약
	ReserveSubmittedItem();

	PendingSubmissionSourceId = INDEX_NONE;
	SelectableTargetIds.Reset();
	CurrentTargetIndex = INDEX_NONE;
	PendingRootAction = ENUBattleRootAction::Attack;
	PendingItemInstanceId.Invalidate();
	PendingSkillId = 0;
	ClearPendingSPSpend();
	AdvanceToNextCombatant();
}

void UNUBattleCommandComponentBase::HandleSubmitRejected(ENUBattleCommandSubmitResult Result)
{
	if (PendingSubmissionSourceId == INDEX_NONE) return;
	PendingSubmissionSourceId = INDEX_NONE;
	UNUSFXSubsystem::Play(this, NU::SFX::UI::Error);
	OnSubmissionFailed.Broadcast(Result);
	OpenRootMenuForCurrentCombatant();
}

void UNUBattleCommandComponentBase::ResetSelection()
{
	// 예정 소모량은 표시용이라 게이지에서도 걷어내야 함
	// 아래에서 전투원 목록을 비우면 아바타를 못 찾으므로 먼저 처리함
	ClearPendingSPSpend();

	SelectionTurnNumber = INDEX_NONE;
	LastNaiaSwapTurn = INDEX_NONE;

	ControllableCombatantIds.Reset();
	CurrentCombatantIndex = INDEX_NONE;

	// 타겟
	SelectableTargetIds.Reset();
	CurrentTargetIndex = INDEX_NONE;
	bTargetsDowned = false;

	// 아이템
	SelectableItems.Reset();
	CurrentItemIndex = INDEX_NONE;
	PendingItemInstanceId.Invalidate();
	ClearItemReservations();
	
	// 스킬
	SelectableSkillIds.Reset();
	CurrentSkillIndex = INDEX_NONE;
	PendingSkillId = 0;
	
	// 예약
	PendingRootAction = ENUBattleRootAction::Attack;
	PendingSubmissionSourceId = INDEX_NONE;
	LastPresentedSourceCombatantId = INDEX_NONE;
	SelectedRootAction = ENUBattleRootAction::Attack;

	SetSelectionPhase(ENUCommandSelectionPhase::Inactive);
}

void UNUBattleCommandComponentBase::SetSelectionPhase(ENUCommandSelectionPhase NewPhase)
{
	SelectionPhase = NewPhase;
	BroadcastSelectionChanged();
}

void UNUBattleCommandComponentBase::BroadcastSelectionChanged()
{
	RefreshTargetIndicator();
	OnSelectionChanged.Broadcast();
}

ENUCommandSelectionPhase UNUBattleCommandComponentBase::GetSelectionPhase() const
{
	return SelectionPhase;
}

ENUBattleRootAction UNUBattleCommandComponentBase::GetSelectedRootAction() const
{
	return SelectedRootAction;
}

int32 UNUBattleCommandComponentBase::GetSelectionTurnNumber() const
{
	return SelectionTurnNumber;
}

int32 UNUBattleCommandComponentBase::GetCurrentSourceCombatantId() const
{
	if (!ControllableCombatantIds.IsValidIndex(CurrentCombatantIndex))
	{
		return INDEX_NONE;
	}
	
	return ControllableCombatantIds[CurrentCombatantIndex];
}

AActor* UNUBattleCommandComponentBase::GetCurrentSourceAvatar() const
{
	return GetCombatantAvatar(GetCurrentSourceCombatantId());
}

int32 UNUBattleCommandComponentBase::GetCurrentTargetCombatantId() const
{
	if (!SelectableTargetIds.IsValidIndex(CurrentTargetIndex))
	{
		return INDEX_NONE;
	}
	
	return SelectableTargetIds[CurrentTargetIndex];
}

AActor* UNUBattleCommandComponentBase::GetCurrentTargetAvatar() const
{
	return GetCombatantAvatar(GetCurrentTargetCombatantId());
}

FText UNUBattleCommandComponentBase::GetCombatantName(int32 CombatantId) const
{
	if (CombatantId == INDEX_NONE)
	{
		return FText::GetEmpty();
	}

	const FText DisplayName = GetCombatantDisplayName(CombatantId);

	// 이름을 못 찾았을 때 빈칸으로 두면 화면에서 원인이 안 보임, 번호라도 띄운다
	return DisplayName.IsEmpty() ? FText::AsNumber(CombatantId) : DisplayName;
}

FText UNUBattleCommandComponentBase::GetCurrentSourceName() const
{
	return GetCombatantName(GetCurrentSourceCombatantId());
}

FText UNUBattleCommandComponentBase::GetCurrentTargetName() const
{
	return GetCombatantName(GetCurrentTargetCombatantId());
}

TArray<int32> UNUBattleCommandComponentBase::GetSelectableTargetIds() const
{
	return SelectableTargetIds;
}

bool UNUBattleCommandComponentBase::IsRootActionAvailable(ENUBattleRootAction Action) const
{
	if (!IsBattleActive() || GetCurrentSourceCombatantId() == INDEX_NONE) return false;
		
	switch (Action)
	{
	case ENUBattleRootAction::Attack:
		{
			// 적이 살아있을때만 활성화
			// 나중엔 투명화나 타겟 제외가 있으면 걸러내기 가능
			TArray<int32> LivingEnemies;

			CollectLivingCombatantIds(ENUBattleTeam::Enemy, LivingEnemies);

			return !LivingEnemies.IsEmpty();
		}

	case ENUBattleRootAction::Skill:
		{
			if (HasSkillActionOverride()) { return IsSkillActionOverrideAvailable(); }
			TArray<int32> Usable;
			CollectUsableSkills(Usable);
			return !Usable.IsEmpty();
		}
		
	case ENUBattleRootAction::Item:
		{
			TArray<FNUItemInstance> Usable;
			CollectBattleUsableItems(Usable);
			return !Usable.IsEmpty();
		}

	case ENUBattleRootAction::NaiaSwap:
		{
			// 교체는 플레이어 본인 차례에만, 정령이 스스로 교체하지 못하게
			if (!IsPlayerCombatant(GetCurrentSourceCombatantId()))
			{
				return false;
			}

			// 교체가 SP를 채우므로 턴당 1회, 서버에서도 처리함
			if (LastNaiaSwapTurn == GetSelectionTurnNumber())
			{
				return false;
			}

			TArray<int32> Swappable;
			CollectSwappableNaiaSlots(Swappable);
			return !Swappable.IsEmpty();
		}

	case ENUBattleRootAction::Flee:
		return true;
		
	default:
		return false;
	}
}

void UNUBattleCommandComponentBase::SetTargetIndicatorClass(TSubclassOf<ANUBattleTargetIndicator> InTargetIndicatorClass)
{
	if (TargetIndicatorClass == InTargetIndicatorClass) return;
	
	DestroyTargetIndicator();
	TargetIndicatorClass = InTargetIndicatorClass;
}

APlayerState* UNUBattleCommandComponentBase::GetLocalPlayerState() const
{
	ANUPlayerController* PlayerController = Cast<ANUPlayerController>(GetOwner());
	
	return PlayerController ? PlayerController->PlayerState : nullptr;
}

UNUInventoryComponent* UNUBattleCommandComponentBase::GetInventoryComponent() const
{
	ANUPlayerController* PlayerController = Cast<ANUPlayerController>(GetOwner());
	if (!IsValid(PlayerController)) return nullptr;
	
	ANUPlayerState* PlayerState = Cast<ANUPlayerState>(PlayerController->PlayerState);
	if (!IsValid(PlayerState)) return nullptr;
	
	return PlayerState->GetInventoryComponent();
}

FText UNUBattleCommandComponentBase::GetCurrentItemDisplayText() const
{
	if (!SelectableItems.IsValidIndex(CurrentItemIndex))
	{
		return FText::GetEmpty();
	}

	const FNUItemInstance& Item = SelectableItems[CurrentItemIndex];

	const UNUInventoryComponent* Inventory = GetInventoryComponent();
	const FNUItemDefRow* Def = Inventory ? Inventory->FindItemDef(Item.ItemRowName) : nullptr;

	const FText Name = Def ? Def->DisplayName : FText::FromName(Item.ItemRowName);

	return FText::Format(NSLOCTEXT("NU", "ItemEntry", "{0} x{1}"), Name, Item.Count);
}

FText UNUBattleCommandComponentBase::GetItemPositionText() const
{
	if (SelectableItems.IsEmpty()) 
	{
		return FText::GetEmpty();
	}

	return FText::Format(NSLOCTEXT("NU", "ItemPos", "{0} / {1}"),
		CurrentItemIndex + 1, SelectableItems.Num());
}

void UNUBattleCommandComponentBase::CollectBattleUsableItems(TArray<struct FNUItemInstance>& OutItems) const
{
	OutItems.Reset();
	
	UNUInventoryComponent* Inventory = GetInventoryComponent();
	if (!IsValid(Inventory)) return;

	// 교감은 플레이어 본인만 하므로 정령 차례에는 교감 아이템을 목록에서 제외함
	const bool bCanCommune = IsPlayerCombatant(GetCurrentSourceCombatantId());

	for (const FNUItemInstance& Item : Inventory->GetItems())
	{
		const FNUItemDefRow* Def = Inventory->FindItemDef(Item.ItemRowName);
		if (!Def || !Def->bUsableInBattle || Item.Count <= 0)
		{
			// 장비·재료처럼 전투에 못 쓰는 아이템이 빠지는 것은 정상이라 남기지 않음
			// 인벤에 들고 있는데 표에 정의가 없거나 수량이 0인 것은 데이터가 어긋난 것이라 남김
			// 이 함수는 메뉴 활성 여부를 묻느라 매 프레임 불릴 수 있어 행마다 한 번만 찍음
			if (!Def || Item.Count <= 0)
			{
				static TSet<FName> LoggedSkips;
				if (!LoggedSkips.Contains(Item.ItemRowName))
				{
					LoggedSkips.Add(Item.ItemRowName);
					UE_LOG(LogTemp, Warning,
						TEXT("전투 아이템 제외 [%s]: 정의=%s 수량=%d"),
						*Item.ItemRowName.ToString(),
						Def ? TEXT("있음") : TEXT("없음"),
						Item.Count);
				}
			}
			continue;
		}

		if (Def->ItemType == ENUItemType::Commune && !bCanCommune)
		{
			continue;
		}

		OutItems.Add(Item);
	}

	ApplyItemReservations(OutItems);
}

void UNUBattleCommandComponentBase::ReserveSubmittedItem()
{
	if (!PendingItemInstanceId.IsValid())
	{
		return;
	}

	const UNUInventoryComponent* Inventory = GetInventoryComponent();
	const FNUItemInstance* Item = IsValid(Inventory)
		? Inventory->FindInstance(PendingItemInstanceId)
		: nullptr;
	if (!Item)
	{
		return;
	}

	// 턴이 넘어갔으면 지난 턴 예약은 버리고 현재 턴으로 다시 시작함
	if (ReservedItemTurn != SelectionTurnNumber)
	{
		ReservedItemCounts.Reset();
		ReservedItemTurn = SelectionTurnNumber;
	}

	// 칸이 아니라 아이템 종류로 세어 둠
	// 같은 아이템이 여러 칸에 나뉘어 있어도 총량이 맞아야 함
	int32& Reserved = ReservedItemCounts.FindOrAdd(Item->ItemRowName);
	++Reserved;
}

void UNUBattleCommandComponentBase::ClearItemReservations()
{
	ReservedItemCounts.Reset();
	ReservedItemTurn = INDEX_NONE;
}

void UNUBattleCommandComponentBase::ApplyItemReservations(TArray<FNUItemInstance>& OutItems) const
{
	// 지난 턴 예약은 반영하지 않음 
	// 다음 예약이나 초기화에서 실제로 비워짐
	if (ReservedItemCounts.IsEmpty() || ReservedItemTurn != SelectionTurnNumber)
	{
		return;
	}

	// 예약은 종류 단위라 앞 칸부터 차례로 깎음, 사본을 들고 깎아야 원본 예약이 안 지워짐
	TMap<FName, int32> Remaining = ReservedItemCounts;
	for (FNUItemInstance& Item : OutItems)
	{
		int32* Reserved = Remaining.Find(Item.ItemRowName);
		if (Reserved == nullptr || *Reserved <= 0)
		{
			continue;
		}

		const int32 Taken = FMath::Min(*Reserved, Item.Count);
		*Reserved -= Taken;
		Item.Count -= Taken;
	}

	// 남은 개수가 없는 칸은 고를 수 없어야 함 
	// 메뉴에 안뜨거나 제출이 서버에서 거절됨
	OutItems.RemoveAll([](const FNUItemInstance& Item)
	{
		return Item.Count <= 0;
	});
}

bool UNUBattleCommandComponentBase::RequiresTargetSelection(ENUTargetPolicy Policy)
{
	// 단일 대상 정책만 플레이어가 타겟 선택을 필요로 함
	// Self나 AllEnemies, AllAllies 는 서버가 모든 적을 대상으로 판정함
	return Policy == ENUTargetPolicy::SingleAlly
		|| Policy == ENUTargetPolicy::SingleEnemy;
}

ENUBattleTeam UNUBattleCommandComponentBase::GetPolicyTeam(ENUTargetPolicy Policy)
{
	// 타겟팅 정책이 적을 대상으로하면 타겟팅 진형은 Enemy, 반대면 Ally
	return Policy == ENUTargetPolicy::SingleEnemy || Policy == ENUTargetPolicy::AllEnemies 
		? ENUBattleTeam::Enemy
		: ENUBattleTeam::Ally;
}

void UNUBattleCommandComponentBase::RefreshTargetIndicator()
{
	ANUPlayerController* PlayerController = Cast<ANUPlayerController>(GetOwner());
	if (!IsValid(PlayerController) || !PlayerController->IsLocalController()) return;
	
	if (SelectionPhase != ENUCommandSelectionPhase::TargetSelection)
	{
		HideTargetIndicator();
		return;
	}

	// 쓰러진 아군은 아바타가 남아 있어서 보통은 이 검사에 걸리지 않음
	// 포획 흔들림이 끝나기 전처럼 잠깐 숨은 경우를 위한 예비 분기라, 부활 대상일 때만 통과시킴
	ANUBattleAvatar* TargetAvatar = Cast<ANUBattleAvatar>(GetCurrentTargetAvatar());
	if (!IsValid(TargetAvatar) || (TargetAvatar->IsHidden() && !bTargetsDowned))
	{
		HideTargetIndicator();
		return;
	}
	
	// 인디케이터가 없다면 생성
	if (!IsValid(TargetIndicator))
	{
		if (!TargetIndicatorClass) return;
		
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = PlayerController;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		TargetIndicator = GetWorld()->SpawnActor<ANUBattleTargetIndicator>(
			TargetIndicatorClass,
			FTransform::Identity,
			SpawnParameters
			);
		
		// 생성이 안됐다면 리턴
		if (!IsValid(TargetIndicator)) return; 
	}
	
	
	
	TargetIndicator->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	TargetIndicator->AttachToComponent(TargetAvatar->GetTargetIndicatorAnchor(), 
		FAttachmentTransformRules::SnapToTargetIncludingScale);
	TargetIndicator->SetActorHiddenInGame(false);
}

void UNUBattleCommandComponentBase::HideTargetIndicator()
{ 
	if (!IsValid(TargetIndicator)) return;
	
	TargetIndicator->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	TargetIndicator->SetActorHiddenInGame(true);
}

void UNUBattleCommandComponentBase::DestroyTargetIndicator()
{
	if (!IsValid(TargetIndicator)) return;
	
	TargetIndicator->Destroy();
	TargetIndicator = nullptr;
}

FText UNUBattleCommandComponentBase::GetSkillDisplayName() const
{
	return NSLOCTEXT("NUBattle", "SkillCommand", "스킬");
}

FText UNUBattleCommandComponentBase::GetAttackDisplayName() const
{
	return NSLOCTEXT("NUBattle", "AttackCommand", "공격");
}
