// Fill out your copyright notice in the Description page of Project Settings.


#include "NUBattleEncounter.h"

#include "Battle/Presentation/NUBattleActionPresentation.h"
#include "Battle/Data/NUBattleDefinition.h"

#include "AbilitySystemComponent.h"
#include "AnimSequences/PaperZDAnimSequence.h"
#include "Audio/NUSFXSubsystem.h"
#include "Audio/NUSFXTags.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "TimerManager.h"
#include "Battle/NUBattleActionContext.h"
#include "Battle/Capture/NUCaptureMath.h"
#include "Battle/Data/NUActiveSkillContext.h"
#include "Battle/Data/NUJobSkillContext.h"
#include "Battle/Data/NUBattleDataSettings.h"
#include "Battle/Data/NUBattleDataSubsystem.h"
#include "Battle/Data/NUStatusEffectDefinition.h"
#include "Battle/Encounter/NUBattleEncounterSubsystem.h"
#include "Battle/Avatar/NUBattleAvatar.h"
#include "Battle/Avatar/NUBattleEnemyAvatar.h"
#include "Battle/Event/NUBattleEventSubsystem.h"
#include "Battle/Enemy/NUEnemyPatternTypes.h"
#include "Battle/Enemy/NUEnemyPatternSelector.h"
#include "Battle/Formation/NUBattleFormationMath.h"
#include "Battle/UI/NUBattleResultTypes.h"
#include "CharacterProgression/Component/NUPlayerSkillComponent.h"
#include "CharacterProgression/Component/NUProgressionComponent.h"
#include "CharacterProgression/Data/NULevelProgression.h"
#include "CharacterProgression/Net/NUExperienceGrowth.h"
#include "Camera/CameraComponent.h"
#include "CharacterState/NUPlayerState.h"
#include "Recovery/Component/NURecoveryComponent.h"
#include "CharacterState/NUSpiritState.h"
#include "NUGAS/NUAbilitySystemComponent.h"
#include "NUGAS/NUGameplayTags.h"
#include "NUGAS/Attribute/NUVitalAttributeSet.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/NUInventoryComponent.h"
#include "Inventory/NUInventoryTags.h"
#include "Inventory/NUInventoryTypes.h"
#include "Kismet/GameplayStatics.h"
#include "PlayerController/NUPlayerController.h"
#include "Net/UnrealNetwork.h"
#include "NUGAS/Ability/NUBattleGameplayAbility.h"
#include "NUGAS/Ability/NUPlayerBattleAbility.h"
#include "Naia/Component/NUNaiaRosterComponent.h"
#include "Naia/Data/NUNaiaDefinition.h"
#include "Naia/Data/NUNaiaDataSubsystem.h"
#include "Engine/GameInstance.h"
#include "NUGAS/Ability/NUEnemyAttackAbility.h"
#include "NUGAS/Attribute/NUCombatAttributeSet.h"

ANUBattleEncounter::ANUBattleEncounter()
{
	PrimaryActorTick.bCanEverTick = false;
	
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	
	BattleCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("BattleCamera"));
	BattleCamera->SetupAttachment(RootComponent);
	
	BattleCamera->SetRelativeLocation(FVector(-1150.f, 0.f, 280.f));
	BattleCamera->SetRelativeRotation(FRotator(-10.f, 0.f, 0.f));
	BattleCamera->SetFieldOfView(45.f);
	BattleCamera->SetConstraintAspectRatio(false);
	
	bFindCameraComponentWhenViewTarget = true;
	
	bReplicates = true;
	SetReplicateMovement(false);	
	
	// 아레나를 공유하므로 거리 컬링이 의미가 없어 항상 복제로 둠
	// 클라가 Standalone 이라 relevancy 가 평가되지 않고, 인카운터를 둘 이상 만드는 경로도 없음
	// UE 서버와 파티 전투가 생기면 IsNetRelevantFor 로 참가자 커넥션만 통과시켜야 함
	bAlwaysRelevant = true;
	
	// 처음엔 가리고 이후 암전이 끝나면 보여지게 해야함
	SetHidden(true);
}

bool ANUBattleEncounter::ConfigureBattleDefinition(UNUBattleDefinition* Definition)
{
	if (BattleEventHandler.GetObject() != nullptr || Phase != ENUBattlePhase::None) { return false; }
	if (Definition == nullptr) { return true; }

	BattleEventHandler = Definition->GetEventHandler(this);
	if (BattleEventHandler.GetInterface() == nullptr) { return false; }

	BattleDefinition = Definition;
	return true;
}

void ANUBattleEncounter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindEnemyDefeatEvents();

	// 초기화 실패로 파괴되거나 레벨이 바뀌어도 필드 조명은 돌아와야 함
	SetBattleStageLightingActive(false);

	Super::EndPlay(EndPlayReason);
}

void ANUBattleEncounter::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(ThisClass, EncounterId);
	DOREPLIFETIME(ThisClass, Phase);
	DOREPLIFETIME(ThisClass, TurnNumber);
	DOREPLIFETIME(ThisClass, Participants);
	DOREPLIFETIME(ThisClass, ActionSequence);
	DOREPLIFETIME(ThisClass, TransitionCue);
	DOREPLIFETIME(ThisClass, BattleResult);
}

// 참가자가 유효한지 확인하고 초기화하는 함수
bool ANUBattleEncounter::InitializeEncounter(const TArray<FNUBattleParticipant>& InitialParticipants)
{
	if (!HasAuthority() 
		|| Phase != ENUBattlePhase::None
		|| InitialParticipants.IsEmpty())
	{
		return false;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UNUNaiaDataSubsystem* NaiaDataSubsystem =
		GameInstance
		? GameInstance->GetSubsystem<UNUNaiaDataSubsystem>()
		: nullptr;

	// 참가자 유효한지 먼저 확인
	if (!ValidateInitialParticipants(InitialParticipants, NaiaDataSubsystem))
	{
		return false;
	}
	
	// 결과 이벤트의 중복 처리 방지와 명령 검증에 전투별 식별자가 필요하다.
	EncounterId = FGuid::NewGuid();
	SetBattlePhase(ENUBattlePhase::Preparing);
	
	EnemyPatternRuntimes.Reset();	// 쿨타임과 사용 횟수는 전투마다 초기화
	PendingCaptures.Reset();	// 포획한 전투원 전투마다 초기화
	ReportedDefeatedEnemyCombatants.Reset();
	UnbindEnemyDefeatEvents();

	Participants.Reset();
	for (FNUBattleParticipant Participant : InitialParticipants)
	{
		Participant.CombatantId = NextCombatantId++;
		Participants.Add(MoveTemp(Participant));
		
		// 전투 참가자 배정하면서 BattleEncounter는 지정해야함
		// 나중에 지정하면 TransitionCue 정보를 알 수 없음
		if (Participant.Team != ENUBattleTeam::Ally)
		{
			continue;
		}
		
		if (INUCombatantInterface* CombatantInterface = Cast<INUCombatantInterface>(Participant.ASCOwner))
		{
			CombatantInterface->SetBattleEncounter(this);	
		}
	}
	
	// 배경 로딩을 전환 연출 시간에 가려지게 함
	RequestBattleStage();

	// 아바타는 아직 만들지 않음, 암전이 끝난 뒤 스폰해야 교체 순간이 가려짐
	SetTransitionCue(ENUTransitionCue::FadeOut);
	ForceNetUpdate();

	GetWorldTimerManager().SetTimer(
		TransitionTimer,
		this,
		&ThisClass::SpawnBattleParticipants,
		GetTransitionSeconds(),
		false
		);

	return true;
}

void ANUBattleEncounter::SetBattleStageLevel(const TSoftObjectPtr<UWorld>& StageLevel)
{
	BattleStageLevel = StageLevel;
}

void ANUBattleEncounter::RequestBattleStage()
{
	UWorld* World = GetWorld();
	UNUBattleEncounterSubsystem* EncounterSubsystem = IsValid(World)
		? World->GetSubsystem<UNUBattleEncounterSubsystem>()
		: nullptr;

	if (!IsValid(EncounterSubsystem))
	{
		return;
	}

	// 비어 있으면 서브시스템이 배경을 건드리지 않음
	EncounterSubsystem->EnsureStageLoaded(BattleStageLevel);
}

bool ANUBattleEncounter::IsBattleStageReady() const
{
	const UWorld* World = GetWorld();
	const UNUBattleEncounterSubsystem* EncounterSubsystem = IsValid(World)
		? World->GetSubsystem<UNUBattleEncounterSubsystem>()
		: nullptr;

	// 서브시스템이 없으면 기다릴 방법이 없으므로 그냥 진행함
	return !IsValid(EncounterSubsystem) || EncounterSubsystem->IsStageReady();
}

bool ANUBattleEncounter::WaitForBattleStage()
{
	if (IsBattleStageReady())
	{
		return true;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return true;
	}

	if (StageWaitStartTime < KINDA_SMALL_NUMBER)
	{
		StageWaitStartTime = World->GetTimeSeconds();
	}

	// 에셋 경로가 잘못되면 무한정 대기하므로, 최대 시간을 넘으면 로그를 띄우고 그냥 시작함
	if (World->GetTimeSeconds() - StageWaitStartTime >= MaxStageWaitSeconds)
	{
		UE_LOG(LogTemp, Warning, TEXT("전투 배경을 %.1f초 기다렸지만 올라오지 않아 배경 없이 시작합니다"),
			MaxStageWaitSeconds
			);
		return true;
	}

	// TransitionTimer 를 재사용해야 함, 새 핸들을 쓰면 타이머가 둘이 돌아 두 번 스폰됨
	GetWorldTimerManager().SetTimer(
		TransitionTimer,
		this,
		&ThisClass::SpawnBattleParticipants,
		StageRetrySeconds,
		false
		);

	return false;
}

void ANUBattleEncounter::SetBattleStageLightingActive(bool bActive)
{
	// 이 인카운터가 켠 것만 되돌림
	// 켠 적이 없거나 이미 되돌렸으면 다음 전투의 조명이므로 건드리면 안 됨
	if (bStageLightingActive == bActive)
	{
		return;
	}

	UWorld* World = GetWorld();
	UNUBattleEncounterSubsystem* EncounterSubsystem = IsValid(World)
		? World->GetSubsystem<UNUBattleEncounterSubsystem>()
		: nullptr;

	if (!IsValid(EncounterSubsystem))
	{
		return;
	}

	bStageLightingActive = bActive;

	if (bActive)
	{
		// 세트 조명 적용
		// 세트가 조명을 안 들고 있으면 조명은 그대로, 안개는 세트와 무관하게 필드 것이 꺼짐
		EncounterSubsystem->ApplyStageLighting();
		return;
	}

	EncounterSubsystem->RestoreFieldLighting();
}

// 암전이 끝난 뒤 호출됨, 여기서 실패하면 호출자가 이미 성공으로 처리했으므로
// 스스로 정리하고 파괴해야 한다
void ANUBattleEncounter::SpawnBattleParticipants()
{
	if (!HasAuthority())
	{
		return;
	}

	// 배경이 아직 안 올라왔으면 암전을 유지한 채 기다림
	// 그냥 진행하면 텅 빈 아레나가 보임, 전환 시퀀스가 Keep State 로 두면 검은화면 유지됨
	if (!WaitForBattleStage())
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	const UNUNaiaDataSubsystem* NaiaDataSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UNUNaiaDataSubsystem>() : nullptr;

	if (!IsValid(NaiaDataSubsystem))
	{
		// 실패하면 정리하고 스스로 파괴
		AbortInitialize();
		Destroy();
		return;
	}

	// PlayerState만 저장
	// 플레이어와 정령을 같은 줄에 배치하기 위함
	TMap<APlayerState*, int32> AllySlotByOwner;
	for (const FNUBattleParticipant& Participant : Participants)
	{
		APlayerState* OwningPlayerState = Participant.OwningPlayerState.Get();
		if (Participant.Team == ENUBattleTeam::Ally
			&& IsValid(OwningPlayerState)
			&& !AllySlotByOwner.Contains(OwningPlayerState))
		{
			AllySlotByOwner.Add(OwningPlayerState, AllySlotByOwner.Num());
		}
	}

	int32 EnemySlotIndex = 0;

	// 적 진형을 가운데 정렬하려면 총 마리 수를 미리 알아야 함
	int32 EnemyCount = 0;
	for (const FNUBattleParticipant& Participant : Participants)
	{
		if (Participant.Team == ENUBattleTeam::Enemy)
		{
			++EnemyCount;
		}
	}

	// 플레이어, 적 아바타 생성 및 등록
	for (FNUBattleParticipant& Participant : Participants)
	{
		OriginAvatars.Add(Participant.CombatantId, Participant.AvatarActor);
		
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		
		if (Participant.Team == ENUBattleTeam::Ally)
		{
			// 플레이어 진영이면 기존 아바타 숨기기
			// 아바타 액터 == ASCOwner 면 State가 Avatar인 경우라 안숨겨도 됨
			if (IsValid(Participant.AvatarActor) && Participant.AvatarActor != Participant.ASCOwner)
			{
				if (ACharacter* FieldCharacter = Cast<ACharacter>(Participant.AvatarActor))
				{
					UCharacterMovementComponent* Movement = FieldCharacter->GetCharacterMovement();

					Movement->StopMovementImmediately();
					Movement->DisableMovement();
				}
				
				Participant.AvatarActor->SetActorHiddenInGame(true);
				Participant.AvatarActor->SetActorEnableCollision(false);
			}
			
			bool bIsSpirit = false;
			TSubclassOf<ANUBattleAvatar> SpawnClass = 
				ResolveAllyAvatarClass(Participant.ASCOwner, bIsSpirit);
			
			const int32* PartySlotIndex = AllySlotByOwner.Find(Participant.OwningPlayerState.Get());
			FTransform SpawnTransform = GetAllySpawnTransform(
				*PartySlotIndex,
				AllySlotByOwner.Num(),
				bIsSpirit
				);
			
			ANUBattleAvatar* BattleAvatar = GetWorld()->SpawnActor<ANUBattleAvatar>(
				SpawnClass,
				SpawnTransform,
				SpawnParams
				);
			if (!IsValid(BattleAvatar))
			{
				// 스폰 실패하면 되돌리고 스스로 파괴함
				UE_LOG(LogTemp, Error, TEXT("아군 스폰에 실패했습니다!"));
				AbortInitialize();
				Destroy();
				return;
			}
			
			BattleAvatar->InitializeBattleAvatar(Participant.ASCOwner, Participant.CombatantId);
			BattleAvatar->SetIsAlly(true);

			// 정령이면 정령 ID 세팅
			if (const ANUSpiritState* Spirit = Cast<ANUSpiritState>(Participant.ASCOwner))
			{
				BattleAvatar->SetNaiaId(Spirit->GetSpiritId());
			}

			// 방향 규칙은 ResolveBattleFacing 에서 처리
			BattleAvatar->SetBattleFacing(
				ANUBattleAvatar::ResolveBattleFacing(true, BattleAvatar->IsPlayerVisual()));

			// 아직 암전 중, 카메라 페이드가 UMG를 덮지 못하므로 직접 숨김
			BattleAvatar->SetVitalBarHidden(true);

			Participant.AvatarActor = BattleAvatar;
			
			INUCombatantInterface* CombatantState = Cast<INUCombatantInterface>(Participant.ASCOwner);
			CombatantState->SetCurrentAvatar(BattleAvatar);
			
			UE_LOG(LogTemp, Warning, TEXT("Player 아바타 변경!"));
		}
		else
		{
			// 적 진영이면
			const FNUNaiaDefinition* NaiaDefinition = NaiaDataSubsystem->FindDefinition(Participant.EnemySpec.EnemyId);
			FTransform SpawnTransform = GetEnemySpawnTransform(EnemySlotIndex++, EnemyCount);
			
			ANUBattleEnemyAvatar* EnemyAvatar = GetWorld()->SpawnActor<ANUBattleEnemyAvatar>(
				EnemyClass,
				SpawnTransform,
				SpawnParams
				);
			if (!IsValid(EnemyAvatar))
			{
				// 스폰 실패하면 되돌리고 스스로 파괴함
				UE_LOG(LogTemp, Error, TEXT("적 스폰에 실패했습니다!"));
				AbortInitialize();
				Destroy();
				return;
			}

			EnemyAvatar->SetBattleEncounter(this);
			EnemyAvatar->InitializeBattleAvatar(EnemyAvatar, Participant.CombatantId);
			EnemyAvatar->SetIsAlly(false);
			EnemyAvatar->InitializeEnemy(*NaiaDefinition);
			EnemyAvatar->SetVitalBarHidden(true);
			EnemyAvatar->SetNaiaId(NaiaDefinition->DataIndex);	// 적 모습 적용

			// 방향 규칙은 ResolveBattleFacing 에서 처리
			EnemyAvatar->SetBattleFacing(ANUBattleAvatar::ResolveBattleFacing(false, false));

			Participant.ASCOwner = EnemyAvatar;
			Participant.AvatarActor = EnemyAvatar;
			
			UE_LOG(LogTemp, Warning, TEXT("적 Avatar 생성: EnemyId=%s, Name=%s, Level=%d"),
				*Participant.EnemySpec.EnemyId.ToString(),
				*NaiaDefinition->KoreanName.ToString(),
				EnemyAvatar->GetLevel()
			);
		}

		// 참가자 정보와 초기 스탯이 모두 정해진 뒤에 체력바를 채움
		// 바인딩 시점에 채우면 적은 ASCOwner 와 초기 스탯이 아직 없어 0/0 으로 표시됨
		if (ANUBattleAvatar* Avatar = Cast<ANUBattleAvatar>(Participant.AvatarActor))
		{
			Avatar->RefreshVitalWidget();
		}

		// 생성한 아바타가 이미 죽어있다면 사망 표시를 반영
		if (ANUBattleAvatar* Avatar = Cast<ANUBattleAvatar>(Participant.AvatarActor))
		{
			if (!IsAlive(Participant.CombatantId))
			{
				Avatar->SetDeadVisual(true);
			}
		}
	}

	// 아직 암전 중이라 여기서 불러도 끊김이 안 보임
	PreloadConcessionAnimations();
	PreloadActionVFX();

	// 전진 연출의 기준, 아직 아무도 움직이지 않은 시점이어야 함
	CacheFormationCenters();

	BindEnemyDefeatEvents();

	// 조명 변경
	SetBattleStageLightingActive(true);

	// 암전이 끝난 시점이라 이제 Encounter의 아레나 배경이 보이도록 처리
	SetActorHiddenInGame(false);

	ForceNetUpdate();

	// 커맨드창이 검은 화면 위에 뜨지 않도록 암전 해제를 먼저 요청
	SetTransitionCue(ENUTransitionCue::FadeIn);

	// 첫 커맨드 전에 전원 SP 만충
	DeferredSPRefillIds.Reset();
	RefillSPForAll();
	if (GetBattleEventHandler() != nullptr && !GetBattleEventHandler()->PrepareParticipants(this))
	{
		FinishBattle(ENUBattleResult::Defeat);
		return;
	}

	BeginCommandSelection();
}

ENUBattleCommandSubmitResult ANUBattleEncounter::TrySubmitCommand(APlayerController* Requester,
	const FNUBattleCommandRequest& Request)
{
	if (!HasAuthority() || !IsValid(Requester))
	{
		return ENUBattleCommandSubmitResult::Rejected;
	}

	if (Phase != ENUBattlePhase::CommandSelection)
	{
		return ENUBattleCommandSubmitResult::WrongPhase;
	}

	// 요청이 밀려서 이전턴 요청이 들어온 경우
	if (Request.TurnNumber != TurnNumber)
	{
		return ENUBattleCommandSubmitResult::WrongTurn;
	}

	FNUBattleCommandRequest AcceptedRequest = Request;
	if (GetBattleEventHandler() != nullptr && !GetBattleEventHandler()->IsAbilityAllowed(this, Request.AbilityId)) { return ENUBattleCommandSubmitResult::InvalidAbility; }

	FNUBattleParticipant* Source = FindParticipant(AcceptedRequest.SourceCombatantId);
	if (!Source)
	{
		return ENUBattleCommandSubmitResult::InvalidSource;
	}

	if (!IsAlive(Source->CombatantId))
	{
		return ENUBattleCommandSubmitResult::SourceDead;
	}

	// 클라가 SourceCombatantId를 임의로 보낼 수 있으므로 요청자가 실제로 조종하는 전투원인지 확인함
	TArray<int32> CommandableIds;
	GetCommandableCombatantIds(Requester->PlayerState, CommandableIds);
	if (!CommandableIds.Contains(AcceptedRequest.SourceCombatantId))
	{
		UE_LOG(LogTemp, Warning, TEXT("조종할 수 없는 전투원에 대한 명령입니다 : %d"),
			AcceptedRequest.SourceCombatantId);
		return ENUBattleCommandSubmitResult::NotOwner;
	}

	ENUTargetPolicy Policy = ResolveCommandTargetPolicy(AcceptedRequest);

	TArray<int32> Targets;
	if (!ResolveTargets(*Source, Policy, AcceptedRequest.SelectedTargetId, Targets))
	{
		return ENUBattleCommandSubmitResult::InvalidTarget;
	}
	AcceptedRequest.SelectedTargetId = Targets[0];

	switch (AcceptedRequest.CommandType)
	{
	case ENUBattleCommandType::Ability:
		if (!AcceptedRequest.AbilityId.IsValid())
		{
			return ENUBattleCommandSubmitResult::InvalidAbility;
		}

		// 스킬은 보유 여부를 서버가 확인해야 함
		// 여기서 막지 않으면 ExecuteAbilityCommand가 로그만 남기고 넘어가서
		// 이 전투원의 턴이 소모됨
		if (AcceptedRequest.SkillId > 0)
		{
			// 실명이면 액티브 스킬을 쓸 수 없음, 기본 공격은 가능
			// 클라가 목록을 비워 보여주므로 여기 걸리는 것은 오래된 요청이나 위조뿐임
			if (IsSkillBlocked(AcceptedRequest.SourceCombatantId, AcceptedRequest.SkillId))
			{
				UE_LOG(LogTemp, Warning, TEXT("막힌 액티브 스킬을 제출했습니다 : %d (전투원 %d)"),
					AcceptedRequest.SkillId, AcceptedRequest.SourceCombatantId);
				
				return ENUBattleCommandSubmitResult::InvalidAbility;
			}
			
			const UNUAbilitySystemComponent* SourceASC = Cast<UNUAbilitySystemComponent>(
				UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Source->ASCOwner));

			if (!IsValid(SourceASC) || !SourceASC->HasActiveSkill(AcceptedRequest.SkillId))
			{
				UE_LOG(LogTemp, Warning, TEXT("보유하지 않은 스킬을 제출했습니다 : %d (전투원 %d)"),
					AcceptedRequest.SkillId, AcceptedRequest.SourceCombatantId);

				return ENUBattleCommandSubmitResult::InvalidAbility;
			}

			// 전투당 횟수를 다 쓴 스킬 (3310 역방향 등), 클라가 메뉴에서 빼므로 여기 걸리는 것은 오래된 요청이나 위조뿐임
			const UGameInstance* BattleGameInstance = GetGameInstance();
			const UNUBattleDataSubsystem* BattleData = BattleGameInstance
				? BattleGameInstance->GetSubsystem<UNUBattleDataSubsystem>()
				: nullptr;

			const int32 MaxUses = IsValid(BattleData)
				? NUJobSkill::FindMaxUsesPerBattle(BattleData->FindJobSkillEffects(AcceptedRequest.SkillId))
				: 0;

			if (MaxUses > 0 && SourceASC->GetActiveSkillUseCount(AcceptedRequest.SkillId) >= MaxUses)
			{
				UE_LOG(LogTemp, Warning, TEXT("전투당 횟수를 다 쓴 스킬을 제출했습니다 : %d (전투원 %d)"),
					AcceptedRequest.SkillId, AcceptedRequest.SourceCombatantId);

				return ENUBattleCommandSubmitResult::InvalidAbility;
			}
		}
		break;

	case ENUBattleCommandType::Item:
		{
			const FNUItemDefRow* Def = FindItemDef(AcceptedRequest);
			if (!Def || !Def->bUsableInBattle)
			{
				return ENUBattleCommandSubmitResult::InvalidItem;
			}

			// 정령은 교감 아이템을 쓸 수 없음
			// 위조된 요청이 들어오면 여기서 막아야 함
			if (Def->ItemType == ENUItemType::Commune
				&& !IsPlayerCombatant(AcceptedRequest.SourceCombatantId))
			{
				UE_LOG(LogTemp, Warning, TEXT("교감 아이템은 플레이어만 사용할 수 있습니다 : %d"),
					AcceptedRequest.SourceCombatantId);
				return ENUBattleCommandSubmitResult::CannotCommune;
			}
			break;
		}

	default:
		return ENUBattleCommandSubmitResult::Rejected;
	}

	// SP 소모 예약, 실패 원인은 SP 부족뿐임
	if (!ReserveSPForCommand(AcceptedRequest))
	{
		return ENUBattleCommandSubmitResult::NotEnoughSP;
	}

	PendingCommands.Add(AcceptedRequest.SourceCombatantId, MoveTemp(AcceptedRequest));
	
	// 전부 요청이 끝났다면 적 행동 설정
	// 속도를 이용해서 모든 행동의 순서 결정
	if (HasAllRequestedCommands())
	{
		GetWorldTimerManager().ClearTimer(CommandLimitTimer);
		
		GenerateEnemyCommands();
		BuildActionQueue();
	}

	return ENUBattleCommandSubmitResult::Accepted;
}

