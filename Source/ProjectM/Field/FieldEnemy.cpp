#include "Field/FieldEnemy.h"

#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Core/NUGameInstance.h"
#include "Field/Encounter/NUSymbolEncounterComponent.h"
#include "Field/FieldPlayerCharacter.h"
#include "Interaction/Component/NUInteractorComponent.h"
#include "Field/NUSpriteVisualComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Hash/CityHash.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "PaperFlipbookComponent.h"
#include "PaperZDAnimationComponent.h"
#include "PaperZDAnimInstance.h"
#include "TimerManager.h"

namespace
{
	// 이보다 빠르면 걸어서 움직이는 중으로 봄, 멈춘 채 나타났는지와 보호 시간을 셀지를 가름
	constexpr float ProtectionMoveSpeed = 10.f;

	// 보호 시간은 한 프레임에 이만큼까지만 셈, 렉으로 긴 프레임이 와도 보호가 한 번에 깎이지 않게
	constexpr float ProtectionMaxStep = 0.1f;

	// 보호 중 깜빡임 한 주기 (흐려졌다 돌아옴), 가장 흐릴 때의 불투명도
	constexpr float ProtectionBlinkPeriod = 1.f;
	constexpr float ProtectionBlinkMinOpacity = 0.25f;

	bool IsPlayerMoving(const AFieldPlayerCharacter* Player)
	{
		return Player->GetVelocity().SizeSquared2D() > FMath::Square(ProtectionMoveSpeed);
	}
}

AFieldEnemy::AFieldEnemy()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);

	GetCapsuleComponent()->InitCapsuleSize(30.0f, 40.0f);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	bUseControllerRotationYaw = false;

	// 추격하지 않으므로 레벨에 배치한 방향을 유지
	GetCharacterMovement()->bOrientRotationToMovement = false;

	EncounterTrigger = CreateDefaultSubobject<USphereComponent>(TEXT("EncounterTrigger"));
	EncounterTrigger->SetupAttachment(GetCapsuleComponent());
	EncounterTrigger->InitSphereRadius(120.0f);
	EncounterTrigger->SetCollisionProfileName(TEXT("NU_Interactable"));
	EncounterTrigger->SetGenerateOverlapEvents(true);

	Sprite = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("Sprite"));
	Sprite->SetupAttachment(GetCapsuleComponent());
	Sprite->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// 발밑 피벗은 SpriteVisual이 정한다 (MakeFieldEnemy)

	Animation = CreateDefaultSubobject<UPaperZDAnimationComponent>(TEXT("Animation"));
	Animation->InitRenderComponent(Sprite);

	// 머티리얼·애님BP·빌보드 담당.
	// 캡슐이 레벨 배치 방향으로 회전해 있어도 빌보드가 유지되도록
	// MakeFieldEnemy()가 bUseAbsoluteRotation을 켠다
	SpriteVisual = CreateDefaultSubobject<UNUSpriteVisualComponent>(TEXT("SpriteVisual"));
	SpriteVisual->Setup(Sprite, Animation, NUSpriteVisuals::MakeFieldEnemy());

	ProtectionBlinkMaterial = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(TEXT("/Paper2D/TranslucentUnlitSpriteMaterial.TranslucentUnlitSpriteMaterial")));
}

void AFieldEnemy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 심볼 상태 복제
	DOREPLIFETIME(ThisClass, EncounterState);
}

