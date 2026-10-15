#include "Quest/Encounter/NUQuestBattleSession.h"

#include "Battle/Avatar/NUBattleAvatar.h"
#include "Battle/Encounter/NUBattleEncounter.h"
#include "Battle/NUBattleActionContext.h"
#include "CharacterProgression/Component/NUProgressionComponent.h"
#include "CharacterState/NUPlayerState.h"
#include "NUGAS/NUAbilitySystemComponent.h"
#include "NUGAS/NUGameplayTags.h"
#include "NUGAS/Attribute/NUVitalAttributeSet.h"
#include "Quest/Ability/NUQuestBurrowAbility.h"
#include "Quest/Ability/NUQuestCalmAbility.h"
#include "Quest/Ability/NUQuestMoveStoneAbility.h"
#include "Quest/Ability/NUQuestWaitAbility.h"
#include "Quest/Component/NUQuestComponent.h"
#include "Quest/Data/NUQuestBattleDefinition.h"
#include "Quest/Data/NUQuestDataSubsystem.h"
#include "Quest/Data/NUQuestDefinition.h"
#include "Quest/Data/NUQuestTypes.h"
#include "Quest/Encounter/NUQuestEncounterSubsystem.h"
#include "Quest/Data/NUQuestTutorialTags.h"
#include "Quest/UI/NUQuestAgitationWidget.h"
#include "Quest/World/NUQuestActionActor.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemGlobals.h"
#include "Blueprint/UserWidget.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameplayCueManager.h"
#include "PaperFlipbookComponent.h"

namespace NUQuestBattleSession
{
	const UNUQuestDefinition* FindQuestDefinition(const UWorld* World, FName QuestId)
	{
		const UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
		const UNUQuestDataSubsystem* Data = GameInstance != nullptr ? GameInstance->GetSubsystem<UNUQuestDataSubsystem>() : nullptr;
		return Data != nullptr ? Data->FindDefinition(QuestId) : nullptr;
	}
}

// -------------------------------------------------------------------------
// Lifecycle
// -------------------------------------------------------------------------

bool UNUQuestBattleSession::Initialize(ANUBattleEncounter* InEncounter, UNUQuestBattleDefinition* InDefinition, UNUQuestComponent* InQuestComponent)
{
	if (!IsValid(InEncounter) || !IsValid(InDefinition) || !IsValid(InQuestComponent) || InQuestComponent->GetOwner() == nullptr)
	{
		return false;
	}

	AActor* Player = InQuestComponent->GetOwner();
	const UNUProgressionComponent* Progression = Player->FindComponentByClass<UNUProgressionComponent>();
	const UGameInstance* GameInstance = InEncounter->GetGameInstance();
	const UNUQuestEncounterSubsystem* Encounters = GameInstance != nullptr ? GameInstance->GetSubsystem<UNUQuestEncounterSubsystem>() : nullptr;
	// 서버가 승인한 진정 도전(105)은 온라인에서도 로컬 판정으로 돌린다. 그 밖의 로컬 전투는 로컬 백엔드에서만 연다.
	const bool bLocalBattleAllowed = (Progression != nullptr && Progression->IsUsingLocalProgressionBackend()) || (Encounters != nullptr && Encounters->IsCalmAttemptApproved(InDefinition));
	if (InEncounter->GetNetMode() != NM_Standalone || !bLocalBattleAllowed || InDefinition->Kind == ENUQuestBattleKind::None || (InDefinition->Kind == ENUQuestBattleKind::Calming && InDefinition->InitialAgitation <= 0.f))
	{
		return false;
	}

	const UNUQuestDefinition* QuestDefinition = NUQuestBattleSession::FindQuestDefinition(InEncounter->GetWorld(), InDefinition->QuestId);
	const FNUQuestProgressState* Progress = InQuestComponent->FindQuestProgress(InDefinition->QuestId);
	const FNUQuestObjectiveSpec* Objective = QuestDefinition != nullptr ? QuestDefinition->Objectives.FindByPredicate([InDefinition](const FNUQuestObjectiveSpec& Item) { return Item.ObjectiveId == InDefinition->ObjectiveId; }) : nullptr;
	const FNUQuestObjectiveProgress* ObjectiveProgress = Progress != nullptr ? Progress->FindObjectiveProgress(InDefinition->ObjectiveId) : nullptr;
	if (Progress == nullptr || Progress->QuestState != ENUQuestState::Active || Objective == nullptr || ObjectiveProgress == nullptr || ObjectiveProgress->IsComplete(Objective->RequiredCount) || !Progress->IsObjectivePrerequisiteMet(*QuestDefinition, *Objective))
	{
		return false;
	}

	FNUQuestTutorialBattleRules NewRules;
	const int32 Recovered = InDefinition->GetCompletedGrassRecoveries(*QuestDefinition, *Progress);
	if (InDefinition->Rule == ENUQuestCalmingRule::Grass && Recovered != InDefinition->GrassRecoveryObjectiveIds.Num()) { return false; }
	if (InDefinition->Kind != ENUQuestBattleKind::Sparring && !NewRules.Initialize(InDefinition, Recovered))
	{
		return false;
	}

	Encounter = InEncounter;
	Definition = InDefinition;
	QuestComponent = InQuestComponent;
	Rules = NewRules;

	InEncounter->OnBattlePhaseChanged.AddUObject(this, &ThisClass::HandlePhaseChanged);
	InEncounter->OnEncounterCompleted.AddUObject(this, &ThisClass::HandleCompleted);
	InEncounter->OnEndPlay.AddUniqueDynamic(this, &ThisClass::HandleEncounterEndPlay);

	GrantAbilities();
	RefreshPresentation();
	PublishPracticeState(false);
	return true;
}