void ANUBattleEncounter::RequestFlee(int32 RequesterCombatantId)
{
	if (!HasAuthority())
	{
		return;
	}
	
	// 종료 단계나 커맨드 선택 중 들어온 요청은 무시
	if (Phase != ENUBattlePhase::ActionExecution)
	{
		return;
	}

	// 같은 턴에 여러 명이 도주해도 한 번만
	if (bFleeRequested)
	{
		return;
	}

	// 적의 도주는 Fled가 아니라 별도 결과여야 하므로 아군만 허용
	const FNUBattleParticipant* Requester = FindParticipant(RequesterCombatantId);
	if (!Requester || Requester->Team != ENUBattleTeam::Ally)
	{
		return;
	}

	bFleeRequested = true;
	UNUSFXSubsystem::Play(this, NU::SFX::Battle::Flee);
	
	UE_LOG(LogTemp, Warning, TEXT("전투원 %d 도주 요청 — 현재 행동이 끝나면 전투를 종료합니다"), RequesterCombatantId);
}

FGuid ANUBattleEncounter::GetEncounterId() const
{
	return EncounterId;
}

ENUBattlePhase ANUBattleEncounter::GetPhase() const
{
	return Phase;
}

int32 ANUBattleEncounter::GetTurnNumber() const
{
	return TurnNumber;
}

bool ANUBattleEncounter::IsAdvancingRound() const
{
	return bAdvancingRound;
}

int32 ANUBattleEncounter::GetActionSequence() const
{
	return ActionSequence;
}

ENUTransitionCue ANUBattleEncounter::GetTransitionCue() const
{
	return TransitionCue;
}

ULevelSequence* ANUBattleEncounter::GetTransitionSequence(ENUTransitionCue Cue) const
{
	switch (Cue)
	{
	case ENUTransitionCue::FadeOut:	
		return FadeOutSequence.LoadSynchronous();
		
	case ENUTransitionCue::FadeIn:	
		return FadeInSequence.LoadSynchronous();
		
	default:						
		return nullptr;
	}
}

// 배틀 결과 필드로 반환
ENUBattleResult ANUBattleEncounter::GetBattleResult() const
{
	return BattleResult;
}

void ANUBattleEncounter::SetTransitionCue(ENUTransitionCue NewCue)
{
	if (!HasAuthority() || TransitionCue == NewCue)
	{
		return;
	}

	TransitionCue = NewCue;
	OnTransitionCue.Broadcast(TransitionCue);	// 리슨 서버일 경우
	ForceNetUpdate();
}

void ANUBattleEncounter::OnRep_TransitionCue()
{
	OnTransitionCue.Broadcast(TransitionCue);
}

const FNUBattleParticipant* ANUBattleEncounter::FindFirstParticipant(ENUBattleTeam Team) const
{
	for (const FNUBattleParticipant& Participant : Participants)
	{
		if (Participant.Team == Team && IsAlive(Participant.CombatantId))
		{
			return &Participant;
		}
	}
	
	return nullptr;
}

const FNUBattleParticipant* ANUBattleEncounter::FindRandomParticipant(ENUBattleTeam Team) const
{
	TArray<const FNUBattleParticipant*> Candidates;
	
	for (const FNUBattleParticipant& Participant : Participants)
	{
		if (Participant.Team == Team && IsAlive(Participant.CombatantId))
		{
			Candidates.Add(&Participant);
		}
	}
	
	if (Candidates.IsEmpty())
	{
		return nullptr;
	}
	
	int32 RandomIndex = FMath::RandHelper(Candidates.Num());
	
	return Candidates[RandomIndex];
}

const UNUBattleGameplayAbility* ANUBattleEncounter::FindBattleAbility(const FNUBattleCommandRequest& Command) const
{
	const FNUBattleParticipant* Source = FindParticipant(Command.SourceCombatantId);
	if (!Source)
	{
		return nullptr;
	}
	
	UNUAbilitySystemComponent* ASC = Cast<UNUAbilitySystemComponent>(
	  UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Source->ASCOwner));
	if (!IsValid(ASC))
	{
		return nullptr;
	}

	FGameplayAbilitySpecHandle Handle = ASC->FindBattleAbilityHandle(Command.AbilityId);
	FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handle);
	
	return Spec ? Cast<UNUBattleGameplayAbility>(Spec->Ability) : nullptr;
}

const FNUItemDefRow* ANUBattleEncounter::FindItemDef(const FNUBattleCommandRequest& Command) const
{
	const FNUBattleParticipant* Source = FindParticipant(Command.SourceCombatantId);
	if (!Source || !Source->OwningPlayerState)
	{
		return nullptr;
	}

	ANUPlayerState* PlayerState = Cast<ANUPlayerState>(Source->OwningPlayerState);
	UNUInventoryComponent* Inventory = PlayerState ? PlayerState->GetInventoryComponent() : nullptr;
	if (!Inventory)
	{
		return nullptr;
	}

	const FNUItemInstance* Instance = Inventory->FindInstance(Command.InstanceId);
	return Instance ? Inventory->FindItemDef(Instance->ItemRowName) : nullptr;
}

bool ANUBattleEncounter::GetParticipant(int32 CombatantId, FNUBattleParticipant& OutParticipant) const
{
	const FNUBattleParticipant* Participant = FindParticipant(CombatantId);
	if (!Participant)
	{
		return false;
	}
	
	OutParticipant = *Participant;
	return true;
}

int32 ANUBattleEncounter::FindCombatantIdByOwner(const AActor* ASCOwner) const
{
	if (!IsValid(ASCOwner))
	{
		return INDEX_NONE;
	}

	for (const FNUBattleParticipant& Participant : Participants)
	{
		if (Participant.ASCOwner == ASCOwner)
		{
			return Participant.CombatantId;
		}
	}

	return INDEX_NONE;
}

void ANUBattleEncounter::GetLivingCombatantIds(ENUBattleTeam Team, TArray<int32>& OutTargetIds) const
{
	OutTargetIds.Reset();
	
	for (const FNUBattleParticipant& Participant : Participants)
	{
		if (Participant.Team == Team && IsAlive(Participant.CombatantId))
		{
			OutTargetIds.Add(Participant.CombatantId);
		}
	}
}

void ANUBattleEncounter::GetDownedCombatantIds(ENUBattleTeam Team, TArray<int32>& OutTargetIds) const
{
	OutTargetIds.Reset();

	for (const FNUBattleParticipant& Participant : Participants)
	{
		if (Participant.Team == Team && !IsAlive(Participant.CombatantId))
		{
			OutTargetIds.Add(Participant.CombatantId);
		}
	}
}

void ANUBattleEncounter::GetUpcomingTurnOrder(TArray<int32>& OutCombatantIds) const
{
	OutCombatantIds.Reset();
	OutCombatantIds.Reserve(ActionQueue.Num());

	for (const FNUResolvedAction& Action : ActionQueue)
	{
		OutCombatantIds.Add(Action.Command.SourceCombatantId);
	}
}

int32 ANUBattleEncounter::GetRunningActionSPSpend(const UNUAbilitySystemComponent* ASC) const
{
	// 큐를 다 소진하면 CurrentActionIndex 가 범위를 벗어나므로 턴 경계의 도트는 여기서 걸러짐
	if (!IsValid(ASC) || Phase != ENUBattlePhase::ActionExecution
		|| !ActionQueue.IsValidIndex(CurrentActionIndex))
	{
		return 0;
	}
	
	const FNUBattleCommandRequest& Command = ActionQueue[CurrentActionIndex].Command;
	
	// 남이 건 도트·폭발로 죽은 것은 이 행동의 소모가 아님
	return FindCombatantASC(Command.SourceCombatantId) == ASC ? Command.SPSpend : 0;
}

bool ANUBattleEncounter::HasActionLeftThisTurn(const UNUAbilitySystemComponent* ASC) const
{
	// 행동 단계가 아니면 큐에 지난 턴의 순서가 남아 있을 수 있음
	if (!IsValid(ASC) || Phase != ENUBattlePhase::ActionExecution)
	{
		return false;
	}
	
	// 건너뛰는 행동(기절 · 마비)도 차례가 지나갈 때 SP 를 채우므로 남은 것으로 셈
	for (int32 Index = FMath::Max(0, CurrentActionIndex); Index < ActionQueue.Num(); ++Index)
	{
		if (FindCombatantASC(ActionQueue[Index].Command.SourceCombatantId) == ASC)
		{
			return true;
		}
	}
	
	return false;
}

void ANUBattleEncounter::GetCommandableCombatantIds(const APlayerState* OwningPlayerState,
	TArray<int32>& OutCombatantIds) const
{
	OutCombatantIds.Reset();
	
	if (!IsValid(OwningPlayerState))
	{
		return;
	}
	
	for (const FNUBattleParticipant& Participant : Participants)
	{
		if (Participant.Team != ENUBattleTeam::Ally
			|| Participant.OwningPlayerState != OwningPlayerState
			|| !IsAlive(Participant.CombatantId))
		{
			continue;
		}
		
		OutCombatantIds.Add(Participant.CombatantId);
	}
	
	// 임시로 플레이어 -> 정령 순으로 정렬 
	OutCombatantIds.Sort();
}

FVector ANUBattleEncounter::GetActionLocation(int32 SourceCombatantId, int32 TargetCombatantId,
	ENUBattleActionMotion Motion) const
{
	const FNUBattleParticipant* Source = FindParticipant(SourceCombatantId);
	if (!Source || !IsValid(Source->AvatarActor))
	{
		return GetActorLocation();
	}
	
	// 제자리 행동은 지금 서 있는 자리, 전진은 진형 자리 기준 (돌아오는 중인 전투원을 읽지 않게)
	if (Motion == ENUBattleActionMotion::InPlace)
	{
		return Source->AvatarActor->GetActorLocation();
	}

	const FVector SourceWorld = NUBattlePresentation::GetFormationLocation(Source->AvatarActor);

	// 전진 방향을 진형에서 유도함, 축이 좌우든 깊이든 사선이든 같은 계산으로 처리됨
	FVector Advance = FVector::ZeroVector;
	if (!GetAdvanceDirection(Source->Team, Advance))
	{
		return SourceWorld;
	}

	const FNUBattleParticipant* Target = FindParticipant(TargetCombatantId);
	const bool bHasTarget = Target && IsValid(Target->AvatarActor);
	if (Motion == ENUBattleActionMotion::MoveToTarget && !bHasTarget)
	{
		// 타깃이 사라졌으면 중앙 연출로 전환
		return GetActionLocation(
			SourceCombatantId,
			INDEX_NONE,
			ENUBattleActionMotion::MoveToCenter
			);
	}

	return NUBattleFormation::ResolveActionLocation(
		Motion,
		SourceWorld,
		bHasTarget ? NUBattlePresentation::GetFormationLocation(Target->AvatarActor) : FVector::ZeroVector,
		bHasTarget,
		GetActorLocation(),
		Advance,
		ActionLineOffset,
		TargetApproachDistance
		);
}

void ANUBattleEncounter::CacheFormationCenters()
{
	FVector AllySum = FVector::ZeroVector;
	FVector EnemySum = FVector::ZeroVector;
	int32 AllyCount = 0;
	int32 EnemyCount = 0;

	for (const FNUBattleParticipant& Participant : Participants)
	{
		if (!IsValid(Participant.AvatarActor))
		{
			continue;
		}

		if (Participant.Team == ENUBattleTeam::Ally)
		{
			AllySum += Participant.AvatarActor->GetActorLocation();
			++AllyCount;
			continue;
		}

		EnemySum += Participant.AvatarActor->GetActorLocation();
		++EnemyCount;
	}

	bFormationCentersValid = AllyCount > 0 && EnemyCount > 0;
	if (!bFormationCentersValid)
	{
		UE_LOG(LogTemp, Warning, TEXT("진형 중심을 구하지 못해 전진 연출이 제자리로 처리됩니다. 아군=%d 적=%d"),
			AllyCount,
			EnemyCount
			);
		return;
	}

	AllyFormationCenter = AllySum / AllyCount;
	EnemyFormationCenter = EnemySum / EnemyCount;

	// 두 중심이 수평으로 겹치면 전진 방향을 만들 수 없음
	// 높이는 제외하고 XY로만 일치 검사
	if (FVector::DistSquared2D(AllyFormationCenter, EnemyFormationCenter) <= UE_SMALL_NUMBER)
	{
		bFormationCentersValid = false;

		UE_LOG(LogTemp, Warning,
			TEXT("두 진영의 진형 중심이 수평으로 겹쳐 전진 방향을 정할 수 없습니다. 높이만 다르게 배치했는지 확인하세요")
			);
	}
}

bool ANUBattleEncounter::GetAdvanceDirection(ENUBattleTeam Team, FVector& OutDirection) const
{
	if (!bFormationCentersValid)
	{
		return false;
	}

	return NUBattleFormation::ResolveAdvanceDirection(
		AllyFormationCenter,
		EnemyFormationCenter,
		Team == ENUBattleTeam::Ally,
		OutDirection
		);
}

