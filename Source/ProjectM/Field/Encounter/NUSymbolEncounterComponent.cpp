#include "Field/Encounter/NUSymbolEncounterComponent.h"

#include "Audio/NUSFXSubsystem.h"
#include "Audio/NUSFXTags.h"
#include "Battle/Encounter/NUBattleEncounter.h"
#include "Battle/Encounter/NUBattleEncounterSubsystem.h"
#include "Battle/Net/NUBattleNetStageSubsystem.h"
#include "Battle/Net/NUBattleNetSubsystem.h"
#include "CharacterState/NUPlayerState.h"
#include "Field/Encounter/NUSymbolEncounterSourceInterface.h"
#include "Field/FieldEnemy.h"
#include "Field/FieldPlayerCharacter.h"
#include "EngineUtils.h"
#include "Net/EmberServerManager.h"
#include "NUGAS/NUAbilitySystemComponent.h"
#include "WorldLocation/World/NUEncounterZone.h"

UNUSymbolEncounterComponent::UNUSymbolEncounterComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

ENUBattleEncounterStartResult UNUSymbolEncounterComponent::TryStartFieldEvent(const TArray<FNUEnemySpawnSpec>& Enemies,
	TSoftClassPtr<ANUBattleEncounter> EncounterClass, ENUBattleEncounterSource Source, AActor* SourceActor,
	TSoftObjectPtr<UWorld> BattleStageLevel)
{
	// 전투 정보 검사
	const ENUBattleEncounterStartResult ValidationResult = ValidateFieldEvent(Enemies, EncounterClass, Source, SourceActor);

	if (ValidationResult != ENUBattleEncounterStartResult::Started)
	{
		return ValidationResult;
	}

	AFieldPlayerCharacter* FieldCharacter = CastChecked<AFieldPlayerCharacter>(GetOwner());
	ANUPlayerState* PlayerState = FieldCharacter->GetPlayerState<ANUPlayerState>();

	// 서버에 로그인돼 있으면 서버 전투로, 아니면 기존 로컬 전투로 - 로컬 경로는 검증·디버깅용으로 남겨둠
	// 서버 전투의 조우음은 서버가 시작을 받아 줄 때(UNUBattleNetSubsystem::HandleAck) 낸다
	if (TryStartServerBattle(Enemies, SourceActor, BattleStageLevel))
	{
		return ENUBattleEncounterStartResult::Started;
	}

	// 온라인 단일 대상 요청이 막혔다면 로컬 전투로 우회하지 않는다.
	const UGameInstance* GI = GetWorld()->GetGameInstance();
	const UEmberServerManager* Server = GI ? GI->GetSubsystem<UEmberServerManager>() : nullptr;
	if (Enemies.Num() == 1 && Server && Server->IsLoggedIn())
	{
		return ENUBattleEncounterStartResult::InvalidRequest;
	}

	// 배틀 시스템의 Request 형식으로 반환
	FNUBattleEncounterRequest Request;
	Request.Initiator = PlayerState;
	Request.Enemies = Enemies;
	Request.EncounterClass = EncounterClass;
	Request.Source = Source;
	Request.SourceActor = SourceActor;

	// 전투가 벌어질 아레나는 서브시스템이 정하고, 요청은 그 위에 올릴 배경만 넘긴다
	Request.BattleStageLevel = BattleStageLevel;

	UWorld* World = GetWorld();
	UNUBattleEncounterSubsystem* EncounterSubsystem =
		IsValid(World) 
		? World->GetSubsystem<UNUBattleEncounterSubsystem>() 
		: nullptr;

	if (!IsValid(EncounterSubsystem))
	{
		return ENUBattleEncounterStartResult::SpawnFailed;
	}

	ANUBattleEncounter* NewEncounter = nullptr;
	const ENUBattleEncounterStartResult StartResult = EncounterSubsystem->TryStartEncounter(Request, NewEncounter);

	if (StartResult != ENUBattleEncounterStartResult::Started || !IsValid(NewEncounter))
	{
		return StartResult == 
			ENUBattleEncounterStartResult::Started
			? ENUBattleEncounterStartResult::SpawnFailed
			: StartResult;
	}

	// Encounter 원인 필드 심볼 추적
	ClearActiveEncounter();
	ActiveEncounter = NewEncounter;
	ActiveSourceActor = SourceActor;

	// 전투 종료 시 필드 심볼 결과를 돌려주기 위한 델리게이트
	NewEncounter->OnEncounterCompleted.AddUObject(this, &ThisClass::HandleEncounterCompleted);

	if (IsValid(SourceActor) && SourceActor->GetClass()->ImplementsInterface(UNUSymbolEncounterSourceInterface::StaticClass()))
	{
		INUSymbolEncounterSourceInterface::Execute_HandleSymbolEncounterStarted(SourceActor, FieldCharacter);
	}

	UNUSFXSubsystem::Play(this, NU::SFX::Field::Encounter);
	return ENUBattleEncounterStartResult::Started;
}

