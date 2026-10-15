// Fill out your copyright notice in the Description page of Project Settings.


#include "NUSpiritState.h"

#include "NUPlayerState.h"

#include "Battle/Encounter/NUBattleEncounter.h"
#include "Engine/GameInstance.h"
#include "NUGAS/NUAbilitySystemComponent.h"
#include "NUGAS/Attribute/NUVitalAttributeSet.h"
#include "NUGAS/Attribute/NUCombatAttributeSet.h"
#include "NUGAS/Attribute/NUStatAttributeSet.h"
#include "Engine/World.h"
#include "Field/NUFieldSpirit.h"
#include "Field/Net/FieldSyncSubsystem.h"
#include "Naia/Data/NUNaiaAffection.h"
#include "Naia/Data/NUNaiaDataSubsystem.h"
#include "Naia/Data/NUNaiaDefinition.h"
#include "Net/UnrealNetwork.h"
#include "NUGAS/NUGameplayTags.h"

ANUSpiritState::ANUSpiritState()
{
	PrimaryActorTick.bCanEverTick = false;
	
	bReplicates = true;
	SetReplicateMovement(false);	
	
	// 항상 복제, 클라가 Standalone 이라 relevancy 가 평가되지 않음
	// 나중에 좁힐 때 인카운터, 아바타와 같은 조건을 쓰면 안 됨 — 이 액터는 참가자의 ASCOwner 라서
	// 파티원 화면에서 빠지면 상대 나이아의 체력바와 아바타 초기화가 깨짐
	bAlwaysRelevant = true;
	
	SetNetUpdateFrequency(100.f);
	
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	
	AbilitySystemComponent = CreateDefaultSubobject<UNUAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	
	StatusSet = CreateDefaultSubobject<UNUStatAttributeSet>(TEXT("StatusSet"));
	VitalSet = CreateDefaultSubobject<UNUVitalAttributeSet>(TEXT("VitalSet"));
	CombatSet = CreateDefaultSubobject<UNUCombatAttributeSet>(TEXT("CombatSet"));
}

void ANUSpiritState::BeginPlay()
{
	Super::BeginPlay();
	
	// PlayerState와 똑같이 맞춤
	AbilitySystemComponent->RefreshAbilityActorInfo();
	
	if (HasAuthority())
	{
		GiveStartupAbilities();
	}
}

UAbilitySystemComponent* ANUSpiritState::GetAbilitySystemComponent() const
{
	return GetNUASC();
}

UNUAbilitySystemComponent* ANUSpiritState::GetNUASC() const
{
	return AbilitySystemComponent;
}

void ANUSpiritState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(ThisClass, OwningPlayerState);
	DOREPLIFETIME(ThisClass, SpiritId);
	DOREPLIFETIME(ThisClass, InstanceId);
	DOREPLIFETIME(ThisClass, Level);
	DOREPLIFETIME(ThisClass, ActiveSkillIds);
	DOREPLIFETIME(ThisClass, PassiveSkillId);
	DOREPLIFETIME(ThisClass, Experience);
	DOREPLIFETIME(ThisClass, Affection);
}