void ANUBattleEncounter::ResolveAbilityTargetActors(int32 SourceCombatantId, ENUTargetPolicy Policy,
	int32 SelectedTargetId, TArray<AActor*>& OutTargetActors) const
{
	OutTargetActors.Reset();
	
	const FNUBattleParticipant* Source = FindParticipant(SourceCombatantId);
	if (!Source)
	{
		return;
	}
	
	TArray<int32> TargetIds;
	if (!ResolveTargets(*Source, Policy, SelectedTargetId, TargetIds))
	{
		return;
	}
	
	for (int32 TargetId : TargetIds)
	{
		const FNUBattleParticipant* Target = FindParticipant(TargetId);
		if (Target && IsValid(Target->AvatarActor))
		{
			OutTargetActors.Add(Target->AvatarActor);
		}
	}
}

void ANUBattleEncounter::RegisterParryPress(const APlayerState* Request, int32 ForActionSequence, float PressOffset)
{
	if (!HasAuthority() || Phase != ENUBattlePhase::ActionExecution)
	{
		return;
	}
	
	// 행동 순서가 다르면 입력 버림
	if (ForActionSequence != ActionSequence)
	{
		return;
	}
	
	// 패링 불가능한 어빌리티면 넘김
	UNUBattleGameplayAbility* Attack = GetActiveBattleAbility();
	if (!Attack || !Attack->IsCurrentlyParryable())
	{
		return;
	}
	
	// 플레이어 소유의 정령까지 같이 패링 결과 반영하도록 묶음
	TArray<int32> OwnedIds;
	GetCommandableCombatantIds(Request, OwnedIds);
	
	for (int32 CombatantId : OwnedIds)
	{
		Attack->RegisterParryPress(CombatantId, PressOffset);
	}
}

void ANUBattleEncounter::RegisterDodgeRequest(const APlayerState* Request, int32 ForActionSequence)
{
	if (!HasAuthority() || Phase != ENUBattlePhase::ActionExecution)
	{
		return;
	}

	// 행동 순서가 다르면 입력 버림
	if (ForActionSequence != ActionSequence)
	{
		return;
	}

	// 반응창이 드는 어빌리티가 아니거나
	// 패링 가능한 공격이면 회피 대상이 아님
	UNUBattleGameplayAbility* Attack = GetActiveBattleAbility();
	if (!Attack || !Attack->IsCurrentlyReactable() || Attack->IsCurrentlyParryable())
	{
		return;
	}

	// 패링과 마찬가지로 소유한 정령까지 같이 회피 처리
	TArray<int32> OwnedIds;
	GetCommandableCombatantIds(Request, OwnedIds);

	for (int32 CombatantId : OwnedIds)
	{
		Attack->RegisterDodgeRequest(CombatantId);
	}
}

void ANUBattleEncounter::OnRep_Phase()
{
	// 페이즈 전환 연출 바인딩 필요
	OnBattlePhaseChanged.Broadcast(Phase, TurnNumber);
}

void ANUBattleEncounter::OnRep_TurnNumber()
{
	OnBattlePhaseChanged.Broadcast(Phase, TurnNumber);
}

void ANUBattleEncounter::OnRep_BattleResult()
{
	// 전투 결과 연출 바인딩
}

// CommandSelection 페이즈로 진입하기 전 초기화 하는 단계
void ANUBattleEncounter::BeginCommandSelection()
{
	if (!HasAuthority())
	{
		return;
	}
	
	++TurnNumber;
	if (GetBattleEventHandler() != nullptr) { GetBattleEventHandler()->BeginTurn(this, TurnNumber); }
	if (GetBattleEventHandler() != nullptr && GetBattleEventHandler()->GetForcedResult(this) != ENUBattleResult::None) { FinishBattle(GetBattleEventHandler()->GetForcedResult(this)); return; }
	
	PendingCommands.Reset();
	ActionQueue.Reset();
	CurrentActionIndex = INDEX_NONE;
	bFleeRequested = false;

	// 교체는 턴당 1회
	// 새 턴이 시작되면 다시 쓸 수 있으므로 명령 선택 단계가 시작할때마다 비움
	NaiaSwappedCombatantIds.Reset();

	// 지난 라운드에 남은 SP 를 전부 쓴 전투원은 행동 뒤 SP 가 안 찼음, 새 명령 전에 채움 (3203 나이아 개화)
	for (const int32 CombatantId : DeferredSPRefillIds)
	{
		RefillSP(CombatantId);
	}
	DeferredSPRefillIds.Reset();

	SetBattlePhase(ENUBattlePhase::CommandSelection);

	// 첫 턴은 스폰 직후라 체력바가 숨겨져 있다. 암전이 풀리는 시점에 다시 켠다
	if (TurnNumber == 1)
	{
		for (const FNUBattleParticipant& Participant : Participants)
		{
			if (ANUBattleAvatar* Avatar = Cast<ANUBattleAvatar>(Participant.AvatarActor))
			{
				Avatar->SetVitalBarHidden(GetBattleEventHandler() != nullptr && GetBattleEventHandler()->ShouldHideVitalBar(this, Participant.Team));
			}
		}
	}

	// 전투가 열릴 때 한 번만 발동하는 패시브 (2833 동기화)
	// 복사해온 패시브가 이번 턴부터 돌아야 해서 아래 턴시작 패시브보다 먼저 와야 함 
	if (TurnNumber == 1)
	{
		for (const FNUBattleParticipant& Participant : Participants)
		{
			if (!IsValid(Participant.ASCOwner))
			{
				continue;
			}

			if (UNUAbilitySystemComponent* ASC = Cast<UNUAbilitySystemComponent>(
					UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Participant.ASCOwner)))
			{
				ASC->ApplyBattleStartPassives();
			}
		}
	}

	// 턴이 열릴 때 발동하는 패시브 (2840 생명의 숨결)
	// 앞의 CheckBattleState 가 도트를 먼저 굴리므로 도트로 깎인 뒤의 체력에 붙음
	for (const FNUBattleParticipant& Participant : Participants)
	{
		if (!IsValid(Participant.ASCOwner))
		{
			continue;
		}

		if (UNUAbilitySystemComponent* ASC = Cast<UNUAbilitySystemComponent>(
				UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Participant.ASCOwner)))
		{
			// 상태의 끝을 먼저 닫음, 명령을 고르기 전에 SP 상한이 바뀌어야 이번 턴 명령에 페널티가 듦 (3104 번아웃)
			ASC->AdvanceJobStates(TurnNumber);
			ASC->ApplyTurnStartPassives();
		}
	}

	// 적 행동만 수행하는 전용 Encounter는 플레이어 명령과 제한 시간을 기다리지 않는다.
	if (!ShouldWaitForPlayerCommands())
	{
		GenerateEnemyCommands();
		BuildActionQueue();
		return;
	}

	// 최대 행동 시간 제한 걸기
	GetWorldTimerManager().SetTimer(
		CommandLimitTimer,
		this,
		&ThisClass::HandleCommandTimeOut,
		MaxCommandSeconds,
		false
		);
}

// 모두 명령을 제출했는지 확인하는 함수
bool ANUBattleEncounter::HasAllRequestedCommands()
{
	for (const FNUBattleParticipant& Participant : Participants)
	{
		if (Participant.Team == ENUBattleTeam::Enemy)
		{
			continue;
		}
		if (!IsAlive(Participant.CombatantId))
		{
			continue;
		}
		
		// 기절한 전투원은 명령을 낼 수 없으므로 기다리지 않음
		// 빼지 않으면 커맨드 제한 시간이 끝날 때까지 턴이 넘어가지 않음
		if (HasActionBlockingStatus(Participant.CombatantId))
		{
			continue;
		}
		
		// 한명이라도 제출하지 않았으면 false
		if (!PendingCommands.Contains(Participant.CombatantId))
		{
			return false;
		}
	}
	
	return true;
}

// 적의 행동을 결정하는 함수
void ANUBattleEncounter::GenerateEnemyCommands()
{
	for (const FNUBattleParticipant& Participant : Participants)
	{
		if (Participant.Team != ENUBattleTeam::Enemy)
		{
			continue;
		}
		if (!IsAlive(Participant.CombatantId))
		{
			continue;
		}

		FNUBattleCommandRequest Command;
		Command.TurnNumber = TurnNumber;
		Command.SourceCombatantId = Participant.CombatantId;
		Command.CommandType = ENUBattleCommandType::Ability;
		
		FNUEnemySelectedAction Action = SelectEnemyAbility(Participant);
		Command.AbilityId = Action.AbilityId;
		Command.SkillId = Action.SkillId;
		Command.bOverrideParryTiming = Action.bOverrideParryTiming;
		Command.ParryChallengeDuration = Action.ChallengeDuration;
		Command.ParrySuccessAnchor = Action.SuccessAnchor;
		Command.ParrySuccessWindow = Action.SuccessWindow;

		// 플레이어 명령은 TrySubmitCommand가 수락 시점에 타겟을 확정하지만
		// 적 명령은 PendingCommands에 직접 넣어서 타겟 확정 부분이 없음
		// 따라서 여기서 타겟 확정 시도
		ENUTargetPolicy Policy = ResolveCommandTargetPolicy(Command);

		TArray<int32> Targets;
		if (!ResolveTargets(Participant, Policy, INDEX_NONE, Targets))
		{
			// 대상을 못 잡는 적만 건너뜀, 자기 대상 스킬을 쓸 적은 계속 명령을 낼 수 있어야 함
			continue;
		}
		Command.SelectedTargetId = Targets[0];

		PendingCommands.Add(Participant.CombatantId, MoveTemp(Command));
	}
}

FNUEnemySelectedAction ANUBattleEncounter::SelectEnemyAbility(const FNUBattleParticipant& Participant)
{
	auto BasicAttackFallback = []()
	{
		FNUEnemySelectedAction Result;
		Result.AbilityId = NU::Ability::Battle::BasicAttack;
		return Result;
	};
	
	const FNUEnemyPattern* Pattern = FindEnemyPattern(Participant.EnemySpec.EnemyId);

	// 패턴이 등록되지 않은 몬스터는 기본공격 처리
	if (!Pattern)
	{
		return BasicAttackFallback();
	}

	UNUAbilitySystemComponent* SourceASC = Cast<UNUAbilitySystemComponent>(
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Participant.ASCOwner));

	if (!IsValid(SourceASC))
	{
		return BasicAttackFallback();
	}

	// 실명이면 스킬을 못 쓰고 기본 공격만 함
	if (IsActiveSkillBlocked(Participant.CombatantId))
	{
		return BasicAttackFallback();
	}

	FNUEnemyPatternContext Context;
	Context.Encounter = this;
	Context.SourceASC = SourceASC;
	Context.SourceTeam = Participant.Team;
	Context.TurnNumber = TurnNumber;
	Context.Pattern = Pattern;
	Context.Runtime = &EnemyPatternRuntimes.FindOrAdd(Participant.CombatantId);

	FNUEnemySelectedAction Action = NUEnemyPattern::SelectAbility(Context);
	if (!Action.IsValid())
	{
		return BasicAttackFallback();
	}

	UE_LOG(LogTemp, Log, TEXT("적 패턴 선택: EnemyId=%s, CombatantId=%d, Turn=%d, Ability=%s, SkillId=%d"),
		*Participant.EnemySpec.EnemyId.ToString(),
		Participant.CombatantId,
		TurnNumber,
		*Action.AbilityId.ToString(),
		Action.SkillId
	);

	return Action;
}

const FNUEnemyPattern* ANUBattleEncounter::FindEnemyPattern(FName EnemyId) const
{
	UGameInstance* GameInstance = GetGameInstance();
	UNUBattleDataSubsystem* BattleDataSubsystem =
		GameInstance
		? GameInstance->GetSubsystem<UNUBattleDataSubsystem>()
		: nullptr;

	return IsValid(BattleDataSubsystem) ? BattleDataSubsystem->FindEnemyPattern(EnemyId) : nullptr;
}

bool ANUBattleEncounter::IsCaptured(int32 CombatantId) const
{
	return PendingCaptures.ContainsByPredicate(
		[CombatantId](const FNUPendingCapture& Entry)
		{
			return Entry.TargetCombatantId == CombatantId;
		});
}

bool ANUBattleEncounter::IsPlayerCombatant(int32 CombatantId) const
{
	const FNUBattleParticipant* Participant = FindParticipant(CombatantId);

	// ASCOwner가 PlayerState면 플레이어, SpiritState면 정령
	return Participant && Cast<ANUPlayerState>(Participant->ASCOwner) != nullptr;
}

ENUCaptureResult ANUBattleEncounter::TryCapture(int32 SourceCombatantId, int32 TargetCombatantId,
	float StoneMagnitude, TArray<bool>& OutShakes)
{
	if (GetBattleEventHandler() != nullptr && !GetBattleEventHandler()->IsCaptureAllowed(this)) { return ENUCaptureResult::NotCapturable; }
	if (!HasAuthority())
	{
		return ENUCaptureResult::InvalidTarget;
	}

	// 중복 검사를 가장 먼저 처리
	if (IsCaptured(TargetCombatantId))
	{
		return ENUCaptureResult::AlreadyCaptured;
	}

	// 유효하지 않은 타겟
	const FNUBattleParticipant* Target = FindParticipant(TargetCombatantId);
	if (!Target || Target->Team != ENUBattleTeam::Enemy || !IsAlive(TargetCombatantId))
	{
		return ENUCaptureResult::InvalidTarget;
	}

	UGameInstance* GameInstance = GetGameInstance();
	const UNUNaiaDataSubsystem* NaiaDataSubsystem =
		GameInstance ? GameInstance->GetSubsystem<UNUNaiaDataSubsystem>() : nullptr;

	const FNUNaiaDefinition* Definition = IsValid(NaiaDataSubsystem)
		? NaiaDataSubsystem->FindDefinition(Target->EnemySpec.EnemyId)
		: nullptr;

	// 포획 불가능한 적
	if (!Definition || !Definition->bCapturable)
	{
		return ENUCaptureResult::NotCapturable;
	}

	// 체력 문턱값은 없다 - 만피여도 던질 수 있고, 체력은 확률에만 영향을 준다
	// 서버 계약도 같다 (가이드 "포획 체력 게이트: 게이트 없음")

	// 시전자의 로스터에 자리가 있는지 검사
	const FNUBattleParticipant* Source = FindParticipant(SourceCombatantId);
	ANUPlayerState* OwnerState = Source ? Cast<ANUPlayerState>(Source->OwningPlayerState) : nullptr;
	UNUNaiaRosterComponent* Roster = IsValid(OwnerState) ? OwnerState->GetNaiaRosterComponent() : nullptr;

	// 이미 포획한 나이아 수가 빈칸보다 크거나 같으면
	// 추가로 포획해도 파티 or 보관함에 등록 불가능함
	if (!IsValid(Roster) || Roster->GetFreeSlotCount() <= PendingCaptures.Num())
	{
		return ENUCaptureResult::RosterFull;
	}

	// ---- 확률 판정 ----
	// 자리 검사까지 통과한 뒤에 굴린다, 못 넣을 나이아를 잡아놓고 버리면 안 됨

	const UNUAbilitySystemComponent* TargetASC = Cast<UNUAbilitySystemComponent>(
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target->ASCOwner));

	const UNUVitalAttributeSet* Vital = IsValid(TargetASC) ? TargetASC->GetVitalAttributeSet() : nullptr;
	if (!IsValid(Vital))
	{
		return ENUCaptureResult::InvalidTarget;
	}

	const int32 AttemptCount = CaptureAttemptsByCombatant.FindRef(TargetCombatantId);

	const float BaseChance = NUCapture::GetBaseChance(Definition->Grade, Definition->EvolutionStage);
	const float HealthBonus = NUCapture::GetHealthBonus(Vital->GetHealth(), Vital->GetMaxHealth());
	const float StatusBonus = TargetASC->GetCaptureBonus();
	const float StoneMultiplier = NUCapture::GetStoneMultiplier(StoneMagnitude);
	const float AttemptBonus = NUCapture::GetAttemptBonus(AttemptCount);

	const float Chance = NUCapture::ResolveChance(
		BaseChance, HealthBonus, StatusBonus, StoneMultiplier, AttemptBonus);

	UE_LOG(LogTemp, Warning,
		TEXT("포획 확률 %.1f%% : 기본 %.2f x HP %.2f x 상태이상 %.2f x 교감석 %.1f x 누적 %.2f (시도 %d회)"),
		Chance * 100.f, BaseChance, HealthBonus, StatusBonus, StoneMultiplier, AttemptBonus, AttemptCount);

	// 흔들림 연출을 사망 처리보다 먼저 시작
	// 성공하면 바로 아래에서 사망 이벤트가 나가고 아바타가 숨겨지는데,
	// 아바타가 연출 중이면 숨기기를 미루도록 SetDeadVisual 이 받음
	const bool bRolledSuccess = NUCapture::RollShakes(Chance, OutShakes);
	if (ANUBattleAvatar* TargetAvatar = Cast<ANUBattleAvatar>(Target->AvatarActor))
	{
		TargetAvatar->MulticastPlayCaptureShakes(NUCapture::CountPassedShakes(OutShakes), bRolledSuccess);
	}

	if (!bRolledSuccess)
	{
		// 실패는 카운트만 올린다, 대상의 상태이상과 체력은 그대로 둠
		CaptureAttemptsByCombatant.Add(TargetCombatantId, AttemptCount + 1);

		UE_LOG(LogTemp, Warning, TEXT("포획 실패 : 흔들림 %d회에서 열림 (다음 시도 %d회차)"),
			OutShakes.Num(), AttemptCount + 1);

		return ENUCaptureResult::ChanceFailed;
	}

	// ---- 여기부터 성공 확정 ----

	// 성공했을 때만 누적을 지운다
	CaptureAttemptsByCombatant.Remove(TargetCombatantId);

	// 잡은 나이아의 레벨을 그대로 가져감
	// 스폰할때 아바타에 저장해둔 레벨값 가져와서 사용, 없다면 테이블에서 SpawnLevel 사용
	int32 CapturedLevel = Definition->SpawnLevel;
	if (const ANUBattleEnemyAvatar* EnemyAvatar = Cast<ANUBattleEnemyAvatar>(Target->AvatarActor))
	{
		CapturedLevel = EnemyAvatar->GetLevel();
	}

	// 로스터에 바로 넣지 않고 예약만, 전투 종료시 로스터 등록
	FNUPendingCapture& Pending = PendingCaptures.AddDefaulted_GetRef();
	Pending.TargetCombatantId = TargetCombatantId;
	Pending.NaiaId = Definition->DataIndex;
	Pending.Level = CapturedLevel;
	Pending.Owner = OwnerState;

	// 전투 판정을 위해 포획한 적은 사망처리
	// 포획과 처치의 구분은 PendingCaptures 사용함
	FGameplayEventData Payload;
	Payload.Instigator = Source ? Source->AvatarActor : nullptr;
	Payload.Target = Target->AvatarActor;

	// 포획한 적에게 직접 사망 Event 전송
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		Target->AvatarActor, 
		NU::Event::Dead, 
		Payload
		);

	UE_LOG(LogTemp, Warning, TEXT("포획 성공 : NaiaId=%d, Level=%d, CombatantId=%d"),
		Definition->DataIndex, CapturedLevel, TargetCombatantId);

	return ENUCaptureResult::Success;
}