ANUBattleEncounter* UNUSymbolEncounterComponent::GetActiveEncounter() const
{
	return ActiveEncounter.Get();
}

AActor* UNUSymbolEncounterComponent::GetActiveSourceActor() const
{
	return ActiveSourceActor.Get();
}

void UNUSymbolEncounterComponent::BeginPlay()
{
	Super::BeginPlay();

	// 빙의는 BeginPlay 뒤라 여기서 로컬 캐릭터인지 못 가림, 받는 쪽에서 가림
	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (UNUBattleNetSubsystem* BattleNet = GameInstance ? GameInstance->GetSubsystem<UNUBattleNetSubsystem>() : nullptr)
	{
		BattleNet->OnPartyBattleJoined.AddUObject(this, &ThisClass::HandlePartyBattleJoined);
	}
}

void UNUSymbolEncounterComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearActiveEncounter();

	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (UNUBattleNetSubsystem* BattleNet = GameInstance ? GameInstance->GetSubsystem<UNUBattleNetSubsystem>() : nullptr)
	{
		BattleNet->OnPartyBattleJoined.RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
}

ENUBattleEncounterStartResult UNUSymbolEncounterComponent::ValidateFieldEvent(const TArray<FNUEnemySpawnSpec>& Enemies, TSoftClassPtr<ANUBattleEncounter> EncounterClass, ENUBattleEncounterSource Source, AActor* SourceActor) const
{
	const AFieldPlayerCharacter* FieldCharacter = Cast<AFieldPlayerCharacter>(GetOwner());
	if (!IsValid(FieldCharacter) || !FieldCharacter->HasAuthority())
	{
		return ENUBattleEncounterStartResult::NotAuthority;
	}

	const UGameInstance* GI = GetWorld()->GetGameInstance();
	const UNUBattleNetSubsystem* Net = GI ? GI->GetSubsystem<UNUBattleNetSubsystem>() : nullptr;
	const UNUBattleNetStageSubsystem* Stage = GetWorld()->GetSubsystem<UNUBattleNetStageSubsystem>();
	// 서버 요청 대기 중 로컬 전투로 우회하거나 화면 복귀 도중 재진입하지 않는다.
	const bool bAlreadyInBattle = ActiveEncounter.IsValid() || bServerBattleActive
		|| (Net && (Net->IsInBattle() || Net->IsBattleStartPending())) || (Stage && Stage->IsTransitionActive());
	
	if (bAlreadyInBattle || Enemies.IsEmpty() || EncounterClass.IsNull())
	{
		return bAlreadyInBattle
			? ENUBattleEncounterStartResult::AlreadyInBattle
			: ENUBattleEncounterStartResult::InvalidRequest;
	}

	for (const FNUEnemySpawnSpec& Enemy : Enemies)
	{
		if (!Enemy.IsValid())
		{
			return ENUBattleEncounterStartResult::InvalidRequest;
		}
	}

	// 심볼 인카운터는 SourceActor와 배틀 결과를 받을 인터페이스 필요
	if ((Source == ENUBattleEncounterSource::Random && IsValid(SourceActor))
		|| (Source == ENUBattleEncounterSource::Symbol
			&& (!IsValid(SourceActor) || !SourceActor->GetClass()->ImplementsInterface(UNUSymbolEncounterSourceInterface::StaticClass()))))
	{
		return ENUBattleEncounterStartResult::InvalidRequest;
	}

	const ANUPlayerState* PlayerState = FieldCharacter->GetPlayerState<ANUPlayerState>();
	if (!IsValid(PlayerState))
	{
		return ENUBattleEncounterStartResult::NoValidParticipants;
	}

	// 전투 참가 중인지 검사
	if (IsValid(PlayerState->GetBattleEncounter()))
	{
		return ENUBattleEncounterStartResult::AlreadyInBattle;
	}

	const UNUAbilitySystemComponent* ASC = PlayerState->GetNUASC();
	if (!IsValid(ASC) || !ASC->IsAlive())
	{
		return ENUBattleEncounterStartResult::NoValidParticipants;
	}

	return ENUBattleEncounterStartResult::Started;
}