void UNUQuestBattleSession::Shutdown()
{
	if (ANUBattleEncounter* Current = GetEncounter())
	{
		Current->OnBattlePhaseChanged.RemoveAll(this);
		Current->OnEncounterCompleted.RemoveAll(this);
		Current->OnEndPlay.RemoveDynamic(this, &ThisClass::HandleEncounterEndPlay);
	}

	ReleaseAbilities();
	ClearPresentation();
	PublishPracticeState(true);
	Encounter.Reset();
	Definition = nullptr;
	Rules.Reset();
}

bool UNUQuestBattleSession::IsRunning() const
{
	return Encounter.IsValid() && Definition != nullptr && Definition->Kind != ENUQuestBattleKind::None;
}

// 인카운터 콜백은 완료와 비정상 종료 모두 같은 정리 경로로 보낸다.
void UNUQuestBattleSession::HandlePhaseChanged(ENUBattlePhase Phase, int32 TurnNumber) { RefreshPresentation(); }

void UNUQuestBattleSession::HandleCompleted(ANUBattleEncounter* InEncounter, ENUBattleResult Result)
{
	if (GetEncounter() != InEncounter)
	{
		return;
	}

	PublishResult();
	Shutdown();
}

// 전투 액터 파괴와 맵 이동에도 바인딩과 임시 어빌리티를 회수한다.
void UNUQuestBattleSession::HandleEncounterEndPlay(AActor* Actor, EEndPlayReason::Type EndPlayReason)
{
	if (!Encounter.IsValid() || Encounter.Get() == Actor)
	{
		Shutdown();
	}
}

// -------------------------------------------------------------------------
// Abilities
// -------------------------------------------------------------------------

UNUAbilitySystemComponent* UNUQuestBattleSession::GetPlayerASC() const
{
	const UNUQuestComponent* Quest = GetQuestComponent();
	return Quest != nullptr ? Cast<UNUAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Quest->GetOwner())) : nullptr;
}

