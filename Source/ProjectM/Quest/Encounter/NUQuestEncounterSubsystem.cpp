#include "Quest/Encounter/NUQuestEncounterSubsystem.h"

#include "Battle/Encounter/NUBattleEncounter.h"
#include "Battle/Encounter/NUBattleEncounterSubsystem.h"
#include "Battle/Net/NUBattleNetStageSubsystem.h"
#include "Battle/Net/NUBattleNetSubsystem.h"
#include "CharacterProgression/Component/NUProgressionComponent.h"
#include "CharacterState/NUPlayerState.h"
#include "Naia/Data/NUNaiaDataSubsystem.h"
#include "Naia/Data/NUNaiaDefinition.h"
#include "Net/EmberServerManager.h"
#include "NUGAS/NUAbilitySystemComponent.h"
#include "Quest/Component/NUQuestComponent.h"
#include "Quest/Data/NUQuestBattleDefinition.h"
#include "Quest/Data/NUQuestDataSettings.h"
#include "Quest/Data/NUQuestDataSubsystem.h"
#include "Quest/Data/NUQuestDefinition.h"
#include "Quest/Encounter/NUQuestTutorialEncounter.h"
#include "Quest/UI/NUQuestPracticeGuideWidget.h"
#include "UI/NUUIManager.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Subsystems/SubsystemCollection.h"

void UNUQuestEncounterSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UNUBattleNetSubsystem>();
	GetGameInstance()->GetSubsystem<UNUBattleNetSubsystem>()->OnBattleStateChanged.AddUObject(this, &ThisClass::HandleBattleStateChanged);
	OnPracticeStateChanged.AddUObject(this, &ThisClass::RefreshPracticeGuide);
}

void UNUQuestEncounterSubsystem::Deinitialize()
{
	if (UNUBattleNetSubsystem* Net = GetGameInstance()->GetSubsystem<UNUBattleNetSubsystem>())
	{
		Net->OnBattleStateChanged.RemoveAll(this);
	}
	OnPracticeStateChanged.RemoveAll(this);
	if (UNUQuestPracticeGuideWidget* Guide = PracticeGuide.Get())
	{
		Guide->RemoveFromParent();
	}
	PracticeGuide.Reset();

	Super::Deinitialize();
}

