#include "Field/Encounter/NURandomEncounterComponent.h"

#include "CharacterState/NUPlayerState.h"
#include "Battle/Encounter/NUBattleEncounterTypes.h"
#include "WorldLocation/World/NUEncounterZone.h"
#include "Field/Encounter/NUSymbolEncounterComponent.h"
#include "Field/FieldPlayerCharacter.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UNURandomEncounterComponent::UNURandomEncounterComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}


void UNURandomEncounterComponent::BeginPlay()
{
	Super::BeginPlay();

	SetComponentTickEnabled(bIsEncounterEnabled);

	if (bIsEncounterEnabled)
	{
		ResetEncounterProgress();
	}
}


void UNURandomEncounterComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		return;
	}

	const FVector CurrentLocation = Owner->GetActorLocation();

	// Encounter zone에서만 이동 거리 누적
	ANUEncounterZone* EncounterZone = FindCurrentEncounterZone();
	if (!CanAccumulateDistance() || !IsValid(EncounterZone))
	{
		PreviousLocation = CurrentLocation;

		// Zone에서 이동 거리는 누적 X
		if (!IsValid(EncounterZone))
		{
			MovedDistance = 0.0f;
		}

		return;
	}

	// 높이 변화 제외, 필드의 수평 이동거리만 계산
	FVector LocationDelta = CurrentLocation - PreviousLocation;
	LocationDelta.Z = 0.0f;

	MovedDistance += LocationDelta.Size();
	PreviousLocation = CurrentLocation;

	if (MovedDistance >= TargetDistance)
	{
		TryStartEncounter(*EncounterZone);
	}
}

// ---------------------------------------
// 인카운터 설정 및 제어
// ---------------------------------------

void UNURandomEncounterComponent::SetEncounterEnabled(bool bEnabled)
{
	bIsEncounterEnabled = bEnabled;
	SetComponentTickEnabled(bEnabled);

	if (bEnabled)
	{
		ResetEncounterProgress();
	}
}

void UNURandomEncounterComponent::ResetEncounterProgress()
{
	MovedDistance = 0.0f;

	if (const AActor* Owner = GetOwner())
	{
		PreviousLocation = Owner->GetActorLocation();
	}
	else
	{
		PreviousLocation = FVector::ZeroVector;
	}

	GenerateNextEncounterDistance();
}

// ---------------------------------------
// 이동 거리 처리 
// ---------------------------------------

ANUEncounterZone* UNURandomEncounterComponent::FindCurrentEncounterZone() const
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		return nullptr;
	}

	TArray<AActor*> OverlappingZones;
	Owner->GetOverlappingActors(OverlappingZones, ANUEncounterZone::StaticClass());

	for (AActor* OverlappingActor : OverlappingZones)
	{
		if (ANUEncounterZone* EncounterZone = Cast<ANUEncounterZone>(OverlappingActor))
		{
			return EncounterZone;
		}
	}

	return nullptr;
}

bool UNURandomEncounterComponent::CanAccumulateDistance() const
{
	if (!bIsEncounterEnabled)
	{
		return false;
	}

	const ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!IsValid(OwnerCharacter))
	{
		return false;
	}

	if (!OwnerCharacter->HasAuthority())
	{
		return false;
	}

	// 지면에서 이동 중일 때만 이동 거리 누적
	const UCharacterMovementComponent* MovementComponent = OwnerCharacter->GetCharacterMovement();
	if (!MovementComponent || !MovementComponent->IsMovingOnGround())
	{
		return false;
	}

	const ANUPlayerState* PlayerState = OwnerCharacter->GetPlayerState<ANUPlayerState>();
	if (!IsValid(PlayerState))
	{
		return false;
	}

	// 전투 중일 경우 누적 X
	if (PlayerState->GetBattleEncounter())
	{
		return false;
	}

	return true;
}

bool UNURandomEncounterComponent::TryStartEncounter(const ANUEncounterZone& EncounterZone)
{
	AFieldPlayerCharacter* OwnerCharacter = Cast<AFieldPlayerCharacter>(GetOwner());
	if (!IsValid(OwnerCharacter) || !OwnerCharacter->HasAuthority())
	{
		return false;
	}

	UNUSymbolEncounterComponent* SymbolEncounterComponent =
		OwnerCharacter->SymbolEncounterComponent;
	if (!IsValid(SymbolEncounterComponent))
	{
		return false;
	}

	TArray<FName> SelectedEnemyIds;
	if (!EncounterZone.SelectRandomEnemies(SelectedEnemyIds))
	{
		UE_LOG(LogTemp, Warning, TEXT("RandomEncounter: Enemy 선택 실패. Zone=%s"),
			*EncounterZone.GetName()
		);

		ResetEncounterProgress();
		return false;
	}

	TArray<FNUEnemySpawnSpec> Enemies;
	Enemies.Reserve(SelectedEnemyIds.Num());

	// 선택된 수만큼 Enemy 스폰
	for (const FName EnemyId : SelectedEnemyIds)
	{
		FNUEnemySpawnSpec EnemySpec;
		EnemySpec.EnemyId = EnemyId;

		Enemies.Add(EnemySpec);

		UE_LOG(LogTemp, Warning, TEXT("RandomEncounter Request: EnemyId=%s"),
			*EnemyId.ToString()
		);
	}

	for (const FNUEnemySpawnSpec& Enemy : Enemies)
	{
		UE_LOG(LogTemp, Warning, TEXT("RandomEncounter Request: EnemyId=%s"),
			*Enemy.EnemyId.ToString());
	}

	const ENUBattleEncounterStartResult StartResult =
		SymbolEncounterComponent->TryStartFieldEvent(
			Enemies,
			EncounterClass,
			ENUBattleEncounterSource::Random,
			nullptr,
			EncounterZone.GetBattleStageLevel()
			);

	if (StartResult != ENUBattleEncounterStartResult::Started)
	{
		UE_LOG(LogTemp, Warning, TEXT("RandomEncounter failed: Result=%s, EncounterClass=%s"),
			*UEnum::GetValueAsString(StartResult),
			*EncounterClass.ToSoftObjectPath().ToString()
		);
		ResetEncounterProgress();
		return false;
	}

	ResetEncounterProgress();
	return true;
}

void UNURandomEncounterComponent::GenerateNextEncounterDistance()
{
	const float SafeMinDistance = FMath::Max(0.0f, MinEncounterDistance);
	const float SafeMaxDistance = FMath::Max(SafeMinDistance, MaxEncounterDistance);

	TargetDistance = FMath::FRandRange(SafeMinDistance, SafeMaxDistance);
}