void UNUQuestBattleSession::GrantAbilities()
{
	UNUAbilitySystemComponent* ASC = GetPlayerASC();
	if (ASC != nullptr && IsCalming())
	{
		GrantedASC = ASC;
		if (ASC->FindAbilitySpecFromClass(UNUQuestCalmAbility::StaticClass()) == nullptr)
		{
			GrantedHandles.Add(ASC->GiveAbility(FGameplayAbilitySpec(UNUQuestCalmAbility::StaticClass(), 1)));
		}

		// 땅은 돌 옮기기, 나머지 진정 전투는 기다리기를 보조 행동으로 제공한다.
		if (Definition->Rule != ENUQuestCalmingRule::Earth && ASC->FindAbilitySpecFromClass(UNUQuestWaitAbility::StaticClass()) == nullptr)
		{
			GrantedHandles.Add(ASC->GiveAbility(FGameplayAbilitySpec(UNUQuestWaitAbility::StaticClass(), 1)));
		}

		if (Definition->Rule == ENUQuestCalmingRule::Earth && ASC->FindAbilitySpecFromClass(UNUQuestMoveStoneAbility::StaticClass()) == nullptr)
		{
			GrantedHandles.Add(ASC->GiveAbility(FGameplayAbilitySpec(UNUQuestMoveStoneAbility::StaticClass(), 1)));
		}
	}

	if (Definition->Kind == ENUQuestBattleKind::Calming && Definition->Rule == ENUQuestCalmingRule::Earth)
	{
		const FNUBattleParticipant* Enemy = GetEncounter()->FindFirstParticipant(ENUBattleTeam::Enemy);
		UNUAbilitySystemComponent* EnemyASC = Enemy != nullptr ? Cast<UNUAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Enemy->ASCOwner)) : nullptr;
		// 적 ASC는 이 전투의 임시 아바타와 함께 제거된다.
		if (EnemyASC != nullptr && EnemyASC->FindAbilitySpecFromClass(UNUQuestBurrowAbility::StaticClass()) == nullptr)
		{
			EnemyASC->GiveAbility(FGameplayAbilitySpec(UNUQuestBurrowAbility::StaticClass(), 1));
		}
	}
}

// 원래 보유하던 어빌리티는 유지하고 이 전투에서 부여한 핸들만 회수한다.
void UNUQuestBattleSession::ReleaseAbilities()
{
	if (GrantedASC.IsValid())
	{
		for (FGameplayAbilitySpecHandle Handle : GrantedHandles)
		{
			GrantedASC->SetRemoveAbilityOnEnd(Handle);
		}
	}

	GrantedHandles.Reset();
	GrantedASC.Reset();
}

// -------------------------------------------------------------------------
// State
// -------------------------------------------------------------------------

bool UNUQuestBattleSession::IsCalming() const { return IsRunning() && Rules.IsCalming(); }

bool UNUQuestBattleSession::IsEnemyBurrowed() const { return IsRunning() && Rules.IsEnemyBurrowed(); }

bool UNUQuestBattleSession::ShouldEnemyBurrow() const { return IsRunning() && Rules.ShouldEnemyBurrow(); }

bool UNUQuestBattleSession::HasSucceeded() const { return IsRunning() && Rules.HasSucceeded(); }

// -------------------------------------------------------------------------
// Turn / Action
// -------------------------------------------------------------------------

// 턴 번호는 규칙 상태에 기록하고 UI를 갱신한다.
void UNUQuestBattleSession::BeginTurn(int32 TurnNumber)
{
	// 지난 턴에 옆으로 밀어 둔 돌은 이번 턴이 시작될 때 치운다.
	if (WorldObject.IsValid()) { WorldObject->ClearMovedBattleStone(); }

	if (Rules.IsActive())
	{
		UE_LOG(LogNUQuest, Log, TEXT("QuestBattle BeginTurn Quest=%s Turn=%d PreviousTurn=%d Agitation=%.1f"), *Definition->QuestId.ToString(), TurnNumber, Rules.GetTurnNumber(), Rules.GetAgitation());
		Rules.BeginTurn(TurnNumber);
	}
	if (Definition != nullptr && (Definition->Kind == ENUQuestBattleKind::Parry || Definition->Kind == ENUQuestBattleKind::Dodge))
	{
		// Q5는 플레이어 행동이 없으므로 연습 공격마다 반응 비용을 다시 사용할 수 있게 한다.
		if (UNUAbilitySystemComponent* PlayerASC = GetPlayerASC())
		{
			PlayerASC->SetNumericAttributeBase(UNUVitalAttributeSet::GetSPAttribute(), PlayerASC->GetNumericAttribute(UNUVitalAttributeSet::GetMaxSPAttribute()));
		}
	}
	RefreshPresentation();
}