void ANUSpiritState::InitializeSpirit(ANUPlayerState* InOwningPlayerState, const FNUNaiaInstance& Entry)
{
	if (!HasAuthority()) return;
	
	OwningPlayerState = InOwningPlayerState; 
	InstanceId = Entry.InstanceId;
	SpiritId = Entry.NaiaId;
	
	const FNUNaiaDefinition* Definition = FindNaiaDefinition();
	
	Level = Entry.Level > 0
		? Entry.Level
		: (Definition ? FMath::Max(1, Definition->SpawnLevel) : 1);

	ActiveSkillIds = Entry.ActiveSkillIds;
	PassiveSkillId = Entry.PassiveSkillId;
	
	// 애정도는 아래 ApplyInitialVitals 가 스탯 보정에 쓰므로 그보다 먼저 받아야 함
	Experience = Entry.Experience;
	Affection = Entry.Affection;
	
	// 서버가 저장한 스킬 목록을 ASC로 넘김
	// 실행기 스펙은 처음 쓸 때 부여되므로 여기서는 목록만 넘기면 됨
	if (IsValid(AbilitySystemComponent))
	{
		AbilitySystemComponent->SetActiveSkillIds(Entry.ActiveSkillIds);
		
		// ASC는 캐릭터,나이아 공용이므로 Array 형태로 패시브를 담아서 저장해야 함
		TArray<int32> OwnedPassiveIds;
		if (Entry.PassiveSkillId > 0)
		{
			OwnedPassiveIds.Add(Entry.PassiveSkillId);
		}
		AbilitySystemComponent->SetPassiveSkillIds(OwnedPassiveIds);

		UE_LOG(LogTemp, Log, TEXT("나이아 %d 액티브 스킬 %d개 : %s"),
			SpiritId,
			Entry.ActiveSkillIds.Num(),
			*FString::JoinBy(Entry.ActiveSkillIds, TEXT(", "),
				[](int32 SkillId)
				{
					return FString::FromInt(SkillId);
				})
			);
	}

	ApplyInitialVitals();
	ApplyInitialCombatStats();
	
	// 기록은 비율이라 여기서 최대체력을 이용해 현재 체력을 구함
	// 패시브까지 집계된 최대체력을 이용해야해서 여기서 처리함
	if (IsValid(AbilitySystemComponent))
	{
		const float MaxHealth =
			AbilitySystemComponent->GetNumericAttribute(UNUVitalAttributeSet::GetMaxHealthAttribute());
		
		AbilitySystemComponent->SetNumericAttributeBase(
			UNUVitalAttributeSet::GetHealthAttribute(), MaxHealth * Entry.HealthRatio);
	}
	
	ForceNetUpdate();
}

void ANUSpiritState::ApplyInitialVitals()
{
	if (!HasAuthority() || !IsValid(AbilitySystemComponent)) return;
	
	FNUAbilitySystemInitVitalData InitData;
	
	if (const FNUNaiaDefinition* Definition = FindNaiaDefinition())
	{
		const FNUNaiaStats Stats = Definition->ResolveStats(Level, NUNaiaAffection::ResolveTier(Affection));
		InitData.MaxHealth = Stats.MaxHealth;
		InitData.Health	   = Stats.MaxHealth;
		InitData.MaxSP	   = Stats.MaxSP;
		InitData.SP		   = Stats.MaxSP;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("체력 초기화 에러, 나이아 정의를 찾을 수 없습니다 : %d"), SpiritId);
		InitData.MaxHealth = 80.f;
		InitData.Health	   = 80.f;
		InitData.MaxSP	   = 40.f;
		InitData.SP		   = 40.f;
	}

	AbilitySystemComponent->ApplyInitialVitals(InitData, InitVitalsEffect, Level);
}

void ANUSpiritState::ApplyInitialCombatStats()
{
	if (!HasAuthority() || !IsValid(AbilitySystemComponent)) return;
	
	FNUAbilitySystemInitCombatData InitData;
	
	// 기존 속성 태그 삭제
	FGameplayTagContainer TagContainer;
	AbilitySystemComponent->GetOwnedGameplayTags(TagContainer);
	AbilitySystemComponent->RemoveLooseGameplayTags(
		TagContainer.Filter(FGameplayTagContainer(NU::Element::Root)));	// 기존 속성태그 삭제
	
	if (const FNUNaiaDefinition* Definition = FindNaiaDefinition())
	{
		const FNUNaiaStats Stats = Definition->ResolveStats(Level, NUNaiaAffection::ResolveTier(Affection));
		InitData.AttackPower	  = Stats.AttackPower;
		InitData.MagicAttackPower = Stats.MagicAttackPower;
		InitData.Defense		  = Stats.Defense;
		InitData.Speed			  = Stats.Speed;
		InitData.CriticalRate	  = Stats.CriticalRate;
		InitData.CriticalDamage	  = Stats.CriticalDamage;
		
		
		// 정의에서 읽어서 속성 태그 부여
		if (Definition->ElementTag.IsValid())
		{
			AbilitySystemComponent->AddLooseGameplayTag(Definition->ElementTag);
		}
		
		// 이중속성은 규칙 확정되면 나중에
		// if (Definition->SecondaryElementTag.IsValid())
		// {
		// 	AbilitySystemComponent->AddLooseGameplayTag(Definition->SecondaryElementTag);
		// }
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("전투 스탯 초기화 에러, 나이아 정의를 찾을 수 없습니다 : %d"), SpiritId);
		InitData.AttackPower	  = 20.f;
		InitData.MagicAttackPower = 20.f;
		InitData.Defense		  = 5.f;
		InitData.Speed			  = 10.f;
		InitData.CriticalRate	  = 5.f;
		InitData.CriticalDamage	  = 150.f;
	}
	
	AbilitySystemComponent->ApplyInitialCombatStats(InitData, InitCombatStatsEffect, Level);
}