bool UNUQuestEncounterSubsystem::TryStartQuestEncounter(ANUPlayerState* Player, const FNUBattleEncounterRequest& Placement, bool bServerSession)
{
	if (!IsValid(Player) || !Player->HasAuthority() || !IsValid(Player->GetWorld()))
	{
		return false;
	}

	// 설치 액터 외의 호출도 완료된 목표나 선행 조건을 건너뛸 수 없다.
	const UNUQuestBattleDefinition* Battle = Cast<UNUQuestBattleDefinition>(Placement.BattleDefinition);
	const UNUQuestComponent* Quest = Player->GetQuestComponent();
	const UNUQuestDataSubsystem* Data = GetGameInstance()->GetSubsystem<UNUQuestDataSubsystem>();
	const UNUQuestDefinition* Definition = Battle != nullptr && Data != nullptr ? Data->FindDefinition(Battle->QuestId) : nullptr;
	const FNUQuestProgressState* Progress = Quest != nullptr && Battle != nullptr ? Quest->FindQuestProgress(Battle->QuestId) : nullptr;

	if (Definition == nullptr || Progress == nullptr || Progress->QuestState != ENUQuestState::Active || Quest->IsServerBackedSession() != bServerSession)
	{
		return false;
	}

	const FNUQuestObjectiveSpec* Objective = Definition->Objectives.FindByPredicate([Battle](const FNUQuestObjectiveSpec& Item)
	{
		return Item.ObjectiveId == Battle->ObjectiveId;
	});
	const FNUQuestObjectiveProgress* ObjectiveProgress = Progress->FindObjectiveProgress(Battle->ObjectiveId);

	if (Objective == nullptr || ObjectiveProgress == nullptr || ObjectiveProgress->IsComplete(Objective->RequiredCount) || !Progress->IsObjectivePrerequisiteMet(*Definition, *Objective))
	{
		return false;
	}

	// 서버도 목표 타입과 battle_mode가 다르면 거절(1715)하므로 로컬에서도 같은 조건으로 막는다.
	if (Objective->ObjectiveType != Battle->GetObjectiveType())
	{
		UE_LOG(LogNUQuest, Error, TEXT("QuestBattle 목표 타입 불일치 Objective=%s Type=%s Kind=%d"), *Battle->ObjectiveId.ToString(), *Objective->ObjectiveType.ToString(), static_cast<int32>(Battle->Kind));
		return false;
	}

	UWorld* World = Player->GetWorld();
	if (Battle->Rule == ENUQuestCalmingRule::Grass && Battle->GetCompletedGrassRecoveries(*Definition, *Progress) != Battle->GrassRecoveryObjectiveIds.Num()) { return false; }
	UNUBattleEncounterSubsystem* Execution = World->GetSubsystem<UNUBattleEncounterSubsystem>();
	UNUBattleNetSubsystem* Net = GetGameInstance()->GetSubsystem<UNUBattleNetSubsystem>();
	UNUBattleNetStageSubsystem* Stage = World->GetSubsystem<UNUBattleNetStageSubsystem>();
	const UNUAbilitySystemComponent* ASC = Player->GetNUASC();
	const int32 Mode = NUQuestBattle::ToServerBattleMode(Battle->Kind);

	if (Mode <= 0 || ASC == nullptr || !ASC->IsAlive() || Net == nullptr || Execution == nullptr || Player->GetBattleEncounter() != nullptr || Net->IsInBattle() || Net->IsBattleStartPending() || (Stage != nullptr && Stage->IsTransitionActive()))
	{
		return false;
	}

	// 온라인은 서버 판정만 사용하고 요청 실패를 로컬 전투로 대체하지 않는다.
	if (bServerSession)
	{
		const UNUNaiaDataSubsystem* NaiaData = GetGameInstance()->GetSubsystem<UNUNaiaDataSubsystem>();
		const FNUNaiaDefinition* Naia = NaiaData != nullptr && Placement.Enemies.Num() == 1 ? NaiaData->FindDefinition(Placement.Enemies[0].EnemyId) : nullptr;
		int32 QuestId = 0;
		int32 ObjectiveId = 0;

		if (Naia == nullptr || Naia->DataIndex <= 0 || !LexTryParseString(QuestId, *Battle->QuestId.ToString()) || !LexTryParseString(ObjectiveId, *Battle->ObjectiveId.ToString()) || QuestId <= 0 || ObjectiveId <= 0)
		{
			return false;
		}

		// 로그인된 서버에 퀘스트 전용 시작 요청을 보낸다.
		UEmberServerManager* Server = GetGameInstance()->GetSubsystem<UEmberServerManager>();
		if (Server == nullptr || !Server->IsLoggedIn())
		{
			return false;
		}

		// 진정을 클라이언트 판정으로 돌리면 서버 전투를 열지 않고 도전 승인(105)만 받은 뒤 로컬 진정 전투를 연다.
		const UNUQuestDataSettings* Settings = GetDefault<UNUQuestDataSettings>();
		if (Battle->Kind == ENUQuestBattleKind::Calming && Settings != nullptr && Settings->bUseClientCalmJudgement)
		{
			return RequestClientCalm(Player, Placement, QuestId, ObjectiveId);
		}

		if (!Net->TryReserveBattleStart())
		{
			return false;
		}

		if (Stage != nullptr)
		{
			Stage->SetPendingStageLevel(Placement.BattleStageLevel);
		}

		Server->SendQuestBattleStart(Naia->DataIndex, QuestId, ObjectiveId, Mode);
		return true;
	}

	const UNUProgressionComponent* Progression = Player->GetProgressionComponent();
	const UEmberServerManager* Server = GetGameInstance()->GetSubsystem<UEmberServerManager>();

	if (World->GetNetMode() != NM_Standalone || Progression == nullptr || !Progression->IsUsingLocalProgressionBackend() || (Server != nullptr && Server->IsLoggedIn()))
	{
		return false;
	}

	return StartLocalQuestEncounter(Player, Placement, Battle) != nullptr;
}

ANUBattleEncounter* UNUQuestEncounterSubsystem::StartLocalQuestEncounter(ANUPlayerState* Player, const FNUBattleEncounterRequest& Placement, const UNUQuestBattleDefinition* Battle) const
{
	UNUBattleEncounterSubsystem* Execution = Player->GetWorld()->GetSubsystem<UNUBattleEncounterSubsystem>();
	if (Execution == nullptr)
	{
		return nullptr;
	}

	// 일반 필드 인카운터를 거치지 않고 전투 실행 계층에만 전달한다.
	FNUBattleEncounterRequest Request = Placement;
	Request.Initiator = Player;
	if (Battle->Kind != ENUQuestBattleKind::Sparring)
	{
		// 연습·진정은 전용 실행기만 허용한다. 에셋 누락 시 일반 전투로 대체하지 않는다.
		UClass* TutorialClass = Battle->TutorialEncounterClass.LoadSynchronous();
		if (!IsValid(TutorialClass) || !TutorialClass->IsChildOf(ANUQuestTutorialEncounter::StaticClass())) { return nullptr; }
		Request.EncounterClass = TSoftClassPtr<ANUBattleEncounter>(TutorialClass);
	}
	ANUBattleEncounter* Encounter = nullptr;
	return Execution->TryStartEncounter(Request, Encounter) == ENUBattleEncounterStartResult::Started ? Encounter : nullptr;
}