// 진정 전투는 기본 공격을 제공하지 않고 달래기를 사용한다.
bool UNUQuestBattleSession::IsAbilityAllowed(FGameplayTag AbilityId) const
{
	if (AbilityId == NU::Quest::Ability::Wait)
	{
		return IsCalming() && Definition->Rule != ENUQuestCalmingRule::Earth;
	}

	if (AbilityId == NU::Quest::Ability::MoveStone) { return CanMoveStone(); }
	if (AbilityId == NU::Quest::Ability::Calm)
	{
		return IsCalming();
	}
	if (AbilityId == NU::Ability::Battle::BasicAttack && IsCalming()) { return false; }

	return true;
}

void UNUQuestBattleSession::AdjustAction(const FNUBattleCommandRequest& Command, FNUBattleActionContext& Context) const
{
	if (!IsRunning())
	{
		return;
	}

	FNUBattleParticipant Source;
	if (!GetEncounter()->GetParticipant(Command.SourceCombatantId, Source) || Source.Team != ENUBattleTeam::Enemy)
	{
		return;
	}

	if (Rules.IsActive())
	{
		if (Command.AbilityId == NU::Quest::Ability::Burrow)
		{
			Context.bReactable = false;
			Context.bParryable = false;
			return;
		}
		Rules.AdjustEnemyAction(Context);
	}
	else
	{
		Context.bReactable = true;
		Context.bParryable = true;
	}
}

// 바람·전기 조우의 행동 순서 기믹을 공용 큐의 속도 계산에 반영한다.
float UNUQuestBattleSession::AdjustSpeed(bool bEnemy, float Speed) const
{
	return Rules.AdjustSpeed(bEnemy, Speed);
}

void UNUQuestBattleSession::ApplyBurrow()
{
	if (!IsRunning() || GetEncounter()->GetPhase() != ENUBattlePhase::ActionExecution || !Rules.EnterBurrow()) { return; }
	RefreshPresentation();
}

bool UNUQuestBattleSession::CanMoveStone() const
{
	if (!IsCalming()) { return false; }
	const ENUBattlePhase Phase = GetEncounter()->GetPhase();
	// 매몰 예정은 허용하지 않는다. 실제로 숨은 상태에서만 선택하고 실행할 수 있다.
	return Rules.CanMoveStone()
		&& (Phase == ENUBattlePhase::CommandSelection || Phase == ENUBattlePhase::ActionExecution);
}

bool UNUQuestBattleSession::MoveStone()
{
	if (!CanMoveStone() || GetEncounter()->GetPhase() != ENUBattlePhase::ActionExecution || !Rules.MoveStone()) { return false; }
	if (WorldObject.IsValid())
	{
		WorldObject->MoveBattleStone();
		PlayResultCue(WorldObject.Get(), NU::Quest::Cue::MoveStone, WorldObject->Mesh->GetComponentLocation());
	}
	UNUQuestComponent* Quest = GetQuestComponent();
	if (!Definition->EarthStoneObjectiveId.IsNone() && Quest != nullptr)
	{
		const UNUQuestDefinition* QuestDefinition = NUQuestBattleSession::FindQuestDefinition(GetEncounter()->GetWorld(), Definition->QuestId);
		Quest->AddObjectiveProgress(QuestDefinition, Definition->EarthStoneObjectiveId, 1);
	}
	RefreshPresentation();
	return true;
}