void UNUSymbolEncounterComponent::HandleEncounterCompleted(ANUBattleEncounter* Encounter, ENUBattleResult Result)
{
	if (Encounter != ActiveEncounter.Get())
	{
		return;
	}

	TWeakObjectPtr<AActor> CompletedSource = ActiveSourceActor;
	ClearActiveEncounter();

	// 필드 심볼에 승리, 패배, 도망 결과 전달
	AActor* SourceActor = CompletedSource.Get();
	if (IsValid(SourceActor) && SourceActor->GetClass()->ImplementsInterface(UNUSymbolEncounterSourceInterface::StaticClass()))
	{
		INUSymbolEncounterSourceInterface::Execute_HandleSymbolEncounterCompleted(SourceActor, Result);
	}
}

void UNUSymbolEncounterComponent::ClearActiveEncounter()
{
	if (ANUBattleEncounter* Encounter = ActiveEncounter.Get())
	{
		Encounter->OnEncounterCompleted.RemoveAll(this);
	}

	if (bServerBattleActive)
	{
		UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
		if (UNUBattleNetSubsystem* BattleNet = GameInstance ? GameInstance->GetSubsystem<UNUBattleNetSubsystem>() : nullptr)
		{
			BattleNet->OnBattleEnded.RemoveAll(this);
			BattleNet->OnBattleStateChanged.RemoveAll(this);
		}
	}

	bServerBattleActive = false;
	ActiveEncounter.Reset();
	ActiveSourceActor.Reset();
}

bool UNUSymbolEncounterComponent::TryStartServerBattle(const TArray<FNUEnemySpawnSpec>& Enemies,
	AActor* SourceActor, const TSoftObjectPtr<UWorld>& BattleStageLevel)
{
	// 현재 넷 전투는 야생 나이아 한 마리만 받는 프로토콜이라 여럿이면 로컬 전투로 전환함
	if (Enemies.Num() != 1)
	{
		return false;
	}

	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (!IsValid(GameInstance))
	{
		return false;
	}

	UEmberServerManager* Server = GameInstance->GetSubsystem<UEmberServerManager>();
	if (!IsValid(Server) || !Server->IsLoggedIn())
	{
		return false;
	}

	UNUBattleNetSubsystem* BattleNet = GameInstance->GetSubsystem<UNUBattleNetSubsystem>();
	if (!IsValid(BattleNet) || BattleNet->IsInBattle() || BattleNet->IsBattleStartPending())
	{
		return false;
	}

	// EnemyId(FName)는 로컬 나이아 테이블의 RowName인데, 그 값 자체가 항상 NaiaId 숫자 문자열임
	// 서버는 숫자만 있으면 되므로 로컬 테이블 조회를 거치지 않고 바로 사용 가능
	const int32 NaiaId = FCString::Atoi(*Enemies[0].EnemyId.ToString());
	if (NaiaId <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("서버 전투 시작 실패 : EnemyId를 NaiaId로 해석할 수 없음 [%s]"),
			*Enemies[0].EnemyId.ToString());
		return false;
	}

	// 필드 심볼이면 심볼 전투, 서버가 같은 공간의 파티원을 같이 넣음 (랜덤 인카운터는 SourceActor 가 없음)
	const AFieldEnemy* Symbol = Cast<AFieldEnemy>(SourceActor);
	const int32 EncounterKind = IsValid(Symbol) ? 1 : 0;
	const int64 SymbolId = IsValid(Symbol) ? Symbol->GetSymbolId() : 0;

	BattleNet->RequestBattleStart(NaiaId, EncounterKind, SymbolId);
	if (!BattleNet->IsBattleStartPending())
	{
		return false;
	}

	// 이번 요청이 시작 대기를 확보했을 때만 배경과 심볼을 설정한다.
	if (UNUBattleNetStageSubsystem* Stage = GetWorld()->GetSubsystem<UNUBattleNetStageSubsystem>())
	{
		Stage->SetPendingStageLevel(BattleStageLevel);
	}

	TrackServerBattle(BattleNet, SourceActor);

	return true;
}

void UNUSymbolEncounterComponent::TrackServerBattle(UNUBattleNetSubsystem* BattleNet, AActor* SourceActor)
{
	ClearActiveEncounter();
	bServerBattleActive = true;
	ActiveSourceActor = SourceActor;

	BattleNet->OnBattleEnded.AddUObject(this, &ThisClass::HandleServerBattleEnded);
	BattleNet->OnBattleStateChanged.AddUObject(this, &ThisClass::HandleServerBattleStateChanged);

	if (IsValid(SourceActor) && SourceActor->GetClass()->ImplementsInterface(UNUSymbolEncounterSourceInterface::StaticClass()))
	{
		AFieldPlayerCharacter* FieldCharacter = CastChecked<AFieldPlayerCharacter>(GetOwner());
		INUSymbolEncounterSourceInterface::Execute_HandleSymbolEncounterStarted(SourceActor, FieldCharacter);
	}
}

