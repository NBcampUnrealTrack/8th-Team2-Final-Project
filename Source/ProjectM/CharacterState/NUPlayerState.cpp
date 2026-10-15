// Fill out your copyright notice in the Description page of Project Settings.


#include "NUPlayerState.h"

#include "NUSpiritState.h"
#include "CharacterProgression/Component/NUJobComponent.h"
#include "CharacterProgression/Component/NUPlayerSkillComponent.h"
#include "CharacterProgression/Component/NUProgressionComponent.h"
#include "Field/Net/FieldSyncSubsystem.h"
#include "Field/Interior/NUInteriorParticipantComponent.h"
#include "Inventory/NUEquipmentComponent.h"
#include "Inventory/NUInventoryComponent.h"
#include "Quest/Component/NUQuestComponent.h"
#include "WorldLocation/Component/NUWorldLocationComponent.h"
#include "Recovery/Component/NURecoveryComponent.h"
#include "NUGAS/NUAbilitySystemComponent.h"
#include "NUGAS/Attribute/NUVitalAttributeSet.h"
#include "NUGAS/Attribute/NUCombatAttributeSet.h"
#include "NUGAS/Attribute/NUStatAttributeSet.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Naia/Component/NUNaiaRosterComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

namespace
{
	bool IsPawnInsideInterior(const APawn* Pawn)
	{
		const UNUInteriorParticipantComponent* Participant = IsValid(Pawn)
			? Pawn->FindComponentByClass<UNUInteriorParticipantComponent>() : nullptr;
		return IsValid(Participant) && Participant->GetCurrentInterior() != nullptr;
	}
}

ANUPlayerState::ANUPlayerState()
{
	SetNetUpdateFrequency(100.f);
	
	AbilitySystemComponent = CreateDefaultSubobject<UNUAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	
	StatusSet = CreateDefaultSubobject<UNUStatAttributeSet>(TEXT("StatusSet"));
	VitalSet = CreateDefaultSubobject<UNUVitalAttributeSet>(TEXT("VitalSet"));
	CombatSet = CreateDefaultSubobject<UNUCombatAttributeSet>(TEXT("CombatSet"));

	InventoryComponent = CreateDefaultSubobject<UNUInventoryComponent>(TEXT("InventoryComponent"));
	EquipmentComponent = CreateDefaultSubobject<UNUEquipmentComponent>(TEXT("EquipmentComponent"));
	QuestComponent = CreateDefaultSubobject<UNUQuestComponent>(TEXT("QuestComponent"));
	WorldLocationComponent = CreateDefaultSubobject<UNUWorldLocationComponent>(TEXT("WorldLocationComponent"));
	RecoveryComponent = CreateDefaultSubobject<UNURecoveryComponent>(TEXT("RecoveryComponent"));
	JobComponent = CreateDefaultSubobject<UNUJobComponent>(TEXT("JobComponent"));
	ProgressionComponent = CreateDefaultSubobject<UNUProgressionComponent>(TEXT("ProgressionComponent"));
	PlayerSkillComponent = CreateDefaultSubobject<UNUPlayerSkillComponent>(TEXT("PlayerSkillComponent"));
	NaiaRosterComponent = CreateDefaultSubobject<UNUNaiaRosterComponent>(TEXT("NaiaRosterComponent"));
}

void ANUPlayerState::BeginPlay()
{
	Super::BeginPlay();
	
	// InitAbilityActorInfo(this, this)를 직접 부르면 이미 도착한 CurrentAvatar를 덮어씀
	// 엔진이 프로퍼티/RepNotify를 먼저 적용하고 BeginPlay를 나중에 부르므로
	// 늦게 접속한 클라는 아바타가 실린 채로 여기 도달
	AbilitySystemComponent->RefreshAbilityActorInfo();
	// CharacterSelect 뒤 맵 이동 전에 도착한 170번 응답을 새 PlayerState와 GAS에 연결한다.
	ProgressionComponent->ApplyCachedServerSnapshot();
	
	if (HasAuthority())
	{
		// 서버 상태가 오기 전에는 기본 성장 상태를 확정하지 않고 GAS만 구성한다.
		// 실제 Level/Experience/PrimaryStats는 이후 ApplyState()로 전달한다.
		if (ProgressionComponent->RebuildAttributes())
		{
			ProgressionComponent->InitializeCurrentVitalsOnce();
		}
		GiveStartupAbilities();
	}
}

UAbilitySystemComponent* ANUPlayerState::GetAbilitySystemComponent() const
{
	return GetNUASC();
}