// -------------------------------------------------------------------------
// Damage / Reaction
// -------------------------------------------------------------------------

// 연습 상대는 HP 대신 목표 조건으로 종료한다.
bool UNUQuestBattleSession::InterceptDamage(UNUAbilitySystemComponent* Target, float& Damage)
{
	if (!IsRunning() || Target == nullptr)
	{
		return false;
	}

	const FNUBattleParticipant* Enemy = GetEncounter()->FindFirstParticipant(ENUBattleTeam::Enemy);
	const bool bEnemy = Enemy != nullptr && Enemy->ASCOwner == Target->GetOwnerActor();
	const float TargetHealth = Target->GetNumericAttribute(UNUVitalAttributeSet::GetHealthAttribute());
	const bool bIntercepted = Rules.InterceptDamage(bEnemy, TargetHealth, Damage);
	if (IsCalming())
	{
		RefreshPresentation();
	}

	return bIntercepted;
}

// 실제 명중·패링·회피가 성립했을 때만 연습 결과를 갱신한다.
void UNUQuestBattleSession::NotifyHit(UNUAbilitySystemComponent* Actor, FGameplayTag AbilityId)
{
	const FNUBattleParticipant* Enemy = IsRunning() ? GetEncounter()->FindFirstParticipant(ENUBattleTeam::Enemy) : nullptr;
	const bool bEnemyActor = Actor != nullptr && Enemy != nullptr && Enemy->ASCOwner == Actor->GetOwnerActor();
	if (Rules.ShouldRestorePlayerHealthOnHit(bEnemyActor))
	{
		// Q5는 매 공격 전 체력을 회복해 패링·회피를 반복 연습할 수 있게 한다.
		if (UNUAbilitySystemComponent* PlayerASC = GetPlayerASC())
		{
			PlayerASC->SetNumericAttributeBase(UNUVitalAttributeSet::GetHealthAttribute(), PlayerASC->GetNumericAttribute(UNUVitalAttributeSet::GetMaxHealthAttribute()));
		}
	}

	if (Cast<ANUPlayerState>(Actor != nullptr ? Actor->GetOwnerActor() : nullptr) != nullptr)
	{
		Rules.NotifyPlayerAbilityHit(AbilityId);
		PublishPracticeState(false);
	}
}

void UNUQuestBattleSession::NotifyReaction(UNUAbilitySystemComponent* Actor, bool bParry)
{
	const UNUQuestComponent* Quest = GetQuestComponent();
	if (!IsRunning() || Actor == nullptr || Quest == nullptr || Actor->GetOwnerActor() != Quest->GetOwner())
	{
		return;
	}

	Rules.NotifyPlayerReaction(bParry);
	RefreshPresentation();
	PublishPracticeState(false);
}

void UNUQuestBattleSession::PublishPracticeState(bool bClear) const
{
	// 연습 전투(기본 공격·패링·회피)만 전투 튜토리얼 안내에 횟수를 넘긴다. 진정 전투는 동요 표시가 따로 있다.
	const UNUQuestComponent* Quest = GetQuestComponent();
	const UWorld* World = Quest != nullptr ? Quest->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
	UNUQuestEncounterSubsystem* Encounters = GameInstance != nullptr ? GameInstance->GetSubsystem<UNUQuestEncounterSubsystem>() : nullptr;
	if (Encounters == nullptr || !Rules.IsTraining())
	{
		return;
	}
	if (bClear)
	{
		Encounters->ClearLocalPracticeState();
		return;
	}
	Encounters->SetLocalPracticeState(Rules.GetKind(), Rules.GetPracticeCount(), Rules.GetPracticeRequired());
}