void AFieldEnemy::BeginPlay()
{
	Super::BeginPlay();

	EncounterTrigger->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleEncounterTriggerBeginOverlap);

	// 스프라이트 표현 적용은 SpriteVisual의 BeginPlay가 담당하므로
	// 기존 스프라이트 적용은 제거함

	// 첫 번째 적의 나이아 그림을 입힘 (퀘스트 나이아 액터와 같은 방식), SpriteVisual 이 먼저 입힌 기본 그림(슬라임)을 덮음
	// 적 ID 는 나이아 표 행 이름이라 곧 NaiaId 숫자, 정의나 애님 BP가 없으면 기본 그림으로 섬
	if (GetNetMode() != NM_DedicatedServer)
	{
		const int32 NaiaId = EnemyPartyIds.IsEmpty() ? 0 : FCString::Atoi(*EnemyPartyIds[0].ToString());
		if (NaiaId <= 0 || !SpriteVisual->ApplyNaiaAnimBlueprint(NaiaId))
		{
			UE_LOG(LogTemp, Warning, TEXT("FieldEnemy: 나이아 그림을 적용하지 못해 기본 그림으로 섭니다 [%s] Enemy=%s"),
				*GetName(), EnemyPartyIds.IsEmpty() ? TEXT("없음") : *EnemyPartyIds[0].ToString());
		}

		// 보호가 걸리는 자동 심볼만 깜빡임 머티리얼을 미리 불러 둠
		if (TriggerMode == ENUSymbolEncounterTriggerMode::Automatic && ReentryGraceSeconds > 0.0f
			&& !ProtectionBlinkMaterial.IsNull())
		{
			LoadedBlinkMaterial = ProtectionBlinkMaterial.LoadSynchronous();
		}
	}

	// 이전에 격파한 심볼이면 제거.
	// 복제되는 액터이므로 서버만 파괴해야 한다 (파괴는 클라로 복제된다)
	if (!HasAuthority()) return;

	// 이미 처치된 보스는 제거
	if (DefeatPolicy == ENUFieldEnemyDefeatPolicy::RemoveForSession)
	{
		const UNUGameInstance* GameInstance = Cast<UNUGameInstance>(UGameplayStatics::GetGameInstance(this));

		if (IsValid(GameInstance) && GameInstance->IsFieldEnemyDefeated(GetPersistentEnemyId()))
		{
			Destroy();
			return;
		}
	}

	ApplyEncounterState();
}

void AFieldEnemy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	SpriteVisual->UpdateBillboard();
	TickProtection(DeltaSeconds);
}

bool AFieldEnemy::CanInteract_Implementation(AActor* Interactor) const
{
	// 수동 심볼이 대기 상태일 때만 상호작용 허용
	return TriggerMode == ENUSymbolEncounterTriggerMode::Interaction
		&& EncounterState == ENUFieldEnemyState::Idle
		&& IsValid(Cast<AFieldPlayerCharacter>(Interactor));
}

bool AFieldEnemy::Interact_Implementation(AActor* Interactor)
{
	if (!CanInteract_Implementation(Interactor))
	{
		return false;
	}

	return TryStartEncounter(Cast<AFieldPlayerCharacter>(Interactor));
}

void AFieldEnemy::HandleSymbolEncounterStarted_Implementation(AActor* Initiator)
{
	if (!HasAuthority())
	{
		return;
	}

	// 전투 시작 상태로 변경
	GetWorldTimerManager().ClearTimer(LifecycleTimer);
	ClearProtection();
	StopWaitingForReturn();
	EncounterPlayer = Cast<AFieldPlayerCharacter>(Initiator);
	SetEncounterState(ENUFieldEnemyState::InEncounter);
}