void ANUBattleEncounter::RespawnActiveSpiritFieldAvatars()
{
	if (!HasAuthority())
	{
		return;
	}

	TSet<ANUPlayerState*> OwnerStates;
	CollectOwnerStates(OwnerStates);
	
	for (ANUPlayerState* OwnerState : OwnerStates)
	{
		OwnerState->TryEnsureActiveSpiritFieldAvatar();
	}
}

void ANUBattleEncounter::RestoreWaitingNaiaAfterBattle()
{
	if (!HasAuthority())
	{
		return;
	}

	TSet<ANUPlayerState*> OwnerStates;
	CollectOwnerStates(OwnerStates);

	for (ANUPlayerState* OwnerState : OwnerStates)
	{
		const UNUNaiaRosterComponent* Roster = OwnerState->GetNaiaRosterComponent();
		if (!IsValid(Roster))
		{
			continue;
		}

		for (const TObjectPtr<ANUSpiritState>& Naia : Roster->GetParty())
		{
			if (UNUAbilitySystemComponent* ASC = IsValid(Naia) ? Naia->GetNUASC() : nullptr)
			{
				ASC->ClearTurnBasedEffects();	// 남아있는 디버프 처리
				ASC->ReviveIfDead(1.f);	// 사망했다면 부활 처리
			}
		}
	}
}

void ANUBattleEncounter::CollectOwnerStates(TSet<ANUPlayerState*>& OutOwnerStates) const
{
	OutOwnerStates.Empty();
	
	// 파티 플레이 대비, 참가자별 소유 플레이어를 중복 없이 모음
	for (const FNUBattleParticipant& Participant : Participants)
	{
		if (ANUPlayerState* OwnerState = Cast<ANUPlayerState>(Participant.OwningPlayerState))
		{
			OutOwnerStates.Add(OwnerState);
		}
	}
}

int32 ANUBattleEncounter::FindSpiritCombatantId(const APlayerState* OwningPlayerState) const
{
	if (!IsValid(OwningPlayerState))
	{
		return INDEX_NONE;
	}

	for (const FNUBattleParticipant& Participant : Participants)
	{
		if (Participant.Team == ENUBattleTeam::Ally
			&& Participant.OwningPlayerState == OwningPlayerState
			&& Cast<ANUSpiritState>(Participant.ASCOwner))
		{
			return Participant.CombatantId;
		}
	}
	return INDEX_NONE;
}

// 전투 중 나이아 교체
// 아바타는 파괴하지 않고 ASC 소유자와 외형만 갈아끼움, CombatantId 는 자리 번호라 유지됨
ENUNaiaSwapResult ANUBattleEncounter::ApplyNaiaSwap(APlayerState* Requester, int32 ToPartySlot)
{
	if (!HasAuthority())
	{
		return ENUNaiaSwapResult::InvalidRequester;
	}

	// ActionExecution 중엔 허용하지 않음
	// 행동 큐에 이 CombatantId 의 명령이 이미 들어 있고, 진행 중인 어빌리티가 대상으로 잡고 있을 수 있음
	if (Phase != ENUBattlePhase::CommandSelection)
	{
		return ENUNaiaSwapResult::WrongPhase;
	}

	ANUPlayerState* OwnerState = Cast<ANUPlayerState>(Requester);
	UNUNaiaRosterComponent* Roster = IsValid(OwnerState) ? OwnerState->GetNaiaRosterComponent() : nullptr;

	const int32 CombatantId = FindSpiritCombatantId(OwnerState);
	FNUBattleParticipant* Participant = CombatantId != INDEX_NONE ? FindParticipant(CombatantId) : nullptr;

	if (!IsValid(Roster) || !Participant)
	{
		return ENUNaiaSwapResult::InvalidRequester;
	}

	// 교체는 SP를 채우므로 반복하면 행동 없이 SP를 충전할 수 있음
	// 클라도 메뉴를 회색으로 만들지만 위조 요청은 여기서 막아야 함
	if (NaiaSwappedCombatantIds.Contains(CombatantId))
	{
		return ENUNaiaSwapResult::AlreadySwappedThisTurn;
	}

	// 0번은 이미 활성이라 교체할 것이 없음
	const TArray<TObjectPtr<ANUSpiritState>>& Party = Roster->GetParty();
	if (ToPartySlot <= 0 || !Party.IsValidIndex(ToPartySlot))
	{
		return ENUNaiaSwapResult::InvalidSlot;
	}

	ANUSpiritState* NewNaia = Party[ToPartySlot].Get();
	ANUSpiritState* OldNaia = Cast<ANUSpiritState>(Participant->ASCOwner);
	if (!IsValid(NewNaia) || NewNaia == OldNaia)
	{
		return ENUNaiaSwapResult::InvalidSlot;
	}

	// 죽은 나이아를 넣으면 참가자가 즉시 사망 상태로 들어오므로 막음
	if (UNUAbilitySystemComponent* NewASC = NewNaia->GetNUASC())
	{
		if (NewASC->GetTagCount(NU::State::Dead) > 0)
		{
			return ENUNaiaSwapResult::TargetDead;
		}
	}

	// 교체한 나이아가 이번 턴에 냈던 명령을 취소함
	// RefundReservedSP 가 PendingCommands 를 읽으므로 환불이 제거보다 먼저여야 함
	RefundReservedSP(CombatantId);
	PendingCommands.Remove(CombatantId);

	ANUBattleAvatar* Avatar = Cast<ANUBattleAvatar>(Participant->AvatarActor);

	// 나가는 나이아 정리
	// BattleEncounter 를 비워야 나중에 필드 아바타를 다시 만들 수 있음
	// EnsureFieldAvatar 가 IsValid(GetBattleEncounter()) 면 물러나므로, 남겨두면 영구히 안 생김
	if (IsValid(OldNaia))
	{
		OldNaia->SetCurrentAvatar(nullptr);
		OldNaia->SetBattleEncounter(nullptr);
	}

	// 아바타를 새로운 나이아로 교체
	if (IsValid(Avatar))
	{
		Avatar->SetAbilitySystemOwner(NewNaia);
		Avatar->SetNaiaId(NewNaia->GetSpiritId());
		Avatar->ResetPresentationState();
	}

	// 들어오는 나이아 연결
	NewNaia->SetCurrentAvatar(Avatar);
	NewNaia->SetBattleEncounter(this);

	Participant->ASCOwner = NewNaia;

	// 파티 0번과 자리를 맞바꿈, 액터를 그대로 두므로 GAS 상태가 유지됨
	Roster->SwapPartyOrderForBattle(0, ToPartySlot);

	// 대기중인 나이아는 턴 종료시 SP 회복의 영향을 받지 않으므로
	// 교체할때 채워주는 방식으로 처리함
	RefillSP(CombatantId);

	// 교체했다면 교체한 플레이어 ID 기록, 턴당 1번만 바꿀수 있게 하기 위함
	NaiaSwappedCombatantIds.Add(CombatantId);

	UNUSFXSubsystem::Play(this, NU::SFX::Battle::NaiaSwap);

	ForceNetUpdate();

	UE_LOG(LogTemp, Warning, TEXT("나이아 교체 : CombatantId=%d, 파티 0 <-> %d, NaiaId=%d"),
		CombatantId, ToPartySlot, NewNaia->GetSpiritId());

	return ENUNaiaSwapResult::Success;
}

// 속도를 이용해서 예약된 명령들의 실행순서를 정하는 함수
void ANUBattleEncounter::BuildActionQueue()
{
	ActionQueue.Reset();

	for (const TPair<int32, FNUBattleCommandRequest>& Pair : PendingCommands)
	{
		FNUResolvedAction ResolvedAction;
		ResolvedAction.Command = Pair.Value;
		ResolvedAction.Priority = ResolveActionPriority(Pair.Value);
		ResolvedAction.Speed = ResolveActionSpeed(Pair.Value);
		if (GetBattleEventHandler() != nullptr)
		{
			const FNUBattleParticipant* Participant = FindParticipant(Pair.Value.SourceCombatantId);
			ResolvedAction.Speed = GetBattleEventHandler()->AdjustSpeed(this, Participant != nullptr && Participant->Team == ENUBattleTeam::Enemy, ResolvedAction.Speed);
		}
		ResolvedAction.bForceLast = HasForceLastAction(Pair.Key);

		// 선제공격은 첫 턴에만 켜진다, 묻는 것이 곧 소모라 전투원마다 한 번만 도는 이 자리에서 물음
		// 첫 턴에 행동을 못 낸 전투원은 그냥 못 쓰고 끝남
		ResolvedAction.bForceFirst = TurnNumber == 1 && ConsumeFirstStrike(Pair.Key);

		ActionQueue.Add(MoveTemp(ResolvedAction));
	}

	SortActionQueue();

	TArray<FString> OrderText;
	OrderText.Reserve(ActionQueue.Num());

	for (const FNUResolvedAction& Action : ActionQueue)
	{
		const TCHAR* Mark = TEXT("");
		if (Action.bForceFirst)
		{
			Mark = TEXT("(선제)");
		}
		else if (Action.bForceLast)
		{
			Mark = TEXT("(둔화)");
		}

		OrderText.Add(FString::Printf(TEXT("%d%s"), Action.Command.SourceCombatantId, Mark));
	}

	// 순서가 왜 이렇게 났는지는 이 줄로만 볼 수 있다, 둔화·선제공격·우선도·속도가 한 줄에 다 걸린다
	UE_LOG(LogTemp, Warning, TEXT("행동 순서 : %s"), *FString::Join(OrderText, TEXT(" > ")));

	CurrentActionIndex = INDEX_NONE;
	SetBattlePhase(ENUBattlePhase::ActionExecution);

	ExecuteNextAction();
}

void ANUBattleEncounter::SortActionQueue()
{
	// 판정 순서
	// 1. 마지막 강제(둔화)  2. 맨 앞 강제(선제공격)  3. 행동 우선도  4. 속도   5. CombatantId
	ActionQueue.Sort([this](const FNUResolvedAction& A, const FNUResolvedAction& B)
	{
		if (A.bForceLast != B.bForceLast)
		{
			// true면 a가 b보다 앞에옴
			// 따라서 a가 마지막 강제인 상태가 아니라면 앞에옴
			return !A.bForceLast;
		}
		if (A.bForceFirst != B.bForceFirst)
		{
			// 둘이 같이 걸리면 둔화가 이김, 위에서 먼저 갈리기 때문
			// 선제공격은 첫 턴에만 켜지는데 그 시점엔 아직 아무도 둔화를 걸지 못해 실제로 만나지는 않음
			// 도주(우선도 200)보다도 앞에 판정, 가장 먼저 행동한다를 그대로 옮긴 것
			return A.bForceFirst;
		}
		if (A.Priority != B.Priority)
		{
			return A.Priority > B.Priority;
		}
		if (!FMath::IsNearlyEqual(A.Speed, B.Speed))
		{
			return A.Speed > B.Speed;
		}
		return A.Command.SourceCombatantId < B.Command.SourceCombatantId;
	});
}

// 다음 행동을 처리하는 함수
// Index가 유효할때까지 반복함
void ANUBattleEncounter::ExecuteNextAction()
{
	// 전투가 이미 끝났다면 진행 X (도주 시 전투가 먼저 종료됨)
	if (Phase != ENUBattlePhase::ActionExecution)
	{
		return;
	}
	
	if (bFleeRequested)
	{
		FinishBattle(ENUBattleResult::Fled);
		return;
	}

	// 한쪽 진영이 전멸했으면 남은 행동을 버리고 즉시 판정함
	if (TryFinishByElimination())
	{
		return;
	}

	// 직전에 끝난 행동의 주인에게 SP를 돌려줌
	// 첫 진입은 CurrentActionIndex가 INDEX_NONE이라 그냥 지나감
	RefillSPAfterAction(CurrentActionIndex);

	++CurrentActionIndex;

	// 모든 행동이 끝났다면 판정하고 넘어감
	if (!ActionQueue.IsValidIndex(CurrentActionIndex))
	{
		CheckBattleState();
		return;
	}
	
	// 환각이 걸리면 아래에서 이 명령을 기본 공격으로 바꿔치기 해야 함
	FNUBattleCommandRequest Command = ActionQueue[CurrentActionIndex].Command;
	
	// 능력 발동 차례가 된 전투원이 사망했거나 인정했다면 다음 행동으로 넘어감
	if (!IsAlive(Command.SourceCombatantId) || IsConceded(Command.SourceCombatantId))
	{
		ExecuteNextAction();
		return;
	}
	
	// 기절이면 행동 불가, 마비면 확률로 실패함
	// 명령을 낸 뒤에 걸린 상태이상도 여기서 걸러짐
	// 속성 봉인은 적 패턴이 고른 스킬도 걸러야 해서 여기까지 봐야 함
	if (HasActionBlockingStatus(Command.SourceCombatantId)
		|| IsSkillBlocked(Command.SourceCombatantId, Command.SkillId)
		|| RollActionFailure(Command.SourceCombatantId))
	{
		ShowActionFailPopup(Command.SourceCombatantId);
		
		ExecuteNextAction();
		return;
	}
	
	// 환각은 마비를 통과한 뒤에 판정함, 원래 행동을 버리고 자기 진영을 때림
	// 대상을 못 잡는 경우만 행동 취소로 떨어짐
	if (RollHallucination(Command.SourceCombatantId)
		&& !ApplyHallucinationToCurrentAction(Command))
	{
		ShowActionFailPopup(Command.SourceCombatantId);
		
		ExecuteNextAction();
		return;
	}
	
	// 몇번째 행동인지 복제되는 값
	++ActionSequence;
	
	switch (Command.CommandType)
	{
	case ENUBattleCommandType::Ability:
		ExecuteAbilityCommand(Command);
		break;
		
	case ENUBattleCommandType::Item:
		ExecuteItemCommand(Command);
		break;
		
	default:
		ExecuteNextAction();
		break;
	}
}

void ANUBattleEncounter::ExecuteAbilityCommand(const FNUBattleCommandRequest& Command)
{
	const FNUBattleParticipant* Source = FindParticipant(Command.SourceCombatantId);
	
	// 어빌리티 발동할 전투원이 유효하지 않다면 넘김
	if (!Source || !IsAlive(Source->CombatantId))
	{
		ScheduleNextAction();
		return;
	}
	
	UNUAbilitySystemComponent* SourceASC = Cast<UNUAbilitySystemComponent>(
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Source->ASCOwner)
		);
	
	if (!IsValid(SourceASC))
	{
		ScheduleNextAction();
		return;
	}
	
	// 이 행동이 무엇인지 한번만 해결하고 아래로 넘김
	FNUBattleActionContext Context;
	ResolveActionContext(Command, Context);
	if (GetBattleEventHandler() != nullptr) { GetBattleEventHandler()->AdjustActionContext(this, Command, Context); }

	if (Context.bSpendAllSP)
	{
		UE_LOG(LogTemp, Log, TEXT("[직업] 스킬 %d : 남은 SP %d 를 모두 써 계수 %.0f%%"),
			Command.SkillId, Command.SPSpend, Context.DamageCoefficient * 100.f);
	}
	
	// 실행기가 정해진 행동(스킬)은 클래스로 찾고 없으면 그때 부여함
	// 그 외는 기존대로 AbilityId 태그로 찾음
	const FGameplayAbilitySpecHandle AbilityHandle = Context.ExecutorClass
		? SourceASC->FindOrGiveExecutor(Context.ExecutorClass)
		: SourceASC->FindBattleAbilityHandle(Command.AbilityId);

	if (!AbilityHandle.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("보유하지 않은 전투 Ability입니다 : %s (SkillId=%d)"),
			*Command.AbilityId.ToString(), Command.SkillId);
		ScheduleNextAction();
		return;
	}
	
	TArray<int32> TargetIds;
	if (!ResolveTargets(*Source, Context.TargetPolicy, Command.SelectedTargetId, TargetIds))
	{
		UE_LOG(LogTemp, Warning, TEXT("맞는 타겟을 찾을 수 없습니다!"));
		ScheduleNextAction();
		return;
	}

	const FNUBattleParticipant* Target = FindParticipant(TargetIds[0]);
	if (!Target || !IsValid(Target->AvatarActor))
	{
		ScheduleNextAction();
		return;
	}


	FGameplayEventData EventData;
	EventData.EventTag = Command.AbilityId;
	EventData.Instigator = Source->AvatarActor;
	EventData.Target = Target->AvatarActor;

	// 확정된 타수를 어빌리티로 넘김
	// 투입 SP를 넘겨 어빌리티가 다시 나누게 할 경우 중간에 소모량이 달라지면 어긋남
	EventData.EventMagnitude = static_cast<float>(Command.HitCount);

	// 어빌리티 발동 전에 배너 생성, 발동 뒤에는 스펙 상태가 바뀔 수 있음
	FNUBattleActionBanner Banner = BuildActionBanner(Command, Context, SourceASC, AbilityHandle);

	// 실행기가 데이터를 읽을 수 있도록 어빌리티 발동 직전에 정보를 전달함
	// 위쪽 조기 반환보다 앞에서 실으면 다음 행동으로 새므로 여기여야 함
	// CDO 값으로 도는 행동(기본공격/아이템/도주)은 피해 관련으로 실을게 없음
	// bReactable 이 켜져있다면, 적 기본공격 같은 경우엔 필요함
	if (Context.bValid || Context.bReactable)
	{
		SourceASC->SetPendingActionContext(Context);
	}

	if (!TriggerActionAbility(SourceASC, AbilityHandle, EventData))
	{
		return;
	}

	// 실제로 실행 중인 행동만 띄움
	// CommitAbility 실패로 즉시 끝났으면 모션도 안 나가므로 배너도 띄우지 않음
	if (IsActionAbilityRunning())
	{
		MulticastShowActionBanner(Banner);
	}

	UE_LOG(LogTemp, Warning, TEXT("%s 수행 대상 = %d 타겟 = %d"),
		*EventData.EventTag.ToString(),
		Source->CombatantId,
		Target->CombatantId
		);
}

FNUBattleActionBanner ANUBattleEncounter::BuildActionBanner(const FNUBattleCommandRequest& Command,
	const FNUBattleActionContext& Context, UNUAbilitySystemComponent* SourceASC,
	const FGameplayAbilitySpecHandle& AbilityHandle)
{
	FNUBattleActionBanner Banner;

	// 아이템은 아이템 이름, 스킬이면 KoreanName, 그 외엔 어빌리티 CDO의 이름 적용
	Banner.DisplayName = Context.DisplayName;
	
	// 예약 시점에 확정된 타수
	Banner.HitCount = Command.HitCount;

	// 레벨은 어빌리티스펙 고유 값이라 여기서 읽음
	if (IsValid(SourceASC))
	{
		if (const FGameplayAbilitySpec* Spec = SourceASC->FindAbilitySpecFromHandle(AbilityHandle))
		{
			Banner.Level = Spec->Level;
		}
	}
	
	return Banner;
}

void ANUBattleEncounter::MulticastShowActionBanner_Implementation(const FNUBattleActionBanner& Banner)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (ANUPlayerController* PC = Cast<ANUPlayerController>(
		UGameplayStatics::GetPlayerController(this, 0)))
	{
		PC->ShowActionBanner(Banner);
	}
}