// GAS 행동 실행 중에만 동요를 변경한다. 버튼 입력만으로는 목표가 진행되지 않는다.
void UNUQuestBattleSession::ApplyCalm()
{
	if (!IsCalming() || GetEncounter()->GetPhase() != ENUBattlePhase::ActionExecution)
	{
		return;
	}

	const ENUQuestCalmResult Result = Rules.ApplyCalm();
	const FNUBattleParticipant* Enemy = GetEncounter()->FindFirstParticipant(ENUBattleTeam::Enemy);
	if (Enemy != nullptr && IsValid(Enemy->AvatarActor) && Result != ENUQuestCalmResult::None)
	{
		const FGameplayTag CueTag = Result == ENUQuestCalmResult::Calmed ? NU::Quest::Cue::Calm : NU::Quest::Cue::CalmFail;
		PlayResultCue(Enemy->AvatarActor, CueTag, Enemy->AvatarActor->GetActorLocation());
	}
	RefreshPresentation();
}

// 진정 판정은 로컬에서만 돌고 GE가 없으므로, 사망 큐처럼 대상 액터에 직접 띄운다.
void UNUQuestBattleSession::PlayResultCue(AActor* Target, FGameplayTag CueTag, const FVector& Location) const
{
	UGameplayCueManager* CueManager = UAbilitySystemGlobals::Get().GetGameplayCueManager();
	if (!IsValid(Target) || CueManager == nullptr)
	{
		return;
	}

	FGameplayCueParameters Parameters;
	Parameters.Location = Location;
	CueManager->HandleGameplayCue(Target, CueTag, EGameplayCueEvent::Executed, Parameters);
}

// -------------------------------------------------------------------------
// Result
// -------------------------------------------------------------------------

// 공용 인카운터의 승리를 퀘스트 목표에 한 번만 전달한다.
void UNUQuestBattleSession::PublishResult()
{
	UNUQuestComponent* Quest = GetQuestComponent();
	if (!IsRunning() || Quest == nullptr || bPublished || GetEncounter()->GetBattleResult() != ENUBattleResult::Victory) { return; }

	bPublished = true;
	if (Definition->Kind != ENUQuestBattleKind::Sparring && !Rules.HasSucceeded()) { return; }

	// 온라인 진정은 진정 도전 결과(106)로 서버가 목표를 올린다. 로컬에서 따로 반영하지 않는다.
	if (Quest->IsServerBackedSession()) { return; }

	// 서버 규칙과 같이 이 전투를 시작한 목표에만 반영한다. 대상 ID로 다른 목표를 찾지 않는다.
	// 전투 ID를 출처로 넘겨 같은 전투가 두 번 반영되지 않게 한다.
	const FGuid EncounterId = GetEncounter()->GetEncounterId();
	if (!EncounterId.IsValid()) { return; }

	const UNUQuestDefinition* QuestDefinition = NUQuestBattleSession::FindQuestDefinition(GetEncounter()->GetWorld(), Definition->QuestId);
	const FName SourceInstanceId(*EncounterId.ToString(EGuidFormats::DigitsWithHyphensLower));
	if (!Quest->AddObjectiveProgress(QuestDefinition, Definition->ObjectiveId, 1, SourceInstanceId))
	{
		UE_LOG(LogNUQuest, Warning, TEXT("QuestBattle 결과 반영 실패 Quest=%s Objective=%s"), *Definition->QuestId.ToString(), *Definition->ObjectiveId.ToString());
	}
}

// -------------------------------------------------------------------------
// Presentation
// -------------------------------------------------------------------------