void UNUQuestEncounterSubsystem::HandleStarted(const FGrpcRevGameSVR_Battle_Started& Message)
{
	const UNUBattleNetSubsystem* Net = GetGameInstance()->GetSubsystem<UNUBattleNetSubsystem>();

	if (Net == nullptr || !Net->IsInBattle() || Net->GetBattleId() != Message.BattleId)
	{
		return;
	}

	if (Message.QuestId <= 0 || Message.ObjectiveId <= 0 || NUQuestBattle::FromServerBattleMode(Message.BattleMode) == ENUQuestBattleKind::None)
	{
		return;
	}

	// 같은 전투의 재동기화가 도착해도 확정된 연습 횟수는 감소시키지 않는다.
	const bool bSameBattle = BattleId == Message.BattleId;
	BattleId = Message.BattleId;
	QuestBattleState.QuestId = Message.QuestId;
	QuestBattleState.ObjectiveId = Message.ObjectiveId;
	QuestBattleState.BattleMode = Message.BattleMode;
	QuestBattleState.PracticeCount = bSameBattle ? FMath::Max(QuestBattleState.PracticeCount, Message.PracticeCount) : Message.PracticeCount;
	QuestBattleState.PracticeRequired = Message.PracticeRequired;
	QuestBattleState.bQuestSuccess = false;

	if (!bSameBattle)
	{
		LastQuestBattleResult = FNUQuestBattleServerState();
	}
	OnPracticeStateChanged.Broadcast();
}

void UNUQuestEncounterSubsystem::HandlePracticeCount(int64 InBattleId, int32 Count)
{
	if (BattleId <= 0 || InBattleId != BattleId || !NUQuestBattle::IsPracticeKind(NUQuestBattle::FromServerBattleMode(QuestBattleState.BattleMode)) || Count < 0)
	{
		return;
	}

	if (Count > QuestBattleState.PracticeCount)
	{
		QuestBattleState.PracticeCount = Count;
		OnPracticeStateChanged.Broadcast();
	}
}

void UNUQuestEncounterSubsystem::HandleEnd(const FGrpcRevGameSVR_Battle_End& Message)
{
	if (BattleId <= 0 || Message.BattleId != BattleId)
	{
		return;
	}

	// 퀘스트 목표 진행은 104만 반영하며 전투 결과로 중복 증가시키지 않는다.
	LastQuestBattleResult = QuestBattleState;
	LastQuestBattleResult.QuestId = Message.QuestId;
	LastQuestBattleResult.ObjectiveId = Message.ObjectiveId;
	LastQuestBattleResult.BattleMode = Message.BattleMode;
	LastQuestBattleResult.bQuestSuccess = Message.QuestSuccess;
}

void UNUQuestEncounterSubsystem::HandleBattleStateChanged()
{
	const UNUBattleNetSubsystem* Net = GetGameInstance()->GetSubsystem<UNUBattleNetSubsystem>();

	if (Net != nullptr && (Net->IsInBattle() || Net->IsBattleStartPending()))
	{
		return;
	}

	// 전투 상태가 종료되면 현재 퀘스트 전투 문맥을 비운다.
	BattleId = 0;
	QuestBattleState = FNUQuestBattleServerState();
	OnPracticeStateChanged.Broadcast();
}

void UNUQuestEncounterSubsystem::SetLocalPracticeState(ENUQuestBattleKind Kind, int32 Count, int32 Required)
{
	if (LocalPracticeKind == Kind && LocalPracticeCount == Count && LocalPracticeRequired == Required)
	{
		return;
	}
	LocalPracticeKind = Kind;
	LocalPracticeCount = Count;
	LocalPracticeRequired = Required;
	OnPracticeStateChanged.Broadcast();
}

void UNUQuestEncounterSubsystem::ClearLocalPracticeState()
{
	SetLocalPracticeState(ENUQuestBattleKind::None, 0, 0);
}