void ANUBattleEncounter::ExecuteItemCommand(const FNUBattleCommandRequest& Command)
{
	const FNUBattleParticipant* Source = FindParticipant(Command.SourceCombatantId);
	if (!Source || !IsAlive(Source->CombatantId))
	{
		ScheduleNextAction();
		return;
	}
	
	ANUPlayerState* OwnerState = Cast<ANUPlayerState>(Source->OwningPlayerState);
	UNUInventoryComponent* Inventory = OwnerState ? OwnerState->GetInventoryComponent() : nullptr;
	const FNUItemInstance* Instance = Inventory ? Inventory->FindInstance(Command.InstanceId) : nullptr;
	const FNUItemDefRow* Def = Instance ? Inventory->FindItemDef(Instance->ItemRowName) : nullptr;
	
	if (!Def)
	{
		UE_LOG(LogTemp, Warning, TEXT("아이템 정의를 찾을 수 없습니다!"));
		ScheduleNextAction();
		return;
	}
	
	// Def는 인벤토리가 아니라 DataTable 행을 가리키므로 아이템을 소모해도 그대로 유효함
	// 반면 Instance는 복제되는 인벤토리 배열의 원소라 RemoveItem 뒤에는 쓰면 안 됨

	// 교감 아이템은 별도 포획 판정으로 가므로 UseEffect가 없어도 됨
	const bool bIsCommuneItem = Def->ItemType == ENUItemType::Commune;
	if (!bIsCommuneItem && !Def->UseEffect)
	{
		// 교감 아이템이 아닌데 UseEffect가 없다면 무효
		UE_LOG(LogTemp, Warning, TEXT("아이템 %s에 UseEffect가 지정되지 않았습니다"),
			*Instance->ItemRowName.ToString());
		ScheduleNextAction();
		return;
	}

	FNUBattleActionContext Context;
	ResolveActionContext(Command, Context);
	
	TArray<int32> TargetIds;
	if (!ResolveTargets(*Source, Context.TargetPolicy, Command.SelectedTargetId, TargetIds))
	{
		UE_LOG(LogTemp, Warning, TEXT("맞는 타겟을 찾을 수 없습니다!"));
		ScheduleNextAction();
		return;
	}
	
	// 타겟 전투원 찾기
	const FNUBattleParticipant* Target = FindParticipant(TargetIds[0]);
	if (!Target || !IsValid(Target->AvatarActor))
	{
		ScheduleNextAction();
		return;
	}
	
	// 사용한 사람 ASC 찾기
	UNUAbilitySystemComponent* SourceASC = Cast<UNUAbilitySystemComponent>(
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Source->ASCOwner));
	if (!IsValid(SourceASC))
	{
		ScheduleNextAction();
		return;
	}
	
	// 사용한 사람이 Item 사용 Ability를 가지고 있는지 확인
	FGameplayAbilitySpecHandle AbilityHandle = SourceASC->FindBattleAbilityHandle(NU::Ability::Battle::UseItem);
	if (!AbilityHandle.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("아이템 사용 Ability를 보유하지 않았습니다!"));
		ScheduleNextAction();
		return;
	}
	
	// GameplayEventData 보낼 준비
	FGameplayEventData EventData;
	EventData.EventTag = NU::Ability::Battle::UseItem;
	EventData.Instigator = Source->AvatarActor;
	EventData.Target = Target->AvatarActor;
	EventData.OptionalObject = Def->UseEffect.Get();
	EventData.EventMagnitude = Def->Effect1_Value;

	// 어빌리티가 아이템 종류를 알아야 포획 처리가 가능함
	// OptionalObject는 GE 클래스가, EventMagnitude는 Effect1_Value가 쓰고 있어 태그로 넘김
	if (bIsCommuneItem)
	{
		EventData.InstigatorTags.AddTag(NU::Item::Commune);
	}

	if (!TriggerActionAbility(SourceASC, AbilityHandle, EventData))
	{
		return;
	}

	// UseItem 어빌리티는 발동했다면 배너 띄우기
	if (IsActionAbilityRunning())
	{
		MulticastShowActionBanner(BuildActionBanner(Command, Context, SourceASC, AbilityHandle));
	}

	UE_LOG(LogTemp, Warning, TEXT("%s 수행 대상 = %d 타겟 = %d"),
		*EventData.EventTag.ToString(),
		Source->CombatantId,
		Target->CombatantId
		);

	// 교감석은 포획 연출이 소리를 냄
	if (!bIsCommuneItem)
	{
		UNUSFXSubsystem::Play(this, NU::SFX::Item::Use);
	}

	// 발동에 성공했을때 소모
	Inventory->RemoveItem(Command.InstanceId, 1);
}

bool ANUBattleEncounter::TryFinishByElimination()
{
	if (GetBattleEventHandler() != nullptr && GetBattleEventHandler()->GetForcedResult(this) != ENUBattleResult::None) { FinishBattle(GetBattleEventHandler()->GetForcedResult(this)); return true; }

	// 인정 임계치에 닿은 적만 남았으면 쓰러뜨리지 않고 인정 연출 뒤 승리
	if (TryFinishByConcession())
	{
		return true;
	}

	// 적이 남아있는지 확인
	if (!HasLivingTeam(ENUBattleTeam::Enemy))
	{
		FinishBattle(ENUBattleResult::Victory);
		return true;
	}

	// 플레이어가 남아있는지 확인
	// 나이아가 남아 있어도 플레이어가 전멸하면 패배
	if (!HasLivingPlayer())
	{
		FinishBattle(ENUBattleResult::Defeat);
		return true;
	}

	return false;
}

bool ANUBattleEncounter::TryFinishByConcession()
{
	// 연출 중에는 다음 행동 · 턴으로 넘어가지 않음
	if (bConcessionPending)
	{
		return true;
	}

	TArray<int32> ConcededIds;
	for (const FNUBattleParticipant& Participant : Participants)
	{
		if (Participant.Team != ENUBattleTeam::Enemy || !IsAlive(Participant.CombatantId))
		{
			continue;
		}

		// 아직 서 있는 적이 있으면 전투를 이어감, 먼저 인정한 적은 ExecuteNextAction 이 행동을 건너뜀
		if (!IsConceded(Participant.CombatantId))
		{
			return false;
		}

		ConcededIds.Add(Participant.CombatantId);
	}

	if (ConcededIds.IsEmpty())
	{
		return false;
	}

	bConcessionPending = true;

	float AnimationSeconds = 0.f;
	for (const int32 CombatantId : ConcededIds)
	{
		const FNUBattleParticipant* Participant = FindParticipant(CombatantId);
		const FNUEnemyPattern* Pattern = Participant ? FindParticipantPattern(*Participant) : nullptr;
		ANUBattleAvatar* Avatar = Participant ? Cast<ANUBattleAvatar>(Participant->AvatarActor) : nullptr;
		if (!Pattern || !IsValid(Avatar))
		{
			continue;
		}

		// 전투 시작 때 불러 둔 것 (PreloadConcessionAnimations)
		UPaperZDAnimSequence* Once = Pattern->ConcedeAnimation.LoadSynchronous();
		UPaperZDAnimSequence* Loop = Pattern->ConcedeLoopAnimation.LoadSynchronous();
		Avatar->MulticastPlayConcession(Once, Loop);

		if (IsValid(Once))
		{
			AnimationSeconds = FMath::Max(AnimationSeconds, Once->GetTotalDuration());
		}

		UE_LOG(LogTemp, Log, TEXT("적 인정: EnemyId=%s, CombatantId=%d, 연출 %.2f초"),
			*Participant->EnemySpec.EnemyId.ToString(), CombatantId, AnimationSeconds);
	}

	GetWorldTimerManager().SetTimer(
		ConcessionTimer,
		this,
		&ThisClass::FinishByConcession,
		FMath::Max(AnimationSeconds + ConcessionHoldSeconds, 0.01f),
		false
		);

	return true;
}

void ANUBattleEncounter::FinishByConcession()
{
	FinishBattle(ENUBattleResult::Victory);
}

bool ANUBattleEncounter::IsConceded(int32 CombatantId) const
{
	const FNUBattleParticipant* Participant = FindParticipant(CombatantId);
	const UAbilitySystemComponent* ASC = Participant
		? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Participant->ASCOwner)
		: nullptr;

	const float Floor = GetConcessionHealthFloor(ASC);

	return Floor > 0.f
		&& ASC->GetNumericAttribute(UNUVitalAttributeSet::GetHealthAttribute()) <= Floor + KINDA_SMALL_NUMBER;
}

float ANUBattleEncounter::GetConcessionHealthFloor(const UAbilitySystemComponent* ASC) const
{
	if (!IsValid(ASC))
	{
		return 0.f;
	}

	for (const FNUBattleParticipant& Participant : Participants)
	{
		if (UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Participant.ASCOwner) != ASC)
		{
			continue;
		}

		const FNUEnemyPattern* Pattern = FindParticipantPattern(Participant);
		if (!Pattern || Pattern->ConcedeHealthRatio <= 0.f)
		{
			return 0.f;
		}

		return ASC->GetNumericAttribute(UNUVitalAttributeSet::GetMaxHealthAttribute()) * Pattern->ConcedeHealthRatio;
	}

	return 0.f;
}

const FNUEnemyPattern* ANUBattleEncounter::FindParticipantPattern(const FNUBattleParticipant& Participant) const
{
	return Participant.Team == ENUBattleTeam::Enemy && Participant.EnemySpec.IsValid()
		? FindEnemyPattern(Participant.EnemySpec.EnemyId)
		: nullptr;
}

void ANUBattleEncounter::PreloadConcessionAnimations()
{
	PreloadedConcessionAnimations.Reset();

	for (const FNUBattleParticipant& Participant : Participants)
	{
		const FNUEnemyPattern* Pattern = FindParticipantPattern(Participant);
		if (!Pattern || Pattern->ConcedeHealthRatio <= 0.f)
		{
			continue;
		}

		if (UPaperZDAnimSequence* Once = Pattern->ConcedeAnimation.LoadSynchronous())
		{
			PreloadedConcessionAnimations.AddUnique(Once);
		}

		if (UPaperZDAnimSequence* Loop = Pattern->ConcedeLoopAnimation.LoadSynchronous())
		{
			PreloadedConcessionAnimations.AddUnique(Loop);
		}
	}
}

void ANUBattleEncounter::PreloadActionVFX()
{
	for (const FNUBattleParticipant& Participant : Participants)
	{
		ANUBattleAvatar* Avatar = Cast<ANUBattleAvatar>(Participant.AvatarActor);
		const UAbilitySystemComponent* ASC =
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Participant.ASCOwner);
		if (!IsValid(Avatar) || !IsValid(ASC))
		{
			continue;
		}

		// 스킬도 실행 어빌리티의 기본값 위에 스킬 표 행을 덮으므로 어빌리티부터 모음
		TArray<const UNUBattleGameplayAbility*> Abilities;
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			if (const UNUBattleGameplayAbility* Ability = Cast<UNUBattleGameplayAbility>(Spec.Ability))
			{
				Abilities.Add(Ability);
			}
		}

		// 쓸 수 있는 스킬, 캐릭터는 성장 노드 보상 · 나이아는 로스터 · 적은 패턴 규칙
		TArray<int32> SkillIds;
		if (const ANUPlayerState* PlayerState = Cast<ANUPlayerState>(Participant.ASCOwner))
		{
			if (const UNUPlayerSkillComponent* Skills = PlayerState->GetPlayerSkillComponent())
			{
				SkillIds = Skills->GetOwnedActiveSkillIds();
			}
		}
		else if (const ANUSpiritState* Naia = Cast<ANUSpiritState>(Participant.ASCOwner))
		{
			SkillIds = Naia->GetActiveSkillIds();
		}
		else if (const FNUEnemyPattern* Pattern = FindParticipantPattern(Participant))
		{
			for (const FNUEnemyPatternRule& Rule : Pattern->Rules)
			{
				SkillIds.AddUnique(Rule.SkillId);
			}
		}

		Avatar->PreloadActionVFX(SkillIds, Abilities);
	}
}

void ANUBattleEncounter::CheckBattleState()
{
	// 행동으로 이미 결판이 났으면 도트를 적용하지 않고 끝냄
	// 마지막 적을 처치한 턴에 화상으로 같이 죽으면 승리가 패배로 뒤집힘
	if (TryFinishByElimination())
	{
		return;
	}

	// 한 턴이 끝났으니 상태이상 지속 턴 감소와 턴 종료 도트 처리
	// 이 사이에 켜지는 상태(문양이 0 이 되는 지맥 붕괴)는 다음 턴부터 시작하도록 표시함
	bAdvancingRound = true;

	for (FNUBattleParticipant& Participant : Participants)
	{
		if (!IsValid(Participant.ASCOwner))
		{
			continue;
		}

		if (UNUAbilitySystemComponent* ASC = Cast<UNUAbilitySystemComponent>(
				UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Participant.ASCOwner)))
		{
			ASC->AdvanceTurnBasedEffects();
		}
	}

	bAdvancingRound = false;

	// 도트 피해로 전투원이 죽을 수 있으므로 다시 판정해야 함
	// 빠뜨리면 전멸한 진영을 두고 다음 턴 명령 선택이 열려 전투가 끝나지 않음
	if (TryFinishByElimination())
	{
		return;
	}

	// 전투가 끝나지 않았다면 행동 선택부터 다시 반복
	BeginCommandSelection();
}

void ANUBattleEncounter::FinishBattle(ENUBattleResult Result)
{
	BattleResult = Result;
	SetBattlePhase(ENUBattlePhase::Ending);
	
	GetWorldTimerManager().ClearTimer(CommandLimitTimer);

	// 흔들림 때문에 미뤄둔 다음 행동이 남아 있으면 전투가 끝난 뒤에 터진다
	GetWorldTimerManager().ClearTimer(ActionDelayHandle);

	UnbindAbilityEnded();

	EnemyPatternRuntimes.Reset();

	// PendingCaptures는 인카운터 초기화시에 리셋
	// 로스터 등록과 보상 계산(NotifyEncounterCompleted)이 이 뒤에 돌기 때문에 비우면 안됨

	// 포획 성공은 흔들림 연출 도중에 전멸 판정이 들어온다
	// 판정·명단 처리는 위에서 이미 끝났으므로 화면 전환만 연출 길이만큼 미룬다
	// 사망 이벤트를 미루면 그 사이 다른 전투원이 잡힌 적을 때리므로 화면만 미루는 것이 맞음
	const float ShakeRemaining = GetCaptureShakeRemainingSeconds();
	if (ShakeRemaining > KINDA_SMALL_NUMBER)
	{
		GetWorldTimerManager().SetTimer(
			TransitionTimer,
			this,
			&ThisClass::ShowBattleResultOrFinish,
			ShakeRemaining,
			false
			);

		return;
	}

	ShowBattleResultOrFinish();
}