// 동요는 HP 어트리뷰트나 HP 위젯을 사용하지 않는다. 판정 상태는 전용 규칙이 소유한다.
void UNUQuestBattleSession::RefreshPresentation()
{
	ANUBattleEncounter* Current = GetEncounter();
	if (Current == nullptr) { return; }

	if (Current->GetPhase() == ENUBattlePhase::Ended) { ReleaseAbilities(); }
	if (Current->GetPhase() == ENUBattlePhase::Ending || Current->GetPhase() == ENUBattlePhase::Ended) { ClearPresentation(); return; }
	const FNUBattleParticipant* Enemy = Current->FindFirstParticipant(ENUBattleTeam::Enemy);
	ANUBattleAvatar* Avatar = Enemy != nullptr ? Cast<ANUBattleAvatar>(Enemy->AvatarActor) : nullptr;
	if (Avatar != nullptr && IsCalming())
	{
		Avatar->SetVitalBarHidden(true);
		if (AgitationWidget == nullptr && Definition->AgitationWidgetClass != nullptr)
		{
			AgitationWidget = NewObject<UWidgetComponent>(Avatar);
			AgitationWidget->SetupAttachment(Avatar->GetRootComponent());
			AgitationWidget->SetWidgetSpace(EWidgetSpace::Screen);
			AgitationWidget->SetDrawSize(FVector2D(220.f, 48.f));
			AgitationWidget->SetRelativeLocation(Definition->AgitationWidgetOffset);
			AgitationWidget->SetWidgetClass(Definition->AgitationWidgetClass);
			AgitationWidget->RegisterComponent();
			AgitationWidget->InitWidget();
		}
		if (AgitationWidget != nullptr)
		{
			if (UNUQuestAgitationWidget* Widget = Cast<UNUQuestAgitationWidget>(AgitationWidget->GetUserWidgetObject()))
			{
				Widget->SetAgitation(Rules.GetAgitation(), Rules.GetInitialAgitation());
			}
		}
	}

	// 땅 조우의 돌은 해당 전투 동안만 적 진형을 기준으로 생성한다.
	if (Avatar != nullptr && IsCalming() && Definition->WorldActionClass != nullptr && !WorldObject.IsValid() && Current->GetPhase() == ENUBattlePhase::CommandSelection)
	{
		FActorSpawnParameters Params;
		Params.Owner = GetQuestComponent() != nullptr ? GetQuestComponent()->GetOwner() : nullptr;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		WorldObject = Current->GetWorld()->SpawnActor<ANUQuestActionActor>(Definition->WorldActionClass, Avatar->GetActorLocation() + Definition->WorldActionOffset, FRotator::ZeroRotator, Params);
	}

	// 돌과 몸의 전환은 전투용 돌 액터가 재생한다. 상태 갱신은 진행 중인 연출을 재시작하지 않는다.
	if (WorldObject.IsValid())
	{
		WorldObject->SetBattleBurrowed(IsEnemyBurrowed(), Avatar != nullptr ? Avatar->FindComponentByClass<UPaperFlipbookComponent>() : nullptr);
	}

	// 물 안개는 달래기 성공으로 걷히고 진정 완료 시 완전히 사라진다.
	if (IsCalming() && Definition->Rule == ENUQuestCalmingRule::Water && Definition->MistWidgetClass != nullptr)
	{
		if (Mist == nullptr)
		{
			ANUPlayerState* State = GetQuestComponent() != nullptr ? Cast<ANUPlayerState>(GetQuestComponent()->GetOwner()) : nullptr;
			APlayerController* PC = State != nullptr ? State->GetPlayerController() : nullptr;
			if (PC != nullptr && PC->IsLocalController())
			{
				Mist = CreateWidget<UUserWidget>(PC, Definition->MistWidgetClass);
				if (Mist != nullptr)
				{
					Mist->SetVisibility(ESlateVisibility::HitTestInvisible);
					Mist->AddToViewport(-1);
				}
			}
		}

		if (Mist != nullptr)
		{
			Mist->SetRenderOpacity(Rules.GetMistRatio() * Definition->MaximumMistOpacity);
		}
	}
}

void UNUQuestBattleSession::ClearPresentation()
{
	if (AgitationWidget != nullptr) { AgitationWidget->DestroyComponent(); AgitationWidget = nullptr; }
	if (WorldObject.IsValid()) { WorldObject->Destroy(); }
	WorldObject.Reset();
	if (Mist != nullptr) { Mist->RemoveFromParent(); Mist = nullptr; }
}