void UNUQuestEncounterSubsystem::RefreshPracticeGuide()
{
	// 서버 연습 전투가 있으면 서버 횟수, 없으면 로컬 연습 전투의 횟수
	ENUQuestBattleKind Kind = NUQuestBattle::FromServerBattleMode(QuestBattleState.BattleMode);
	int32 Count = QuestBattleState.PracticeCount;
	int32 Required = QuestBattleState.PracticeRequired;
	if (BattleId <= 0 || !NUQuestBattle::IsPracticeKind(Kind))
	{
		Kind = LocalPracticeKind;
		Count = LocalPracticeCount;
		Required = LocalPracticeRequired;
	}

	if (!NUQuestBattle::IsPracticeKind(Kind))
	{
		// 전투가 끝나면 흐려진 뒤 스스로 빠진다
		if (UNUQuestPracticeGuideWidget* Guide = PracticeGuide.Get())
		{
			Guide->Dismiss();
		}
		PracticeGuide.Reset();
		return;
	}

	if (UNUQuestPracticeGuideWidget* Guide = EnsurePracticeGuide())
	{
		Guide->SetPractice(Kind, Count, Required);
	}
}

UNUQuestPracticeGuideWidget* UNUQuestEncounterSubsystem::EnsurePracticeGuide()
{
	if (UNUQuestPracticeGuideWidget* Guide = PracticeGuide.Get())
	{
		return Guide;
	}
	APlayerController* Controller = GetGameInstance()->GetFirstLocalPlayerController();
	UNUQuestPracticeGuideWidget* Guide = Controller != nullptr ? CreateWidget<UNUQuestPracticeGuideWidget>(Controller, UNUQuestPracticeGuideWidget::StaticClass()) : nullptr;
	if (Guide == nullptr)
	{
		UE_LOG(LogNUQuest, Warning, TEXT("전투 튜토리얼 안내를 만들지 못했습니다"));
		return nullptr;
	}
	Guide->AddToViewport(NUUILayer::Tracker);
	PracticeGuide = Guide;
	return Guide;
}

// -------------------------------------------------------------------------
// Client Calm Attempt
// -------------------------------------------------------------------------

bool UNUQuestEncounterSubsystem::IsCalmAttemptApproved(const UNUQuestBattleDefinition* Definition) const
{
	return Definition != nullptr && !CalmAttempt.AttemptId.IsEmpty()
		&& Definition->QuestId == FName(*LexToString(CalmAttempt.QuestId))
		&& Definition->ObjectiveId == FName(*LexToString(CalmAttempt.ObjectiveId));
}

bool UNUQuestEncounterSubsystem::RequestClientCalm(ANUPlayerState* Player, const FNUBattleEncounterRequest& Placement, int32 QuestId, int32 ObjectiveId)
{
	// 진정 전투가 진행 중이면 새 도전을 열지 않는다.
	if (CalmAttempt.Encounter.IsValid())
	{
		UE_LOG(LogNUQuest, Warning, TEXT("진정 전투 진행 중이라 새 도전을 열지 않습니다 QuestId=%d ObjectiveId=%d"), QuestId, ObjectiveId);
		return false;
	}

	// 승인 응답을 기다리는 동안 다시 보내면 서버는 새 도전으로 덮어 먼저 온 응답의 도전 ID가 무효가 된다.
	// 응답이 끝내 오지 않는 경우를 위해 잠시 뒤에는 다시 보낼 수 있게 한다.
	constexpr double ResendSeconds = 5.0;
	if (CalmAttempt.IsPending() && CalmAttempt.AttemptId.IsEmpty() && FPlatformTime::Seconds() - CalmAttempt.RequestedAt < ResendSeconds)
	{
		return false;
	}

	UEmberServerManager* Server = GetGameInstance()->GetSubsystem<UEmberServerManager>();
	if (Server == nullptr || !Server->IsLoggedIn())
	{
		return false;
	}

	CalmAttempt = FNUQuestCalmAttempt();
	CalmAttempt.Player = Player;
	CalmAttempt.Placement = Placement;
	CalmAttempt.QuestId = QuestId;
	CalmAttempt.ObjectiveId = ObjectiveId;
	CalmAttempt.RequestedAt = FPlatformTime::Seconds();
	Server->SendQuestCalmStart(QuestId, ObjectiveId);
	return true;
}