void AFieldEnemy::HandleSymbolEncounterCompleted_Implementation(ENUBattleResult Result)
{
	if (!HasAuthority())
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(LifecycleTimer);

	// 승리 시 제거 or 리스폰
	if (Result == ENUBattleResult::Victory)
	{
		if (DefeatPolicy == ENUFieldEnemyDefeatPolicy::RemoveForSession)
		{
			if (UNUGameInstance* GameInstance =
				Cast<UNUGameInstance>(UGameplayStatics::GetGameInstance(this)))
			{
				GameInstance->MarkFieldEnemyDefeated(GetPersistentEnemyId());
			}

			SetEncounterState(ENUFieldEnemyState::Defeated);
			Destroy();
			return;
		}

		// 패배 or 도주 시 다시 활성화
		SetEncounterState(ENUFieldEnemyState::Respawning);

		if (RespawnDelay <= 0.0f)
		{
			FinishRespawn();
		}
		else
		{
			GetWorldTimerManager().SetTimer(
				LifecycleTimer,
				this,
				&ThisClass::FinishRespawn,
				RespawnDelay,
				false);
		}
		return;
	}

	SetEncounterState(ENUFieldEnemyState::Cooldown);

	// 도망치면 필드 캐릭터가 심볼 위 그 자리로 돌아오므로, 돌아온 뒤 빠져나갈 시간을 줌
	WaitForReturn();

	if (RetryCooldown <= 0.0f)
	{
		FinishRetryCooldown();
	}
	else
	{
		GetWorldTimerManager().SetTimer(
			LifecycleTimer,
			this,
			&ThisClass::FinishRetryCooldown,
			RetryCooldown,
			false);
	}
}

void AFieldEnemy::HandleEncounterTriggerBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!HasAuthority()
		|| TriggerMode != ENUSymbolEncounterTriggerMode::Automatic
		|| EncounterState != ENUFieldEnemyState::Idle)
	{
		return;
	}

	// InteractionComponent가 없는 플레이어만 감지
	AFieldPlayerCharacter* FieldPlayer = Cast<AFieldPlayerCharacter>(OtherActor);
	if (!IsValid(FieldPlayer)
		|| OtherComponent != FieldPlayer->GetCapsuleComponent())
	{
		return;
	}

	// 이미 보호 중이면 TickProtection 이 움직인 시간을 셈
	if (bProtecting && FieldPlayer == ProtectedPlayer.Get())
	{
		return;
	}

	// 걸어 들어온 게 아니라 범위 안에 나타난 것이면 보호부터
	// 전투에서 돌아온 플레이어, 멈춘 채 놓인 플레이어(접속 · 포탈 · 부활 · 심볼 재등장)
	const bool bReturned = bWaitingForReturn && FieldPlayer == EncounterPlayer.Get();
	if (ReentryGraceSeconds > 0.0f && (bReturned || !IsPlayerMoving(FieldPlayer)))
	{
		StartProtection(FieldPlayer);
		return;
	}

	TryStartEncounter(FieldPlayer);
}

bool AFieldEnemy::TryStartEncounter(AFieldPlayerCharacter* FieldPlayer)
{
	if (!HasAuthority()
		|| !IsValid(FieldPlayer)
		|| EncounterState != ENUFieldEnemyState::Idle
		|| !IsValid(FieldPlayer->SymbolEncounterComponent))
	{
		return false;
	}

	// Enemy ID를 배틀 요청 형식으로 변환
	TArray<FNUEnemySpawnSpec> Enemies;
	Enemies.Reserve(EnemyPartyIds.Num());

	for (const FName EnemyId : EnemyPartyIds)
	{
		if (EnemyId.IsNone())
		{
			return false;
		}

		FNUEnemySpawnSpec& Enemy = Enemies.AddDefaulted_GetRef();
		Enemy.EnemyId = EnemyId;
	}

	if (Enemies.IsEmpty())
	{
		return false;
	}

	SetEncounterState(ENUFieldEnemyState::Starting);

	const ENUBattleEncounterStartResult StartResult =
		FieldPlayer->SymbolEncounterComponent->TryStartFieldEvent(
			Enemies,
			EncounterClass,
			ENUBattleEncounterSource::Symbol,
			this,
			BattleStageLevel
		);

	// 전투 시작 실패 시 대기 상태
	if (StartResult != ENUBattleEncounterStartResult::Started)
	{
		UE_LOG(LogTemp, Warning, TEXT("심볼 인카운터 실패. Source=%s Result=%s"),
			*GetName(),
			*UEnum::GetValueAsString(StartResult)
		);

		SetEncounterState(ENUFieldEnemyState::Idle);
		return false;
	}

	return true;
}