UNUAbilitySystemComponent* ANUPlayerState::GetNUASC() const
{
	return AbilitySystemComponent;
}

UNUInventoryComponent* ANUPlayerState::GetInventoryComponent() const
{
	return InventoryComponent;
}

UNUEquipmentComponent* ANUPlayerState::GetEquipmentComponent() const
{
	return EquipmentComponent;
}

UNUQuestComponent* ANUPlayerState::GetQuestComponent() const
{
	return QuestComponent;
}

UNUJobComponent* ANUPlayerState::GetJobComponent() const
{
	return JobComponent;
}

UNUPlayerSkillComponent* ANUPlayerState::GetPlayerSkillComponent() const
{
	return PlayerSkillComponent;
}

UNUProgressionComponent* ANUPlayerState::GetProgressionComponent() const
{
	return ProgressionComponent;
}

UNUNaiaRosterComponent* ANUPlayerState::GetNaiaRosterComponent() const
{
	return NaiaRosterComponent;
}

void ANUPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(ThisClass, ActiveSpiritState);
	DOREPLIFETIME(ThisClass, bNaiaSummoned);
}

void ANUPlayerState::GiveStartupAbilities()
{
	AbilitySystemComponent->GiveStartupAbilities();
}

ANUSpiritState* ANUPlayerState::GetActiveSpiritState() const
{
	return ActiveSpiritState;
}

void ANUPlayerState::SetActiveSpiritState(ANUSpiritState* NewSpiritState)
{
	if (!HasAuthority())
	{
		return;
	}
	
	ActiveSpiritState = NewSpiritState;
	ForceNetUpdate();

	TryEnsureActiveSpiritFieldAvatar();
	NotifyFieldSpiritChanged();
}

// 필드 동기화에 정령이 바뀌었다고 알림
// PlayerState는 전원분이 모든 클라에 복제되므로 내 정령 id를 넘기지 않는다
// 다른 클라이언트의 id를 덮어쓰거나 할 수 있음
void ANUPlayerState::NotifyFieldSpiritChanged()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	if (UFieldSyncSubsystem* Sync = World->GetSubsystem<UFieldSyncSubsystem>())
	{
		Sync->RefreshMySpirit();
	}
}

void ANUPlayerState::OnRep_ActiveSpiritState()
{
	NotifyFieldSpiritChanged();
}

void ANUPlayerState::SetNaiaSummoned(bool bSummoned)
{
	if (!HasAuthority())
	{
		return;
	}

	// 실내에서는 소환 입력이나 나이아 선택으로 다시 꺼내지 않는다.
	if (bSummoned && IsPawnInsideInterior(GetPawn()))
	{
		bSummoned = false;
	}
	if (bNaiaSummoned == bSummoned)
	{
		return;
	}

	bNaiaSummoned = bSummoned;
	ForceNetUpdate();

	
	if (bNaiaSummoned)
	{
		// 나이아를 소환한다면 필드 아바타 생성
		TryEnsureActiveSpiritFieldAvatar();
	}
	else if (IsValid(ActiveSpiritState))
	{
		// 나이아를 소환하지 않고, SpiritState가 유효하다면 필드 아바타 제거
		ActiveSpiritState->DestroyFieldAvatar();
	}

	// 남의 화면에 보이는 원격 정령도 같이 사라져야 함
	NotifyFieldSpiritChanged();
}

bool ANUPlayerState::IsNaiaSummoned() const
{
	return bNaiaSummoned;
}

void ANUPlayerState::OnRep_NaiaSummoned()
{
	NotifyFieldSpiritChanged();
}

void ANUPlayerState::TryEnsureActiveSpiritFieldAvatar()
{
	if (!HasAuthority() || !IsValid(ActiveSpiritState))
	{
		return;
	}

	// 꺼내지 않았으면 필드 아바타를 만들지 않음
	// 전투 종료 후 복귀(RespawnActiveSpiritFieldAvatars)도 이 함수를 거치므로 여기 한 곳이면 됨
	if (!bNaiaSummoned)
	{
		return;
	}
	
	AController* OwnerController = Cast<AController>(GetOwner());
	APawn* CurrentPawn = OwnerController ? OwnerController->GetPawn() : nullptr;
	if (!IsValid(CurrentPawn))
	{
		return;
	}
	
	// 실내에서 빙의/로스터 복원이 늦게 끝나도 필드 아바타를 만들지 않는다.
	if (IsPawnInsideInterior(CurrentPawn))
	{
		SetNaiaSummoned(false);
		return;
	}
	ActiveSpiritState->EnsureFieldAvatar(CurrentPawn);
}