void ANUBattleEncounter::CollectLocalRewards(int64& OutExperience, int64& OutGold) const
{
	OutExperience = 0;
	OutGold = 0;
	if (GetBattleEventHandler() != nullptr && !GetBattleEventHandler()->ShouldGrantStandardRewards(this)) { return; }

	// 도주·패배는 보상이 없음
	if (BattleResult != ENUBattleResult::Victory)
	{
		return;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	const UNUNaiaDataSubsystem* NaiaData = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UNUNaiaDataSubsystem>()
		: nullptr;
	if (!IsValid(NaiaData))
	{
		return;
	}

	for (const FNUBattleParticipant& Participant : Participants)
	{
		if (Participant.Team != ENUBattleTeam::Enemy || !Participant.EnemySpec.IsValid())
		{
			continue;
		}

		// 포획한 적은 처치 보상에서 제외
		if (IsCaptured(Participant.CombatantId))
		{
			continue;
		}

		// 살아 있는 적은 안 잡은 것, 승리했어도 도주 등으로 남아 있을 수 있음
		// 인정한 적(대련 보스)은 쓰러뜨리지 않았을 뿐 이긴 것이라 처치와 같이 셈
		if (IsAlive(Participant.CombatantId) && !IsConceded(Participant.CombatantId))
		{
			continue;
		}

		const FNUNaiaDefinition* Definition = NaiaData->FindDefinition(Participant.EnemySpec.EnemyId);
		if (!Definition)
		{
			// 표에 없는 적, 로그만 남김
			UE_LOG(LogTemp, Warning, TEXT("처치 보상을 못 구함, 나이아 표에 %s 가 없습니다"),
				*Participant.EnemySpec.EnemyId.ToString());

			continue;
		}

		OutExperience += Definition->Experience;
		OutGold += Definition->Gold;
	}
}

bool ANUBattleEncounter::ApplyLocalRewards(int64 Experience, int64 Gold, FNUExperienceGrowth& OutGrowth)
{
	// 지급은 전투당 한 번
	if (bRewardsGranted || (Experience <= 0 && Gold <= 0))
	{
		return false;
	}

	// 첫 아군이 나이아일 수 있으므로 ASCOwner 가 아니라 소유 플레이어로 찾음
	ANUPlayerState* PlayerState = nullptr;
	for (const FNUBattleParticipant& Participant : Participants)
	{
		if (Participant.Team != ENUBattleTeam::Ally)
		{
			continue;
		}

		PlayerState = Cast<ANUPlayerState>(Participant.OwningPlayerState);
		if (IsValid(PlayerState))
		{
			break;
		}
	}

	UNUProgressionComponent* Progression = IsValid(PlayerState)
		? PlayerState->GetProgressionComponent()
		: nullptr;

	// 외부 성장 세션이 붙어 있으면 경험치·골드는 서버가 확정함, 클라가 손대면 어긋남
	// 퀘스트 보상(UNUQuestComponent::ApplyRewards)과 같은 판정을 씀
	if (!IsValid(Progression) || !Progression->IsUsingLocalProgressionBackend())
	{
		return false;
	}

	bRewardsGranted = true;

	// 결과창의 경험치 바와 LEVEL UP 표시는 서버가 주는 성장 변화와 같은 모양을 씀
	// 로컬 전투는 그 값을 여기서 직접 만들어 넣음
	OutGrowth.OldLevel = Progression->GetLevel();
	OutGrowth.OldTotalExperience = Progression->GetTotalExperience();

	if (Experience > 0 && !Progression->ApplyLocalExperienceReward(Experience))
	{
		UE_LOG(LogTemp, Warning, TEXT("로컬 처치 보상 경험치 %lld 적용 실패"), Experience);
	}

	if (Gold > 0)
	{
		if (UNUInventoryComponent* Inventory = PlayerState->GetInventoryComponent())
		{
			Inventory->AddGold(Gold);
		}
	}

	OutGrowth.NewLevel = Progression->GetLevel();
	OutGrowth.NewTotalExperience = Progression->GetTotalExperience();
	OutGrowth.ExperienceIntoLevel = Progression->GetExperienceIntoLevel();
	OutGrowth.RequiredExperienceToNext = Progression->GetRequiredExperienceToNext();
	OutGrowth.MaxLevel = FNULevelProgression::Get().GetMaxLevel();

	UE_LOG(LogTemp, Warning, TEXT("로컬 처치 보상 경험치=%lld 골드=%lld 레벨 %d -> %d"),
		Experience, Gold, OutGrowth.OldLevel, OutGrowth.NewLevel);

	return true;
}

void ANUBattleEncounter::ShowBattleResultOrFinish()
{
	// 패배는 결과창이 없어 여기서 소리를 냄 (승리음은 결과창이 냄)
	if (BattleResult == ENUBattleResult::Defeat)
	{
		UNUSFXSubsystem::Play(this, NU::SFX::Battle::Defeat);
	}

	// 넷 전투와 같은 규칙, 승리하거나 포획하면 띄움
	// 포획은 승패와 무관하게 확정되므로 져도 잡은 것은 알려야 함
	if (BattleResult != ENUBattleResult::Victory && PendingCaptures.IsEmpty())
	{
		BeginFinishTransition();
		return;
	}

	// 이 프로젝트는 클라가 Standalone 이라 0번 컨트롤러를 사용함
	// 배너 멀티캐스트(MulticastShowActionBanner)와 같은 방식
	ANUPlayerController* PC = Cast<ANUPlayerController>(
		UGameplayStatics::GetPlayerController(this, 0));
	if (!IsValid(PC))
	{
		BeginFinishTransition();
		return;
	}

	FNUBattleResultView ResultView;
	ResultView.Result = BattleResult;

	// 나이아 표의 처치 보상을 합산해 반영함, 서버가 권위인 상황이면 반영되지 않고 표시만 남는다
	CollectLocalRewards(ResultView.Experience, ResultView.Gold);

	FNUExperienceGrowth Growth;
	const bool bApplied = ApplyLocalRewards(ResultView.Experience, ResultView.Gold, Growth);

	NUBattleResult::BuildMembers(
		this, 
		bApplied ? &Growth : nullptr, 
		ResultView.Members
		);

	// PendingCaptures 는 결과 처리가 끝날 때까지 비우지 않으므로 여기서 그대로 읽으면 됨
	TArray<int32> CapturedNaiaIds;
	for (const FNUPendingCapture& Pending : PendingCaptures)
	{
		CapturedNaiaIds.Add(Pending.NaiaId);
	}
	NUBattleResult::BuildNaiaEntries(this, CapturedNaiaIds, ResultView.CapturedNaia);

	bResultScreenOpen = PC->ShowBattleResult(
		ResultView,
		FSimpleDelegate::CreateUObject(this, &ThisClass::HandleBattleResultDismissed));

	// 창이 안 떴으면(위젯 클래스 미지정 등) 여기서 멈추면 전투가 끝나지 않는다
	if (!bResultScreenOpen)
	{
		BeginFinishTransition();
	}
}

void ANUBattleEncounter::HandleBattleResultDismissed()
{
	if (!bResultScreenOpen)
	{
		return;
	}

	bResultScreenOpen = false;

	// 창의 수명은 PlayerController 가 들고 있음
	if (ANUPlayerController* PC = Cast<ANUPlayerController>(
		UGameplayStatics::GetPlayerController(this, 0)))
	{
		PC->CloseBattleResult();
	}

	BeginFinishTransition();
}

float ANUBattleEncounter::GetCaptureShakeRemainingSeconds() const
{
	float Remaining = 0.f;

	for (const FNUBattleParticipant& Participant : Participants)
	{
		if (const ANUBattleAvatar* Avatar = Cast<ANUBattleAvatar>(Participant.AvatarActor))
		{
			Remaining = FMath::Max(Remaining, Avatar->GetCaptureShakeRemainingSeconds());
		}
	}

	return Remaining;
}

void ANUBattleEncounter::BeginFinishTransition()
{
	// 아바타를 필드로 되돌리는 동안 가린다
	SetTransitionCue(ENUTransitionCue::FadeOut);

	// 카메라 페이드가 UMG를 덮지 못하므로 체력바만 따로 숨긴다.
	// 아바타 자체를 숨기면 페이드가 시작되기 전에 파티가 사라져 보인다
	for (const FNUBattleParticipant& Participant : Participants)
	{
		if (ANUBattleAvatar* Avatar = Cast<ANUBattleAvatar>(Participant.AvatarActor))
		{
			Avatar->SetVitalBarHidden(true);
		}
	}

	ForceNetUpdate();

	// 끝내고 아바타 교체할 때 화면 전환
	GetWorldTimerManager().SetTimer(
		TransitionTimer,
		this,
		&ThisClass::CompleteFinishBattle,
		GetTransitionSeconds(),
		false
		);
}

void ANUBattleEncounter::CompleteFinishBattle()
{
	TArray<TWeakObjectPtr<UNURecoveryComponent>> PendingRecoveries;
	// 아직 암전 중일 때 아레나를 먼저 숨기기
	// 숨기지 않으면 LifeSpan이 5.f라 배경이 보임
	SetActorHiddenInGame(true);

	// LifeSpan을 5.f 로 뒀으므로 조명은 미리 복구
	SetBattleStageLightingActive(false);

	RestoreWaitingNaiaAfterBattle();

	for (FNUBattleParticipant& Participant : Participants)
	{
		AActor* BattleAvatar = Participant.AvatarActor;
		
		if (Participant.Team == ENUBattleTeam::Ally)
		{
			INUCombatantInterface* CombatantInterface = Cast<INUCombatantInterface>(Participant.ASCOwner);
			AActor* OriginAvatar = OriginAvatars.FindRef(Participant.CombatantId);
			
			// 로컬 플레이어 사망은 등록 거점 복귀로 넘기고 나머지는 기존 복구 처리
			if (UNUAbilitySystemComponent* ASC = Cast<UNUAbilitySystemComponent>(
				UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Participant.ASCOwner)))
			{
				ASC->ClearTurnBasedEffects();
				ANUPlayerState* PlayerState = Cast<ANUPlayerState>(Participant.ASCOwner);
				UNURecoveryComponent* Recovery = PlayerState != nullptr ? PlayerState->FindComponentByClass<UNURecoveryComponent>() : nullptr;
				const bool bQueuedRespawn = !ASC->IsAlive() && Recovery != nullptr && Recovery->CanExecuteAuthorityRecovery();
				if (bQueuedRespawn)
				{
					PendingRecoveries.AddUnique(Recovery);
					// 부활 실행은 Ended 이후지만, 전투 암전은 필드 카메라 복원 전부터 유지한다.
					if (ANUPlayerController* PC = Cast<ANUPlayerController>(PlayerState->GetOwner())) { PC->ClientHoldRecoveryFade(); }
				}
				if (!bQueuedRespawn)
				{
					// 로컬 회복 대상이 아니면 기존 전투 복구를 유지한다.
					ASC->ReviveIfDead(1.f);
				}
			}
			
			// 나이아는 OriginAvatar가 교체 이전의 나이아를 그대로 들고있음
			// 복구하면 교체 전 외형이 나타나는 문제가 있기 때문에 이전 원본 필드 나이아는 제거
			// 이후에 파티슬롯 0번으로 다시 생성함
			if (Cast<ANUSpiritState>(Participant.ASCOwner))
			{
				if (IsValid(OriginAvatar))
				{
					OriginAvatar->Destroy();
				}
				if (CombatantInterface)
				{
					CombatantInterface->SetCurrentAvatar(nullptr);
					CombatantInterface->SetBattleEncounter(nullptr);
				}
			}
			else
			{
				// 플레이어는 원본 아바타로 복구
				if (CombatantInterface)
				{
					CombatantInterface->SetCurrentAvatar(OriginAvatar);
					CombatantInterface->SetBattleEncounter(nullptr);
				}
				if (IsValid(OriginAvatar))
				{
					OriginAvatar->SetActorHiddenInGame(false);
					OriginAvatar->SetActorEnableCollision(true);

					if (ACharacter* FieldCharacter = Cast<ACharacter>(OriginAvatar))
					{
						if (UCharacterMovementComponent* Movement = FieldCharacter->GetCharacterMovement())
						{
							Movement->SetMovementMode(MOVE_Walking);
						}
					}
				}
			}
		}
		
		// ASC가 필드 아바타를 가리키도록 바꾼 뒤 제거
		if (IsValid(BattleAvatar))
		{
			BattleAvatar->Destroy();
		}

		Participant.AvatarActor = nullptr;
	}

	// 나이아는 원본 필드 아바타 삭제했으니 다시 생성해야 함
	RespawnActiveSpiritFieldAvatars();

	PendingCommands.Reset();
	ActionQueue.Reset();
	OriginAvatars.Reset();
	
	SetBattlePhase(ENUBattlePhase::Ended);
	NotifyEncounterCompleted();
	// 전투 정리와 종료 통지 이후 별도의 부활 전환을 시작한다.
	for (const TWeakObjectPtr<UNURecoveryComponent>& Recovery : PendingRecoveries)
	{
		if (Recovery.IsValid() && !Recovery->QueueDeath(FGuid::NewGuid(), true))
		{
			const ANUPlayerState* PS = Cast<ANUPlayerState>(Recovery->GetOwner());
			if (ANUPlayerController* PC = PS ? Cast<ANUPlayerController>(PS->GetOwner()) : nullptr) { PC->ClientRecoveryFade(false); }
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("전투 결과 : %s"), *UEnum::GetValueAsString(BattleResult));
	
	// 5초뒤에 제거, 복제시간 확보 위함
	SetLifeSpan(5.f);
}

void ANUBattleEncounter::SetBattlePhase(ENUBattlePhase NewPhase)
{
	Phase = NewPhase;
	OnBattlePhaseChanged.Broadcast(Phase, TurnNumber);	// 리슨서버일경우에 호출
	ForceNetUpdate();
}

void ANUBattleEncounter::AbortInitialize()
{
	for (FNUBattleParticipant& Participant : Participants)
	{
		// OriginAvatars에 등록된 액터 가져오기
		// OriginAvatar가 등록되지 않은 경우, 필드 액터 등록
		AActor* OriginAvatar = OriginAvatars.FindRef(Participant.CombatantId);
		if (!IsValid(OriginAvatar) && Participant.Team == ENUBattleTeam::Ally)
		{
			OriginAvatar = Participant.AvatarActor;
		}
		
		// 전투원 되돌리기
		if (INUCombatantInterface* CombatantInterface = Cast<INUCombatantInterface>(Participant.ASCOwner))
		{
			CombatantInterface->SetBattleEncounter(nullptr);
			CombatantInterface->SetCurrentAvatar(OriginAvatar);
		}
		
		// 원래 아바타로 되돌렸으니 새로 생성한 전투 아바타만 파괴
		if (IsValid(Participant.AvatarActor) && Participant.AvatarActor != OriginAvatar)
		{
			Participant.AvatarActor->Destroy();
		}
		
		// 원본 아바타 = 필드아바타의 Hidden, Collision, 움직임 되돌리기
		if (IsValid(OriginAvatar))
		{
			OriginAvatar->SetActorHiddenInGame(false);
			OriginAvatar->SetActorEnableCollision(true);
			
			if (ACharacter* FieldCharacter = Cast<ACharacter>(OriginAvatar))
			{
				if (UCharacterMovementComponent* Movement = FieldCharacter->GetCharacterMovement())
				{
					Movement->SetMovementMode(MOVE_Walking);
				}
			}
		}
		
		Participant.AvatarActor = nullptr;
	}
	
	Participants.Reset();
	OriginAvatars.Reset();
	NotifyEncounterCompleted();
}

// 전투 종료 결과 심볼에 전달
// 심볼 몬스터 제거 or 리스폰
void ANUBattleEncounter::NotifyEncounterCompleted()
{
	if (!HasAuthority() || bCompletionNotified)
	{
		return;
	}

	bCompletionNotified = true;

	// 보상 계산보다 먼저 등록함
	// 잡은 나이아는 승패와 무관하게 가져가야 함
	CommitPendingCaptures();

	PublishBattleWonEvent();
	OnEncounterCompleted.Broadcast(this, BattleResult);
}

void ANUBattleEncounter::CommitPendingCaptures()
{
	// PendingCaptures는 비우지 않음, 보상 계산 시 IsCaptured로 같은 배열을 읽어야 함
	for (const FNUPendingCapture& Pending : PendingCaptures)
	{
		ANUPlayerState* OwnerState = Pending.Owner.Get();
		UNUNaiaRosterComponent* Roster = IsValid(OwnerState) ? OwnerState->GetNaiaRosterComponent() : nullptr;

		if (!IsValid(Roster))
		{
			// 포획한 플레이어가 전투 중 이탈한 경우
			UE_LOG(LogTemp, Warning, TEXT("포획 등록 실패, 로스터를 찾을 수 없습니다 : NaiaId=%d"),
				Pending.NaiaId);
			continue;
		}

		if (!Roster->AcquireNaia(Pending.NaiaId, Pending.Level))
		{
			UE_LOG(LogTemp, Warning, TEXT("포획 등록 실패, 로스터에 자리가 없습니다 : NaiaId=%d"),
				Pending.NaiaId);
			continue;
		}

		if (UNUBattleEventSubsystem* BattleEvents = GetGameInstance()
			? GetGameInstance()->GetSubsystem<UNUBattleEventSubsystem>()
			: nullptr)
		{
			FNUBattleNaiaCapturedEvent Event;
			Event.EncounterId = EncounterId;
			Event.NaiaId = FName(*LexToString(Pending.NaiaId));
			Event.EligiblePlayerStates.Add(OwnerState);
			BattleEvents->PublishNaiaCaptured(Event);
		}

		UE_LOG(LogTemp, Warning, TEXT("포획 등록 : NaiaId=%d, Level=%d"), Pending.NaiaId, Pending.Level);
	}
}

void ANUBattleEncounter::PublishBattleWonEvent()
{
	if (GetBattleEventHandler() != nullptr && !GetBattleEventHandler()->ShouldPublishStandardVictory(this)) { return; }
	if (BattleResult != ENUBattleResult::Victory)
	{
		return;
	}

	UNUBattleEventSubsystem* BattleEvents = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UNUBattleEventSubsystem>()
		: nullptr;
	if (!IsValid(BattleEvents))
	{
		return;
	}

	// 더미 연결에서는 첫 번째 적의 EnemyId를 전투 목표 ID로 사용한다.
	// Objective의 TargetId가 None이면 기존 매칭 규칙에 따라 모든 승리를 받는다.
	FName BattleTargetId = NAME_None;
	for (const FNUBattleParticipant& Participant : Participants)
	{
		if (Participant.Team == ENUBattleTeam::Enemy && Participant.EnemySpec.IsValid())
		{
			BattleTargetId = Participant.EnemySpec.EnemyId;
			break;
		}
	}

	// 플레이어와 활성 정령이 같은 PlayerState를 공유하므로 한 전투에 한 번만 전달한다.
	TSet<ANUPlayerState*> NotifiedPlayerStates;
	FNUBattleWonEvent Event;
	Event.EncounterId = EncounterId;
	Event.EnemyId = BattleTargetId;
	for (const FNUBattleParticipant& Participant : Participants)
	{
		ANUPlayerState* PlayerState = Cast<ANUPlayerState>(Participant.OwningPlayerState);
		if (Participant.Team != ENUBattleTeam::Ally
			|| !IsValid(PlayerState)
			|| NotifiedPlayerStates.Contains(PlayerState))
		{
			continue;
		}

		NotifiedPlayerStates.Add(PlayerState);
		Event.EligiblePlayerStates.Add(PlayerState);
	}

	BattleEvents->PublishBattleWon(Event);
}

void ANUBattleEncounter::BindEnemyDefeatEvents()
{
	UnbindEnemyDefeatEvents();

	if (!HasAuthority())
	{
		return;
	}

	// 적의 dead 태그 변경 이벤트 구독
	for (const FNUBattleParticipant& Participant : Participants)
	{
		if (Participant.Team != ENUBattleTeam::Enemy)
		{
			continue;
		}

		UAbilitySystemComponent* AbilitySystem = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Participant.ASCOwner);
		if (!IsValid(AbilitySystem) || EnemyDeathDelegateHandles.Contains(AbilitySystem))
		{
			continue;
		}

		const FDelegateHandle Handle = AbilitySystem->RegisterGameplayTagEvent(
			NU::State::Dead,
			EGameplayTagEventType::NewOrRemoved).AddUObject(
			this,
			&ThisClass::HandleEnemyDeadTagChanged);
		EnemyDeathDelegateHandles.Add(AbilitySystem, Handle);
	}

	BroadcastEnemyDefeatedEvents();
}

void ANUBattleEncounter::UnbindEnemyDefeatEvents()
{
	// dead 태그 이벤트 구독 해제
	for (const TPair<TWeakObjectPtr<UAbilitySystemComponent>, FDelegateHandle>& Pair : EnemyDeathDelegateHandles)
	{
		if (UAbilitySystemComponent* AbilitySystem = Pair.Key.Get())
		{
			AbilitySystem->RegisterGameplayTagEvent(NU::State::Dead, EGameplayTagEventType::NewOrRemoved).Remove(Pair.Value);
		}
	}

	EnemyDeathDelegateHandles.Reset();
}

void ANUBattleEncounter::HandleEnemyDeadTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	// dead 태그가 추가됐을 때 처치 이벤트 발행 여부
	if (NewCount > 0)
	{
		BroadcastEnemyDefeatedEvents();
	}
}

void ANUBattleEncounter::BroadcastEnemyDefeatedEvents()
{
	if (!HasAuthority())
	{
		return;
	}

	UNUBattleEventSubsystem* BattleEvents = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UNUBattleEventSubsystem>()
		: nullptr;
	if (!IsValid(BattleEvents))
	{
		return;
	}

	TArray<TWeakObjectPtr<APlayerState>> EligiblePlayerStates;
	GatherEligiblePlayerStates(EligiblePlayerStates);

	// 새로 처치된 적마다 이벤트 한 번만 발행
	for (const FNUBattleParticipant& Participant : Participants)
	{
		if (Participant.Team != ENUBattleTeam::Enemy
			|| !Participant.EnemySpec.IsValid()
			|| IsAlive(Participant.CombatantId)
			|| IsCaptured(Participant.CombatantId)
			|| ReportedDefeatedEnemyCombatants.Contains(Participant.CombatantId))
		{
			continue;
		}

		ReportedDefeatedEnemyCombatants.Add(Participant.CombatantId);

		FNUBattleEnemyDefeatedEvent Event;
		Event.EncounterId = EncounterId;
		Event.CombatantId = Participant.CombatantId;
		Event.EnemyId = Participant.EnemySpec.EnemyId;
		Event.EligiblePlayerStates = EligiblePlayerStates;
		BattleEvents->PublishEnemyDefeated(Event);
	}
}

void ANUBattleEncounter::GatherEligiblePlayerStates(TArray<TWeakObjectPtr<APlayerState>>& OutPlayerStates) const
{
	OutPlayerStates.Reset();

	// 전투 결과를 전달할 아군 플레이어를 중복 없이 수집
	TSet<APlayerState*> AddedPlayerStates;
	for (const FNUBattleParticipant& Participant : Participants)
	{
		APlayerState* PlayerState = Participant.OwningPlayerState.Get();
		if (Participant.Team != ENUBattleTeam::Ally
			|| !IsValid(PlayerState)
			|| AddedPlayerStates.Contains(PlayerState))
		{
			continue;
		}

		AddedPlayerStates.Add(PlayerState);
		OutPlayerStates.Add(PlayerState);
	}
}

const FNUBattleParticipant* ANUBattleEncounter::FindParticipant(int32 CombatantId) const
{
	return Participants.FindByPredicate([CombatantId](const FNUBattleParticipant& Participant)
	{
		return Participant.CombatantId == CombatantId;
	});
}

FNUBattleParticipant* ANUBattleEncounter::FindParticipant(int32 CombatantId)
{
	return Participants.FindByPredicate([CombatantId](const FNUBattleParticipant& Participant)
	{
		return Participant.CombatantId == CombatantId;
	});
}