void AFieldEnemy::SetEncounterState(ENUFieldEnemyState NewState)
{
	if (!HasAuthority() || EncounterState == NewState)
	{
		return;
	}

	EncounterState = NewState;
	ApplyEncounterState();
	ForceNetUpdate();
}

void AFieldEnemy::ApplyEncounterState()
{
	// 숨김
	const bool bShouldHide =
		EncounterState == ENUFieldEnemyState::InEncounter
		|| EncounterState == ENUFieldEnemyState::Respawning
		|| EncounterState == ENUFieldEnemyState::Defeated;

	SetActorHiddenInGame(bShouldHide);
	SetActorEnableCollision(!bShouldHide);

	if (IsValid(EncounterTrigger))
	{
		// 대기 상태 or 쿨다운 상태 감지 범위 활성화
		const bool bDetectionEnabled =
			EncounterState == ENUFieldEnemyState::Idle
			|| EncounterState == ENUFieldEnemyState::Cooldown;

		EncounterTrigger->SetCollisionEnabled(
			bDetectionEnabled
				? ECollisionEnabled::QueryOnly
				: ECollisionEnabled::NoCollision
		);
	}

	if (EncounterState == ENUFieldEnemyState::Idle)
	{
		RefreshNearbyInteractionSelection();
	}
}

void AFieldEnemy::FinishRetryCooldown()
{
	SetEncounterState(ENUFieldEnemyState::Idle);
}

void AFieldEnemy::FinishRespawn()
{
	// 심볼 재등장
	SetEncounterState(ENUFieldEnemyState::Idle);
}

void AFieldEnemy::WaitForReturn()
{
	StopWaitingForReturn();

	// 상호작용 심볼은 플레이어가 직접 눌러야 시작하므로 보호가 필요 없음
	if (TriggerMode != ENUSymbolEncounterTriggerMode::Automatic
		|| ReentryGraceSeconds <= 0.0f
		|| !EncounterPlayer.IsValid())
	{
		return;
	}

	bWaitingForReturn = true;

	// 전투 화면이 내려가는 시간은 연출 · 페이드에 따라 달라서 돌아오는 순간을 지켜봄
	GetWorldTimerManager().SetTimer(
		ReturnWatchTimer,
		this,
		&ThisClass::WatchReturn,
		0.2f,
		true);
}

void AFieldEnemy::WatchReturn()
{
	AFieldPlayerCharacter* Player = EncounterPlayer.Get();
	if (!IsValid(Player))
	{
		StopWaitingForReturn();
		return;
	}

	// 전투 동안은 필드 캐릭터의 충돌이 꺼져 있고 필드로 돌아올 때 켜짐
	if (!Player->GetActorEnableCollision())
	{
		return;
	}

	StopWaitingForReturn();

	// 범위 밖으로 돌아왔으면(진 뒤 부활 거점 등) 보호할 것이 없음, 겹침 이벤트로 이미 보호 중이면 그대로
	if (EncounterTrigger->IsOverlappingActor(Player) && !bProtecting)
	{
		StartProtection(Player);
	}
}

void AFieldEnemy::StopWaitingForReturn()
{
	bWaitingForReturn = false;
	GetWorldTimerManager().ClearTimer(ReturnWatchTimer);
}

void AFieldEnemy::StartProtection(AFieldPlayerCharacter* Player)
{
	StopWaitingForReturn();

	ProtectedPlayer = Player;
	bProtecting = true;
	ProtectedMoveSeconds = 0.f;
	BlinkElapsed = 0.f;

	// 마스크드 그림은 알파를 반만 넘겨도 끊겨서, 보호 중에만 반투명 머티리얼로 바꿔 흐리게 함
	if (IsValid(LoadedBlinkMaterial))
	{
		SpriteVisual->OverrideMaterial(LoadedBlinkMaterial);
	}
}