void UNUSymbolEncounterComponent::HandlePartyBattleJoined(int64 SymbolId)
{
	AFieldPlayerCharacter* FieldCharacter = Cast<AFieldPlayerCharacter>(GetOwner());
	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UNUBattleNetSubsystem* BattleNet = GameInstance ? GameInstance->GetSubsystem<UNUBattleNetSubsystem>() : nullptr;
	if (!IsValid(FieldCharacter) || !FieldCharacter->IsLocallyControlled() || !IsValid(BattleNet))
	{
		return;
	}

	// 같은 공간이면 같은 레벨이 열려 있어 같은 번호의 심볼이 있음
	AFieldEnemy* Symbol = nullptr;
	for (TActorIterator<AFieldEnemy> It(GetWorld()); It; ++It)
	{
		if (It->GetSymbolId() == SymbolId)
		{
			Symbol = *It;
			break;
		}
	}

	// 심볼이 없으면 서 있는 존의 배경, 그것도 없으면 배경을 바꾸지 않음
	TSoftObjectPtr<UWorld> StageLevel = IsValid(Symbol) ? Symbol->BattleStageLevel : TSoftObjectPtr<UWorld>();
	if (!IsValid(Symbol))
	{
		TArray<AActor*> OverlappingZones;
		FieldCharacter->GetOverlappingActors(OverlappingZones, ANUEncounterZone::StaticClass());
		for (const AActor* Actor : OverlappingZones)
		{
			const ANUEncounterZone* Zone = Cast<ANUEncounterZone>(Actor);
			if (IsValid(Zone) && !Zone->GetBattleStageLevel().IsNull())
			{
				StageLevel = Zone->GetBattleStageLevel();
				break;
			}
		}

		UE_LOG(LogTemp, Warning, TEXT("파티 심볼 전투: 이 월드에 심볼 %lld 이 없어 서 있는 존의 배경으로 들어감 (배경=%s)"),
			SymbolId, StageLevel.IsNull() ? TEXT("없음") : *StageLevel.GetAssetName());
	}

	if (UNUBattleNetStageSubsystem* Stage = GetWorld()->GetSubsystem<UNUBattleNetStageSubsystem>())
	{
		Stage->SetPendingStageLevel(StageLevel);
	}

	// 같은 심볼에 같이 닿아 이미 잡고 있으면 그대로 둠
	if (bServerBattleActive && ActiveSourceActor.Get() == Symbol)
	{
		return;
	}

	// 다른 심볼로 시작 요청을 보내 둔 사이에 끌려 들어왔으면 그 심볼은 도주로 되돌림
	if (bServerBattleActive)
	{
		HandleServerBattleEnded(static_cast<int32>(ENUBattleResult::Fled));
	}

	if (IsValid(Symbol))
	{
		TrackServerBattle(BattleNet, Symbol);
	}
}

void UNUSymbolEncounterComponent::HandleServerBattleStateChanged()
{
	const UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UNUBattleNetSubsystem* Net = GI ? GI->GetSubsystem<UNUBattleNetSubsystem>() : nullptr;
	if (bServerBattleActive && Net && !Net->IsInBattle() && !Net->IsBattleStartPending())
	{
		// 시작 거절/연결 종료는 승리로 처리하지 않고 숨겨둔 심볼을 복구한다.
		HandleServerBattleEnded(static_cast<int32>(ENUBattleResult::Fled));
	}
}

void UNUSymbolEncounterComponent::HandleServerBattleEnded(int32 Reason)
{
	if (!bServerBattleActive)
	{
		return;
	}

	TWeakObjectPtr<AActor> CompletedSource = ActiveSourceActor;
	ClearActiveEncounter();

	AActor* SourceActor = CompletedSource.Get();
	if (IsValid(SourceActor) && SourceActor->GetClass()->ImplementsInterface(UNUSymbolEncounterSourceInterface::StaticClass()))
	{
		// SVR_Battle_End.reason: 1 승리 · 2 패배 · 3 도주 - ENUBattleResult(Victory=1/Defeat=2/Fled=3)와 그대로 대응함
		INUSymbolEncounterSourceInterface::Execute_HandleSymbolEncounterCompleted(SourceActor, static_cast<ENUBattleResult>(Reason));
	}
}