bool ANUBattleEncounter::IsAlive(int32 CombatantId) const
{
	const FNUBattleParticipant* Participant = FindParticipant(CombatantId);
	if (!Participant)
	{
		return false;
	}
	
	UNUAbilitySystemComponent* ASC = 
		Cast<UNUAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Participant->ASCOwner));
	
	return IsValid(ASC) && ASC->IsAlive();
}

bool ANUBattleEncounter::HasLivingTeam(ENUBattleTeam Team) const
{
	for (const FNUBattleParticipant& Participant : Participants)
	{
		// 한명이라도 해당 팀원이 살아있다면 true
		if (IsAlive(Participant.CombatantId) && Participant.Team == Team)
		{
			return true;
		}
	}
	return false;
}

bool ANUBattleEncounter::HasLivingPlayer() const
{
	for (const FNUBattleParticipant& Participant : Participants)
	{
		if (Participant.Team != ENUBattleTeam::Ally)
		{
			continue;
		}

		// 나이아는 세지 않음, ASCOwner 가 PlayerState 인 전투원만 확인함
		if (!Cast<ANUPlayerState>(Participant.ASCOwner))
		{
			continue;
		}

		if (IsAlive(Participant.CombatantId))
		{
			return true;
		}
	}
	return false;
}

bool ANUBattleEncounter::ResolveStageFormation(ENUBattleFormationRow Row, int32 SlotIndex,
	int32 SlotCount, FTransform& OutTransform) const
{
	UWorld* World = GetWorld();
	UNUBattleEncounterSubsystem* EncounterSubsystem = IsValid(World)
		? World->GetSubsystem<UNUBattleEncounterSubsystem>()
		: nullptr;

	if (!IsValid(EncounterSubsystem))
	{
		return false;
	}

	return EncounterSubsystem->ResolveFormationTransform(Row, SlotIndex, SlotCount, OutTransform);
}

FTransform ANUBattleEncounter::GetAllySpawnTransform(int32 PartySlotIndex, int32 PartyCount,
	bool bIsSpirit) const
{
	ENUBattleFormationRow Row = bIsSpirit
		? ENUBattleFormationRow::Naia
		: ENUBattleFormationRow::Player;

	FTransform StageTransform;
	if (ResolveStageFormation(Row, PartySlotIndex, PartyCount, StageTransform))
	{
		return StageTransform;
	}

	int32 SafePartyCount = FMath::Max(PartyCount, 1);
	float CenteredSlotIndex = PartySlotIndex - (SafePartyCount - 1) * 0.5f;
	
	float SlotX = CenteredSlotIndex * 150.f;
	float FormationY = bIsSpirit ? 230.f : 450.f;
	FVector LocalOffset(SlotX, FormationY, 50.f);

	return FTransform(FRotator::ZeroRotator, GetActorTransform().TransformPosition(LocalOffset));
}

FTransform ANUBattleEncounter::GetEnemySpawnTransform(int32 SlotIndex, int32 EnemyCount) const
{
	FTransform StageTransform;
	if (ResolveStageFormation(ENUBattleFormationRow::Enemy, SlotIndex, EnemyCount, StageTransform))
	{
		return StageTransform;
	}

	FVector LocalOffset(-SlotIndex * 220.f, -300.f, 50.f);
	
	return FTransform(FRotator::ZeroRotator, GetActorTransform().TransformPosition(LocalOffset));
}

// 초기화 시 전투 참가자가 유효한지 확인하는 함수
bool ANUBattleEncounter::ValidateInitialParticipants(const TArray<FNUBattleParticipant>& InitialParticipants,
	const UNUNaiaDataSubsystem* NaiaDataSubsystem) const
{
	bool bHasEnemy = false;
	
	for (const FNUBattleParticipant& Participant : InitialParticipants)
	{
		if (Participant.Team == ENUBattleTeam::Enemy)
		{
			bHasEnemy =  true;
			
			FName EnemyId = Participant.EnemySpec.EnemyId;
			if (!Participant.EnemySpec.IsValid() || !NaiaDataSubsystem->HasDefinition(EnemyId))
			{
				// 적 ID가 INDEX_NONE 이거나, 유효하지 않은 ID라면 false
				UE_LOG(LogTemp, Error, TEXT("Enemy 데이터를 찾을 수 없습니다. EnemyId=%s"), *EnemyId.ToString());
				return false;
			}
			continue;
		}
		
		// 아군 ASCOwner가 INUCombatantInterface를 구현해야 BattleAvatar 교체 가능
		if (!Cast<INUCombatantInterface>(Participant.ASCOwner))
		{
			UE_LOG(LogTemp, Error, TEXT("아군 %s가 INUCombatantInterface를 구현하지 않습니다"),
				*GetNameSafe(Participant.ASCOwner));
			return false;
		}
		
		// 진형 슬롯 배정 시 OwningPlayerState가 필요함
		if (!Participant.OwningPlayerState.Get())
		{
			UE_LOG(LogTemp, Error, TEXT("아군 %s에 OwningPlayerState가 없습니다"),
				*GetNameSafe(Participant.ASCOwner));
			return false;
		}
		
		bool bIsSpirit = false;
		if (!ResolveAllyAvatarClass(Participant.ASCOwner, bIsSpirit))
		{
			UE_LOG(LogTemp, Error, TEXT("아군 %s의 전투 아바타 클래스를 결정할 수 없습니다 "
				"(PlayerBattleAvatarClass / SpiritBattleAvatarClass 확인)"),
				*GetNameSafe(Participant.ASCOwner));
			return false;
		}
	}
	
	// 적은 있지만 EnemyClass가 설정되지 않음
	if (bHasEnemy && !EnemyClass)
	{
		UE_LOG(LogTemp, Error, TEXT("EnemyClass가 설정되지 않았습니다!"));
		return false;
	}
	
	if (!bHasEnemy)
	{
		UE_LOG(LogTemp, Error, TEXT("적이 없는 전투는 시작할 수 없습니다"));
		return false;
	}
	
	return true;
}

// 아군 ASCOwner 종류에 따라 전투 아바타 클래스 결정
TSubclassOf<ANUBattleAvatar> ANUBattleEncounter::ResolveAllyAvatarClass(const AActor* ASCOwner,
	bool& bOutIsSpirit) const
{
	bOutIsSpirit = false;
	
	if (Cast<ANUPlayerState>(ASCOwner))
	{
		return PlayerBattleAvatarClass;
	}
	
	if (Cast<ANUSpiritState>(ASCOwner))
	{
		bOutIsSpirit = true;
		return SpiritBattleAvatarClass;
	}
	
	return nullptr;
}

UNUAbilitySystemComponent* ANUBattleEncounter::FindCombatantASC(int32 CombatantId) const
{
	const FNUBattleParticipant* Participant = FindParticipant(CombatantId);
	if (!Participant)
	{
		return nullptr;
	}
	
	return Cast<UNUAbilitySystemComponent>(
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Participant->ASCOwner));
}

float ANUBattleEncounter::GetCombatantSpeed(int32 CombatantId) const
{
	const UNUAbilitySystemComponent* ASC = FindCombatantASC(CombatantId);
	const UNUCombatAttributeSet* Combat = IsValid(ASC) ? ASC->GetCombatAttributeSet() : nullptr;

	return Combat ? Combat->GetSpeed() : 0.f;
}

bool ANUBattleEncounter::HasForceLastAction(int32 CombatantId) const
{
	const UNUAbilitySystemComponent* ASC = FindCombatantASC(CombatantId);
	if (!IsValid(ASC))
	{
		return false;
	}
	
	// 둔화가 걸려있는지 확인함
	return ASC->HasStatusEffectMatching(
		[](const FNUStatusEffectDefinition& Definition)
		{
			return Definition.bForceLastAction;
		}
		);
}

bool ANUBattleEncounter::ConsumeFirstStrike(int32 CombatantId)
{
	UNUAbilitySystemComponent* ASC = FindCombatantASC(CombatantId);
	if (!IsValid(ASC))
	{
		return false;
	}
	
	// 패시브 번호는 ASC 쪽에 모아둠, 인카운터가 번호를 알면 같은 번호가 두 파일에 흩어짐
	return ASC->ConsumeFirstStrike();
}

bool ANUBattleEncounter::HasActionBlockingStatus(int32 CombatantId) const
{
	const UNUAbilitySystemComponent* ASC = FindCombatantASC(CombatantId);
	if (!IsValid(ASC))
	{
		return false;
	}
	
	// 기절 같은 상태이상과, 스스로를 묶는 스킬(2210 개화)이 같은 자리에서 막힘
	if (ASC->IsSelfBoundActionBlocked())
	{
		return true;
	}
	
	return ASC->HasStatusEffectMatching(
		[](const FNUStatusEffectDefinition& Definition)
		{
			return Definition.bActionBlock;
		}
		);
}

bool ANUBattleEncounter::IsActiveSkillBlocked(int32 CombatantId) const
{
	const UNUAbilitySystemComponent* ASC = FindCombatantASC(CombatantId);
	if (!IsValid(ASC))
	{
		return false;
	}
	
	return ASC->HasStatusEffectMatching(
		[](const FNUStatusEffectDefinition& Definition)
		{
			return Definition.bBlockActiveSkill;
		}
		);
}

bool ANUBattleEncounter::IsSkillBlocked(int32 CombatantId, int32 SkillId) const
{
	if (SkillId <= 0)
	{
		return false;
	}
	
	// 실명처럼 액티브 전부를 막는 것
	if (IsActiveSkillBlocked(CombatantId))
	{
		return true;
	}
	
	const UNUAbilitySystemComponent* ASC = FindCombatantASC(CombatantId);
	if (!IsValid(ASC))
	{
		return false;
	}
	
	const UGameInstance* GameInstance = GetGameInstance();
	const UNUBattleDataSubsystem* BattleData =
		IsValid(GameInstance) ? GameInstance->GetSubsystem<UNUBattleDataSubsystem>() : nullptr;
	
	const FNUActiveSkillDefinition* Definition =
		IsValid(BattleData) ? BattleData->FindActiveSkillDefinition(SkillId) : nullptr;
	
	// 정의가 없는건 여기서 막지 않음
	return Definition && ASC->IsElementSkillBlocked(Definition->AttackElement);
}

bool ANUBattleEncounter::RollActionFailure(int32 CombatantId) const
{
	const UNUAbilitySystemComponent* ASC = FindCombatantASC(CombatantId);
	if (!IsValid(ASC))
	{
		return false;
	}
	
	TArray<const FNUStatusEffectDefinition*> Statuses;
	ASC->GetActiveStatusEffects(Statuses);

	// 행동 실패 상태이상 확인
	for (const FNUStatusEffectDefinition* Definition : Statuses)
	{
		if (Definition->ActionFailRate <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		// 환각은 행동을 취소하고 아군을 재지정하므로 여기선 제외
		if (Definition->StatusEffectTag.MatchesTagExact(NU::State::Ailment::Hallucination))
		{
			continue;
		}

		if (FMath::FRand() < Definition->ActionFailRate)
		{
			UE_LOG(LogTemp, Warning, TEXT("행동 실패 : 전투원 %d (%s, 확률 %.2f)"),
				CombatantId, *Definition->KoreanName.ToString(), Definition->ActionFailRate);

			return true;
		}
	}

	return false;
}

bool ANUBattleEncounter::RollHallucination(int32 CombatantId) const
{
	const UNUAbilitySystemComponent* ASC = FindCombatantASC(CombatantId);
	if (!IsValid(ASC))
	{
		return false;
	}

	// 확률은 데이터에서 읽고 환각이라는 사실만 태그로 판단함
	const FNUStatusEffectDefinition* Hallucination = nullptr;

	TArray<const FNUStatusEffectDefinition*> Statuses;
	ASC->GetActiveStatusEffects(Statuses);

	for (const FNUStatusEffectDefinition* Definition : Statuses)
	{
		if (Definition->StatusEffectTag.MatchesTagExact(NU::State::Ailment::Hallucination))
		{
			Hallucination = Definition;
			break;
		}
	}

	if (!Hallucination || Hallucination->ActionFailRate <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	if (FMath::FRand() >= Hallucination->ActionFailRate)
	{
		return false;
	}

	UE_LOG(LogTemp, Warning, TEXT("환각 발동 : 전투원 %d (확률 %.2f)"),
		CombatantId, Hallucination->ActionFailRate);

	return true;
}

bool ANUBattleEncounter::ApplyHallucinationToCurrentAction(FNUBattleCommandRequest& InOutCommand)
{
	const FNUBattleParticipant* Source = FindParticipant(InOutCommand.SourceCombatantId);
	if (!Source)
	{
		return false;
	}
	
	// 아군 진영에서 아무나 한명 타겟으로 지정함
	const FNUBattleParticipant* Target = FindRandomParticipant(Source->Team);
	if (!Target)
	{
		return false;
	}
	
	// 원래 행동을 버리고 기본 공격으로 바꿈
	// 투입한 SP 는 돌려주지 않고 1타만 나감
	InOutCommand.CommandType = ENUBattleCommandType::Ability;
	InOutCommand.AbilityId = NU::Ability::Battle::BasicAttack;
	InOutCommand.SkillId = 0;
	InOutCommand.InstanceId.Invalidate();
	InOutCommand.SelectedTargetId = Target->CombatantId;
	InOutCommand.HitCount = 1;
	
	// 큐에도 반영해야 함, 타격 시점의 대상 재해결이 이 항목을 보고 정책을 정함
	ActionQueue[CurrentActionIndex].Command = InOutCommand;
	ActionQueue[CurrentActionIndex].bHallucinated = true;
	
	UE_LOG(LogTemp, Warning, TEXT("환각 : 전투원 %d 이 아군 %d 에게 기본 공격"),
		InOutCommand.SourceCombatantId, Target->CombatantId);
	
	return true;
}

bool ANUBattleEncounter::IsHallucinatedAction(int32 CombatantId) const
{
	// 지금 실행 중인 행동만 봄
	if (!ActionQueue.IsValidIndex(CurrentActionIndex))
	{
		return false;
	}
	
	const FNUResolvedAction& Action = ActionQueue[CurrentActionIndex];
	
	return Action.bHallucinated && Action.Command.SourceCombatantId == CombatantId;
}

void ANUBattleEncounter::ShowActionFailPopup(int32 CombatantId)
{
	const FNUBattleParticipant* Participant = FindParticipant(CombatantId);
	if (!Participant)
	{
		return;
	}
	
	if (ANUBattleAvatar* Avatar = Cast<ANUBattleAvatar>(Participant->AvatarActor))
	{
		Avatar->MulticastShowCombatPopup(FNUCombatPopupData(ENUCombatPopupType::ActionFail));
	}
}

// 행동 순서에 사용할 속도 구하기
// 플레이어 측(캐릭터 + 나이아)은 살아있는 둘의 평균을 사용함
// 둔화는 이 값과 무관하게 bForceLast 로만 처리되므로 여기선 사용하지 않음
float ANUBattleEncounter::ResolveActionSpeed(const FNUBattleCommandRequest& Command)
{
	const FNUBattleParticipant* Source = FindParticipant(Command.SourceCombatantId);
	if (!Source)
	{
		return 0.f;
	}

	// 적은 개체 속도 그대로
	const APlayerState* OwnerState = Source->OwningPlayerState.Get();
	if (Source->Team != ENUBattleTeam::Ally || !IsValid(OwnerState))
	{
		return GetCombatantSpeed(Source->CombatantId);
	}

	// 같은 플레이어가 조종하는 전투원 중 살아있는 것만 평균
	// 나이아를 안 데려왔거나 죽었으면 플레이어 속도 그대로 사용함
	float SpeedSum = 0.f;
	int32 SpeedCount = 0;

	for (const FNUBattleParticipant& Participant : Participants)
	{
		if (Participant.Team != ENUBattleTeam::Ally
			|| Participant.OwningPlayerState != OwnerState
			|| !IsAlive(Participant.CombatantId))
		{
			continue;
		}

		SpeedSum += GetCombatantSpeed(Participant.CombatantId);
		++SpeedCount;
	}

	if (SpeedCount <= 0)
	{
		return GetCombatantSpeed(Source->CombatantId);
	}

	return SpeedSum / static_cast<float>(SpeedCount);
}

int32 ANUBattleEncounter::ResolveActionPriority(const FNUBattleCommandRequest& Command)
{
	FNUBattleActionContext Context;
	ResolveActionContext(Command, Context);
	
	return Context.ActionPriority;
}

ENUTargetPolicy ANUBattleEncounter::ResolveCommandTargetPolicy(const FNUBattleCommandRequest& Command) const
{
	FNUBattleActionContext Context;
	ResolveActionContext(Command, Context);
	
	return Context.TargetPolicy;
}

bool ANUBattleEncounter::ResolveTargets(const FNUBattleParticipant& Source, ENUTargetPolicy Policy,
	int32 SelectedTargetId, TArray<int32>& OutTargetIds) const
{
	OutTargetIds.Reset();
	
	// 환각으로 바뀐 행동은 시전자 진영을 때림
	// 기본 공격 어빌리티가 기본 타겟이 SingleEnemy로 되어있으므로 여기서 덮어써야 함
	// 발동 시점과 타격 시점의 대상 재해결이 같은 결과를 내야 함
	if (IsHallucinatedAction(Source.CombatantId))
	{
		Policy = ENUTargetPolicy::SingleAlly;
	}
	
	// 진영 구분
	ENUBattleTeam AllyTeam = Source.Team;
	ENUBattleTeam EnemyTeam = Source.Team == ENUBattleTeam::Ally ? ENUBattleTeam::Enemy : ENUBattleTeam::Ally;
	
	switch (Policy)
	{
	case ENUTargetPolicy::Self:
		OutTargetIds.Add(Source.CombatantId);
		break;
		
	case ENUTargetPolicy::SingleEnemy:
	case ENUTargetPolicy::SingleAlly:
		{
			ENUBattleTeam DesiredTeam = Policy == ENUTargetPolicy::SingleEnemy
				? EnemyTeam	
				: AllyTeam;
			
			const FNUBattleParticipant* Target = FindParticipant(SelectedTargetId);
			
			// 타겟이 유효하지않거나 진영이 안맞으면 같은 진영의 랜덤 전투원으로 재지정
			if (!Target || Target->Team != DesiredTeam || !IsAlive(Target->CombatantId))
			{
				Target = FindRandomParticipant(DesiredTeam);
			}
			
			if (Target)
			{
				OutTargetIds.Add(Target->CombatantId);
			}
			break;
		}
		
	case ENUTargetPolicy::AllEnemies:
		GetLivingCombatantIds(EnemyTeam, OutTargetIds);
		break;
		
	case ENUTargetPolicy::AllAllies:
		GetLivingCombatantIds(AllyTeam, OutTargetIds);
		break;
	}
	
	return !OutTargetIds.IsEmpty();
}

// 커맨드가 어떤 행동인지 해결함
// 스킬이면 스킬 정의, 아이템이면 아이템 정의, 그 외면 어빌리티 CDO 조회
bool ANUBattleEncounter::ResolveActionContext(const FNUBattleCommandRequest& Command, FNUBattleActionContext& Out) const
{
	Out = FNUBattleActionContext();
	
	// 조회에 실패해도 사용되게 기본값 적용
	switch (Command.CommandType)
	{
	case ENUBattleCommandType::Ability:
		Out.TargetPolicy = ENUTargetPolicy::SingleEnemy;
		break;
		
	case ENUBattleCommandType::Item:
		Out.TargetPolicy = ENUTargetPolicy::SingleAlly;
		break;
		
	default:
		Out.TargetPolicy = ENUTargetPolicy::Self;
	}
	
	const FNUBattleParticipant* Source = FindParticipant(Command.SourceCombatantId);
	if (!Source)
	{
		return false;
	}
	
	UNUAbilitySystemComponent* ASC = Cast<UNUAbilitySystemComponent>(
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Source->ASCOwner));
	if (!IsValid(ASC))
	{
		return false;
	}
	
	// 1. 어빌리티 CDO로 우선 처리
	// 아이템 커맨드는 abilityId 가 비어있으므로 공용 UseItem 태그로 채우기
	const FGameplayTag LookupAbilityId = 
		Command.CommandType == ENUBattleCommandType::Item
			? NU::Ability::Battle::UseItem
			: Command.AbilityId;
	
	const FGameplayAbilitySpecHandle Handle = ASC->FindBattleAbilityHandle(LookupAbilityId);
	const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handle);
	const UNUBattleGameplayAbility* Ability = Spec
		? Cast<UNUBattleGameplayAbility>(Spec->Ability)
		: nullptr;
	
	if (Ability)
	{
		Out.DisplayName = Ability->GetDisplayName();
		Out.TargetPolicy = Ability->GetTargetPolicy();
		Out.ActionPriority = Ability->GetActionPriority();
		
		// 플레이어 전용 어빌리티면 SP 소모, 타격 횟수당 SP 소모치 대입 
		if (const UNUPlayerBattleAbility* PlayerAbility = Cast<UNUPlayerBattleAbility>(Ability))
		{
			Out.SPCost = PlayerAbility->GetSPCost();
			Out.SPCostPerHit = PlayerAbility->GetSPCostPerHit();
		}
	}
	
	// 2. 데이터에 내용이 있다면 읽어서 덮어쓰기
	if (Command.CommandType == ENUBattleCommandType::Item)
	{
		const FNUItemDefRow* Def = FindItemDef(Command);
		if (!Def)
		{
			return false;
		}
		
		Out.DisplayName = Def->DisplayName;
		Out.TargetPolicy = Def->BattleTargetPolicy;
		Out.SPCost = 0;
		Out.SPCostPerHit = 0;
	}
	
	// 스킬은 정의와 효과 행으로 컨텍스트를 생성함
	// 성공하면 bValid가 켜지고 실행기가 이 값으로 돔
	if (Command.SkillId > 0)
	{
		const UGameInstance* GameInstance = GetGameInstance();
		const UNUBattleDataSubsystem* BattleData =
			IsValid(GameInstance) ? GameInstance->GetSubsystem<UNUBattleDataSubsystem>() : nullptr;

		if (!IsValid(BattleData))
		{
			UE_LOG(LogTemp, Error, TEXT("전투 데이터 서브시스템이 없어 스킬을 해결할 수 없습니다 : %d"),
				Command.SkillId);

			return false;
		}

		// 나이아 표에 없으면 직업 표에서 확인함
		const bool bJobSkill = BattleData->FindActiveSkillDefinition(Command.SkillId) == nullptr;

		// 실패하면 Out은 위에서 채운 어빌리티 CDO 값 그대로 남음
		// bValid가 꺼져 있으므로 실행기가 붙지 않고 기존 경로로 흘러감
		// 직업 스킬은 이번 턴 카드로 계수가 갈릴 수 있음 (3302 점괘 투사)
		const bool bBuilt = bJobSkill
			? NUJobSkill::BuildActionContext(Command.SkillId, *BattleData, Out, ASC->GetCurrentJobCard())
			: NUActiveSkill::BuildActionContext(Command.SkillId, *BattleData, Out);

		if (!bBuilt)
		{
			return false;
		}
	}

	if (Source->Team == ENUBattleTeam::Enemy
		&& (Out.TargetPolicy == ENUTargetPolicy::SingleEnemy 
			|| Out.TargetPolicy == ENUTargetPolicy::AllEnemies))
	{
		Out.bReactable = true;
		
		if (Command.SkillId > 0)
		{
			const UGameInstance* GameInstance = GetGameInstance();
			const UNUBattleDataSubsystem* BattleData =
				IsValid(GameInstance) ? GameInstance->GetSubsystem<UNUBattleDataSubsystem>() : nullptr;
			
			// 표에서 해당 스킬의 패링 정보가 있다면 패링 정보 부여
			// 표에 정보가 없다면 bParryable 이 false라 회피 처리
			if (const FNUSkillParryDefinition* Definition = IsValid(BattleData) 
				? BattleData->FindSkillParryDefinition(Command.SkillId) 
				: nullptr)
			{
				Out.bParryable = true;
				Out.ParryTiming.ChallengeDuration = Definition->ChallengeDuration;
				Out.ParryTiming.SuccessAnchor = Definition->SuccessAnchor;
				Out.ParryTiming.SuccessWindow = Definition->SuccessWindow;
			}
		}
		else if (Ability)
		{
			// SkillId = 0 인 행동은 지금처럼 어빌리티의 CDO 사용하도록 함
			Out.bParryable = Ability->IsParryable();
			Out.ParryTiming = Ability->GetParryTiming();
		}
		
		// 패턴에서 패링 타이밍을 덮어쓰는경우 
		if (Command.bOverrideParryTiming)
		{
			Out.ParryTiming.ChallengeDuration = Command.ParryChallengeDuration;
			Out.ParryTiming.SuccessAnchor = Command.ParrySuccessAnchor;
			Out.ParryTiming.SuccessWindow = Command.ParrySuccessWindow;
		}
	}
	
	// 남은 SP 를 전부 쓰는 스킬 (3203 나이아 개화) — 비용은 남은 SP 전부, 계수는 쓴 SP 로 정함
	// 명령을 받을 때는 아직 안 뗀 지금 SP 가, 실행할 때는 그때 예약해 둔 양이 기준이라 단계로 가름
	// 비용이 곧 남은 SP 라 상태이상 · 카드 보정을 붙이지 않음
	if (Out.bSpendAllSP)
	{
		const int32 SpentSP = Phase == ENUBattlePhase::ActionExecution
			? Command.SPSpend
			: FMath::FloorToInt(ASC->GetNumericAttribute(UNUVitalAttributeSet::GetSPAttribute()));

		Out.SPCost = FMath::Max(0, SpentSP);
		Out.SPCostPerHit = 0;

		if (const FNUSpendAllSPSkill* SpendAll = NUJobSkill::FindSpendAllSPSkill(Command.SkillId))
		{
			Out.DamageCoefficient = NUJobSkill::ComputeSpendAllSPCoefficient(*SpendAll, Out.SPCost);
		}

		return Ability != nullptr;
	}

	// 상태이상 보정은 모든 분기가 비용을 채운 뒤에 마지막으로 적용함
	// 스킬 분기가 어빌리티 CDO 값을 덮으므로 위쪽에서 곱하면 덮여서 사라짐
	// 직업 스킬은 카드가 바꾸는 비용(3301 운명 읽기)까지, 기본 공격 · 아이템은 상태이상 보정만
	const bool bSkillAction = Command.SkillId > 0;
	Out.SPCost = bSkillAction ? ASC->ApplySkillSPCost(Out.SPCost) : ASC->ApplyStatusSPCost(Out.SPCost);
	Out.SPCostPerHit = bSkillAction
		? ASC->ApplySkillSPCost(Out.SPCostPerHit)
		: ASC->ApplyStatusSPCost(Out.SPCostPerHit);

	return Ability != nullptr;
}