void UNUQuestEncounterSubsystem::HandleCalmStartResponse(int32 ResultCode, const FString& AttemptId)
{
	if (!CalmAttempt.IsPending() || !CalmAttempt.AttemptId.IsEmpty())
	{
		UE_LOG(LogNUQuest, Warning, TEXT("기다리는 진정 도전이 없어 승인 응답을 무시합니다 (%d)"), ResultCode);
		return;
	}

	if (ResultCode != 200 || AttemptId.IsEmpty())
	{
		UE_LOG(LogNUQuest, Warning, TEXT("진정 도전 승인 거절 (%d) QuestId=%d ObjectiveId=%d"), ResultCode, CalmAttempt.QuestId, CalmAttempt.ObjectiveId);
		CalmAttempt = FNUQuestCalmAttempt();
		return;
	}

	CalmAttempt.AttemptId = AttemptId;
	ANUPlayerState* Player = CalmAttempt.Player.Get();
	const UNUQuestBattleDefinition* Battle = Cast<UNUQuestBattleDefinition>(CalmAttempt.Placement.BattleDefinition);
	ANUBattleEncounter* Encounter = IsValid(Player) && Battle != nullptr ? StartLocalQuestEncounter(Player, CalmAttempt.Placement, Battle) : nullptr;
	if (Encounter == nullptr)
	{
		// 서버에 열린 도전을 남기지 않도록 실패로 닫는다.
		UE_LOG(LogNUQuest, Error, TEXT("진정 전투를 열지 못해 도전을 실패로 닫습니다 QuestId=%d ObjectiveId=%d"), CalmAttempt.QuestId, CalmAttempt.ObjectiveId);
		ReportCalmResult(false);
		return;
	}

	CalmAttempt.Encounter = Encounter;

	// 준비 실패처럼 시작과 동시에 끝난 전투는 완료 알림이 이미 지나갔으므로 바로 보고한다.
	if (Encounter->GetBattleResult() != ENUBattleResult::None)
	{
		ReportCalmResult(Encounter->GetBattleResult() == ENUBattleResult::Victory);
		return;
	}

	Encounter->OnEncounterCompleted.AddUObject(this, &ThisClass::HandleCalmEncounterCompleted);
	Encounter->OnEndPlay.AddUniqueDynamic(this, &ThisClass::HandleCalmEncounterEndPlay);
	UE_LOG(LogNUQuest, Log, TEXT("진정 도전 시작 QuestId=%d ObjectiveId=%d Attempt=%s"), CalmAttempt.QuestId, CalmAttempt.ObjectiveId, *AttemptId);
}

void UNUQuestEncounterSubsystem::HandleCalmEncounterCompleted(ANUBattleEncounter* Encounter, ENUBattleResult Result)
{
	if (Encounter == nullptr || CalmAttempt.Encounter.Get() != Encounter)
	{
		return;
	}

	// 진정 상대는 피해를 받지 않으므로 승리는 동요를 다 가라앉힌 경우에만 나온다.
	ReportCalmResult(Result == ENUBattleResult::Victory);
}

void UNUQuestEncounterSubsystem::HandleCalmEncounterEndPlay(AActor* Actor, EEndPlayReason::Type EndPlayReason)
{
	// 결과 없이 사라진 전투(맵 이동·강제 종료)는 실패로 닫는다.
	if (Actor != nullptr && CalmAttempt.Encounter.Get() == Actor)
	{
		ReportCalmResult(false);
	}
}

void UNUQuestEncounterSubsystem::ReportCalmResult(bool bSuccess)
{
	if (ANUBattleEncounter* Encounter = CalmAttempt.Encounter.Get())
	{
		Encounter->OnEncounterCompleted.RemoveAll(this);
		Encounter->OnEndPlay.RemoveDynamic(this, &ThisClass::HandleCalmEncounterEndPlay);
	}

	const FNUQuestCalmAttempt Finished = CalmAttempt;
	CalmAttempt = FNUQuestCalmAttempt();
	if (Finished.AttemptId.IsEmpty())
	{
		return;
	}

	UEmberServerManager* Server = GetGameInstance()->GetSubsystem<UEmberServerManager>();
	if (Server == nullptr || !Server->IsLoggedIn())
	{
		// 보내지 못한 도전은 서버에서 10분 뒤 만료된다.
		UE_LOG(LogNUQuest, Warning, TEXT("서버 연결이 없어 진정 결과를 보내지 못했습니다 QuestId=%d ObjectiveId=%d Success=%d"), Finished.QuestId, Finished.ObjectiveId, bSuccess ? 1 : 0);
		return;
	}

	Server->SendQuestCalmResult(Finished.QuestId, Finished.ObjectiveId, Finished.AttemptId, bSuccess);
}

void UNUQuestEncounterSubsystem::HandleCalmResultResponse(int32 ResultCode)
{
	// 목표 진행은 퀘스트 컴포넌트가 104와 이 응답의 본문으로 반영한다. 여기서는 거절만 기록한다.
	if (ResultCode != 200)
	{
		UE_LOG(LogNUQuest, Warning, TEXT("진정 결과 접수 거절 (%d)"), ResultCode);
	}
}