void AFieldEnemy::TickProtection(float DeltaSeconds)
{
	if (!bProtecting)
	{
		return;
	}

	// 범위를 벗어나면 보호 끝, 다시 걸어 들어오면 평소대로 바로 전투
	AFieldPlayerCharacter* Player = ProtectedPlayer.Get();
	if (!IsValid(Player) || !EncounterTrigger->IsOverlappingActor(Player))
	{
		ClearProtection();
		return;
	}

	// 코사인으로 흐려졌다 돌아옴, 시작은 원래 밝기라 보호가 걸리는 순간 튀지 않음
	if (IsValid(LoadedBlinkMaterial))
	{
		BlinkElapsed += DeltaSeconds;
		const float Wave = 0.5f + 0.5f * FMath::Cos(UE_TWO_PI * BlinkElapsed / ProtectionBlinkPeriod);
		SpriteVisual->SetOpacity(FMath::Lerp(ProtectionBlinkMinOpacity, 1.f, Wave));
	}

	// 멈춰 있는 동안은 보호, 움직인 시간만 셈 - 로딩 · 페이드처럼 조작 못 하는 시간은 안 깎임
	if (IsPlayerMoving(Player))
	{
		ProtectedMoveSeconds += FMath::Min(DeltaSeconds, ProtectionMaxStep);
	}

	if (ProtectedMoveSeconds < ReentryGraceSeconds)
	{
		return;
	}

	// 보호 시간만큼 움직이고도 범위 안이면 다시 닿은 것으로 봄, 겹침 이벤트는 이미 지나가 다시 안 옴
	ClearProtection();

	if (EncounterState == ENUFieldEnemyState::Idle)
	{
		TryStartEncounter(Player);
	}
}

void AFieldEnemy::ClearProtection()
{
	// 흐린 채 끝나지 않게 불투명도와 머티리얼을 되돌림, 전투 · 리스폰 숨김은 액터 단위라 겹치지 않음
	if (bProtecting && IsValid(LoadedBlinkMaterial))
	{
		SpriteVisual->SetOpacity(1.f);
		SpriteVisual->OverrideMaterial(nullptr);
	}

	ProtectedPlayer.Reset();
	bProtecting = false;
	ProtectedMoveSeconds = 0.f;
	BlinkElapsed = 0.f;
}

void AFieldEnemy::RefreshNearbyInteractionSelection() const
{
	if (!IsValid(EncounterTrigger))
	{
		return;
	}

	TArray<AActor*> OverlappingPlayers;
	EncounterTrigger->GetOverlappingActors(
		OverlappingPlayers,
		AFieldPlayerCharacter::StaticClass());

	for (AActor* Actor : OverlappingPlayers)
	{
		const AFieldPlayerCharacter* FieldPlayer = Cast<AFieldPlayerCharacter>(Actor);

		if (IsValid(FieldPlayer) && IsValid(FieldPlayer->InteractionComponent))
		{
			FieldPlayer->InteractionComponent->UpdateCurrentInteractable();
		}
	}
}

FName AFieldEnemy::GetPersistentEnemyId() const
{
	return FieldEnemyId.IsNone() ? GetFName() : FieldEnemyId;
}

int64 AFieldEnemy::GetSymbolId() const
{
	// FName 해시는 실행마다 달라서 글자로 만듦, 서브레벨로 붙은 사냥터도 그 레벨 패키지 이름이 같음
	const ULevel* Level = GetLevel();
	const FString LevelName = IsValid(Level) ? UWorld::RemovePIEPrefix(Level->GetPackage()->GetName()) : FString();
	const FTCHARToUTF8 Key(*FString::Printf(TEXT("%s:%s"), *LevelName, *GetPersistentEnemyId().ToString()));

	const int64 SymbolId = static_cast<int64>(CityHash64(Key.Get(), Key.Length()) & static_cast<uint64>(MAX_int64));

	return SymbolId != 0 ? SymbolId : 1;
}

void AFieldEnemy::OnRep_EncounterState()
{
	ApplyEncounterState();
}