void ANUBattleEncounter::HandleCommandTimeOut()
{
	if (!HasAuthority() || Phase != ENUBattlePhase::CommandSelection)
	{
		return;
	}
	
	UE_LOG(LogTemp, Warning, TEXT("커맨드 선택 %.0f초 초과 — 미제출 전투원에 기본 공격을 할당합니다"),
		MaxCommandSeconds);
	
	for (const FNUBattleParticipant& Participant : Participants)
	{
		if (Participant.Team != ENUBattleTeam::Ally)
		{
			continue;
		}
		if (!IsAlive(Participant.CombatantId))
		{
			continue;
		}
		if (PendingCommands.Contains(Participant.CombatantId))
		{
			continue;
		}
		
		FillDefaultCommand(Participant);
	}
	
	GenerateEnemyCommands();
	BuildActionQueue();
}

void ANUBattleEncounter::FillDefaultCommand(const FNUBattleParticipant& Source)
{
	// 살아있는 적 중 무작위
	const FNUBattleParticipant* Target = FindRandomParticipant(ENUBattleTeam::Enemy);
	if (!Target)
	{
		return;
	}
	
	FNUBattleCommandRequest Command;
	Command.TurnNumber = TurnNumber;
	Command.SourceCombatantId = Source.CombatantId;
	Command.CommandType = ENUBattleCommandType::Ability;
	Command.AbilityId = NU::Ability::Battle::BasicAttack;
	Command.SelectedTargetId = Target->CombatantId;
	
	PendingCommands.Add(Source.CombatantId, MoveTemp(Command));
	
	UE_LOG(LogTemp, Warning, TEXT("자동 기본 공격: 전투원 %d -> %d"),
		Source.CombatantId, Target->CombatantId);
}

float ANUBattleEncounter::GetTransitionSeconds() const
{
	// 서버가 기다려야 하는 건 암전이 완료되는 시간이므로 FadeOut 기준
	if (const ULevelSequence* Sequence = FadeOutSequence.LoadSynchronous())
	{
		if (UMovieScene* MovieScene = Sequence->GetMovieScene())
		{
			FFrameRate TickResolution = MovieScene->GetTickResolution();
			TRange<FFrameNumber> Range = MovieScene->GetPlaybackRange();
			
			return TickResolution.AsSeconds(FFrameTime(Range.Size<FFrameNumber>())) 
				+ TransitionNetworkSlack;
		}
	}
	
	return TransitionFallbackSeconds + TransitionNetworkSlack;
}

UNUBattleGameplayAbility* ANUBattleEncounter::GetActiveBattleAbility() const
{
	if (!ActiveAbilityASC.IsValid() || !ActiveAbilityHandle.IsValid())
	{
		return nullptr;
	}
	
	FGameplayAbilitySpec* Spec = ActiveAbilityASC->FindAbilitySpecFromHandle(ActiveAbilityHandle);
	return Spec ? Cast<UNUBattleGameplayAbility>(Spec->GetPrimaryInstance()) : nullptr;
}

bool ANUBattleEncounter::TriggerActionAbility(UNUAbilitySystemComponent* SourceASC,
                                              const FGameplayAbilitySpecHandle& AbilityHandle, const FGameplayEventData& EventData)
{
	if (!IsValid(SourceASC) || !AbilityHandle.IsValid())
	{
		ScheduleNextAction();
		return false;
	}
	
	// 이전 행동 대기 남아있으면 정리
	UnbindAbilityEnded();
	
	ActiveAbilityASC = SourceASC;
	ActiveAbilityHandle = AbilityHandle;
	AbilityEndedHandle = SourceASC->OnAbilityEnded.AddUObject(this, &ThisClass::HandleAbilityEnded);
	
	GetWorldTimerManager().SetTimer(
		ActionTimeoutHandle, 
		this, 
		&ThisClass::HandleActionTimeout, 
		MaxActionSeconds
		);
	
	if (!SourceASC->TriggerBattleAbility(AbilityHandle, EventData.EventTag, EventData))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s 발동에 실패했습니다!"), *EventData.EventTag.ToString());

		// 발동에 실패하면 ActivateAbility가 불리지 않아 컨텍스트를 아무도 가져가지 않음
		// 비우지 않으면 다음 행동이 이 컨텍스트를 사용할 수 있음
		SourceASC->ClearPendingActionContext();

		UnbindAbilityEnded();
		ScheduleNextAction();
		return false;
	}
	
	return true;
}

void ANUBattleEncounter::ScheduleNextAction()
{
	if (!HasAuthority())
	{
		return;
	}

	// 포획 흔들림이 남아 있으면 그만큼 미룬다
	// 3번 흔들리고 실패한 경우가 아이템 행동보다 길어서, 미루지 않으면 다음 전투원의
	// 공격과 겹쳐 재생된다 — 연출을 보장하는 쪽이 맞다
	const float ShakeRemaining = GetCaptureShakeRemainingSeconds();
	if (ShakeRemaining > KINDA_SMALL_NUMBER)
	{
		GetWorldTimerManager().SetTimer(
			ActionDelayHandle,
			this,
			&ThisClass::ExecuteNextAction,
			ShakeRemaining,
			false
			);

		return;
	}

	GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::ExecuteNextAction);
}

bool ANUBattleEncounter::IsActionAbilityRunning() const
{
	return ActiveAbilityHandle.IsValid();
}

bool ANUBattleEncounter::ReserveSPForCommand(FNUBattleCommandRequest& AcceptedRequest)
{
	const FNUBattleParticipant* Source = FindParticipant(AcceptedRequest.SourceCombatantId);
	if (!Source)
	{
		return false;;
	}

	UAbilitySystemComponent* ASC =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Source->ASCOwner);
	if (!IsValid(ASC))
	{
		return false;
	}

	// 명령을 바꿔 다시 제출한 경우, 먼저 낸 것을 되돌리지 않으면 SP가 소모된 상태로 남음
	// PendingCommands.Add 가 덮어쓰기라 여기서 처리해야 함
	RefundReservedSP(AcceptedRequest.SourceCombatantId);

	FNUBattleActionContext Context;
	ResolveActionContext(AcceptedRequest, Context);
	
	int32 Required = 0;

	if (AcceptedRequest.CommandType == ENUBattleCommandType::Ability)
	{
		// 반복 어빌리티는 첫 타도 비용을 내므로 최소 한 타분은 받아야 함
		// 투입량은 타당 비용 단위로 내리고, 반복이 아니면 고정 비용 투입
		if (Context.SPCostPerHit > 0)
		{
			// 타수를 여기서 한 번만 정함, 실행과 배너가 이 값을 읽음
			AcceptedRequest.HitCount =
				FMath::Max(1, FMath::Max(0, AcceptedRequest.SPSpend) / Context.SPCostPerHit);

			Required = AcceptedRequest.HitCount * Context.SPCostPerHit;
		}
		else
		{
			// 표가 타수를 정한 스킬은 그 값으로 반복함, 비용은 고정이라 타수와 무관
			AcceptedRequest.HitCount = FMath::Max(1, Context.HitCount);
			Required = Context.SPCost;

			// 남은 SP 를 전부 쓰는 스킬은 쓸 SP 가 있어야 함, 0 이면 계수도 0 이라 턴만 버림
			if (Context.bSpendAllSP && Required <= 0)
			{
				return false;
			}
		}
	}

	float CurrentSP = ASC->GetNumericAttribute(UNUVitalAttributeSet::GetSPAttribute());
	if (static_cast<float>(Required) > CurrentSP)
	{
		// 모자란 경우 false, 
		// SP가 모자란데 넘게 제출했거나 오류이므로 false 리턴해야 함
		return false;
	}

	// 여기서 확정해야 실행 시점의 타수와 차감량이 어긋나지 않음
	AcceptedRequest.SPSpend = Required;

	if (Required > 0)
	{
		ASC->SetNumericAttributeBase(
			UNUVitalAttributeSet::GetSPAttribute(), CurrentSP - static_cast<float>(Required));
	}

	return true;
}

void ANUBattleEncounter::RefundReservedSP(int32 CombatantId)
{
	const FNUBattleCommandRequest* Previous = PendingCommands.Find(CombatantId);
	if (!Previous || Previous->SPSpend <= 0)
	{
		return;
	}

	const FNUBattleParticipant* Source = FindParticipant(CombatantId);
	if (!Source)
	{
		return;
	}

	UAbilitySystemComponent* ASC =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Source->ASCOwner);
	if (!IsValid(ASC))
	{
		return;
	}

	float CurrentSP = ASC->GetNumericAttribute(UNUVitalAttributeSet::GetSPAttribute());
	float MaxSP = ASC->GetNumericAttribute(UNUVitalAttributeSet::GetMaxSPAttribute());

	ASC->SetNumericAttributeBase(UNUVitalAttributeSet::GetSPAttribute(),
		FMath::Min(CurrentSP + static_cast<float>(Previous->SPSpend), MaxSP));
}

void ANUBattleEncounter::RefillSPAfterAction(int32 ActionIndex)
{
	if (!ActionQueue.IsValidIndex(ActionIndex))
	{
		return;
	}

	const FNUBattleCommandRequest& Command = ActionQueue[ActionIndex].Command;

	// 남은 SP 를 전부 쓴 행동은 이 라운드에 SP 가 다시 차지 않음 (3203 나이아 개화 — 그 턴 방어 불가)
	// 다음 명령 선택이 열릴 때 BeginCommandSelection 이 채움
	const FNUSpendAllSPSkill* SpendAll = NUJobSkill::FindSpendAllSPSkill(Command.SkillId);
	if (SpendAll && SpendAll->bNoRefillThisRound)
	{
		DeferredSPRefillIds.Add(Command.SourceCombatantId);
		return;
	}

	// NextAction 처리중에 직전에 행동한 사람의 Index가 들어오기 때문에
	// 행동한 직후 차는것과 같음
	RefillSP(Command.SourceCombatantId);
}

void ANUBattleEncounter::RefillSPForAll()
{
	for (const FNUBattleParticipant& Participant : Participants)
	{
		RefillSP(Participant.CombatantId);
	}
}

void ANUBattleEncounter::RefillSP(int32 CombatantId)
{
	if (!HasAuthority() || !IsAlive(CombatantId))
	{
		return;
	}

	// 적은 SP를 소모하지 않아 제외함
	// 적 스킬에 SP 비용이 생기면 이 제외를 먼저 풀어야 함
	const FNUBattleParticipant* Participant = FindParticipant(CombatantId);
	if (!Participant || Participant->Team == ENUBattleTeam::Enemy)
	{
		return;
	}

	UAbilitySystemComponent* ASC =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Participant->ASCOwner);
	if (!IsValid(ASC))
	{
		return;
	}

	float MaxSP = ASC->GetNumericAttribute(UNUVitalAttributeSet::GetMaxSPAttribute());
	ASC->SetNumericAttributeBase(UNUVitalAttributeSet::GetSPAttribute(), MaxSP);
}

void ANUBattleEncounter::HandleAbilityEnded(const FAbilityEndedData& EndedData)
{
	if (EndedData.AbilitySpecHandle != ActiveAbilityHandle)
	{
		return;
	}
	
	UnbindAbilityEnded();
	ScheduleNextAction();
}

void ANUBattleEncounter::UnbindAbilityEnded()
{
	if (ActiveAbilityASC.IsValid() && AbilityEndedHandle.IsValid())
	{
		ActiveAbilityASC->OnAbilityEnded.Remove(AbilityEndedHandle);
	}
	
	AbilityEndedHandle.Reset();
	ActiveAbilityASC.Reset();
	ActiveAbilityHandle = FGameplayAbilitySpecHandle();
	GetWorldTimerManager().ClearTimer(ActionTimeoutHandle);
}

void ANUBattleEncounter::HandleActionTimeout()
{
	UE_LOG(LogTemp, Error, TEXT("행동이 %f초 안에 끝나지 않아 완료 대기를 중단합니다!"), MaxActionSeconds);
	
	UnbindAbilityEnded();
	ScheduleNextAction();
}