void ANUSpiritState::GiveStartupAbilities()
{
	AbilitySystemComponent->GiveStartupAbilities();
}

void ANUSpiritState::SetSpiritId(int32 NewSpiritId)
{
	if (!HasAuthority()) return;
	
	SpiritId = NewSpiritId;
	ForceNetUpdate();

	NotifyFieldSpiritChanged();
}

void ANUSpiritState::OnRep_SpiritId()
{
	NotifyFieldSpiritChanged();
}

// OwningPlayerState를 거치지 않고 서브시스템을 직접 호출
// 서브시스템이 로컬 PlayerState에서 정령 id를 직접 읽으므로
// 이 액터가 누구 소유인지 알 필요가 없다.
void ANUSpiritState::NotifyFieldSpiritChanged()
{
	UWorld* World = GetWorld();
	if (!IsValid(World)) return;

	if (UFieldSyncSubsystem* Sync = World->GetSubsystem<UFieldSyncSubsystem>())
	{
		Sync->RefreshMySpirit();
	}
}

const FNUNaiaDefinition* ANUSpiritState::FindNaiaDefinition()
{
	const UGameInstance* GI = GetGameInstance();
	const UNUNaiaDataSubsystem* Subsystem = GI ? GI->GetSubsystem<UNUNaiaDataSubsystem>() : nullptr;
	const FNUNaiaDefinition* Definition = IsValid(Subsystem) ? Subsystem->FindDefinitionById(SpiritId) : nullptr;
	
	return Definition;
}

int32 ANUSpiritState::GetSpiritId() const
{
	return SpiritId;
}

int64 ANUSpiritState::GetInstanceId() const
{
	return InstanceId;
}

int32 ANUSpiritState::GetCurrentLevel() const
{
	return Level;
}

const TArray<int32>& ANUSpiritState::GetActiveSkillIds() const
{
	return ActiveSkillIds;
}

void ANUSpiritState::GrantActiveSkillForDebug(int32 SkillId)
{
	if (!HasAuthority() || SkillId <= 0)
	{
		return;
	}
	
	ActiveSkillIds.AddUnique(SkillId);
	
	if (IsValid(AbilitySystemComponent))
	{
		AbilitySystemComponent->SetActiveSkillIds(ActiveSkillIds);
	}
}

int32 ANUSpiritState::GetPassiveSkillId() const
{
	return PassiveSkillId;
}

int32 ANUSpiritState::GetExperience() const
{
	return Experience;
}

int32 ANUSpiritState::GetAffection() const
{
	return Affection;
}

bool ANUSpiritState::EnsureFieldAvatar(AActor* FollowTarget)
{
	if (!HasAuthority() || !IsValid(FollowTarget)) return false;
	
	// 전투중엔 필드 아바타 생성 X
	if (IsValid(GetBattleEncounter())) return false;
	
	// 이미 있다면 따라갈 타겟만 갱신
	if (ANUFieldSpirit* Existing = Cast<ANUFieldSpirit>(GetCurrentAvatar()))
	{
		Existing->SetFollowTarget(FollowTarget);
		return true;
	}
	
	// 다른 종류의 유효한 아바타가 있다면 교체 X
	if (IsValid(GetCurrentAvatar()))
	{
		return false;
	}
	
	if (!FieldSpiritClass)
	{
		UE_LOG(LogTemp, Error, TEXT("FieldSpritClass 가 설정되지 않았습니다!"));
		return false;
	}
	
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ANUFieldSpirit* NewAvatar = GetWorld()->SpawnActor<ANUFieldSpirit>(
			FieldSpiritClass,
			FollowTarget->GetActorTransform(),
			SpawnParams
			);

	if (!IsValid(NewAvatar)) return false;

	UE_LOG(LogTemp, Warning, TEXT("정령 스폰!"));
	
	NewAvatar->InitializeFieldSpirit(this, FollowTarget);

	SetCurrentAvatar(NewAvatar);

	return true;
}

void ANUSpiritState::DestroyFieldAvatar()
{
	if (!HasAuthority()) return;
	
	if (ANUFieldSpirit* FieldAvatar = Cast<ANUFieldSpirit>(GetCurrentAvatar()))
	{
		FieldAvatar->Destroy();
	}
	SetCurrentAvatar(nullptr);
}
