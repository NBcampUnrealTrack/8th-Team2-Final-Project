#include "CharacterProgression/UI/Stats/NUCharacterStatsWidget.h"

#include "CharacterProgression/Component/NUJobComponent.h"
#include "CharacterProgression/Component/NUProgressionComponent.h"
#include "CharacterProgression/Data/NUProgressionDataSubsystem.h"
#include "CharacterState/NUPlayerState.h"
#include "NUGAS/Attribute/NUCombatAttributeSet.h"
#include "NUGAS/Attribute/NUVitalAttributeSet.h"
#include "NUGAS/NUAbilitySystemComponent.h"
#include "Recovery/Net/NURecoveryNetSubsystem.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

namespace
{
	// 화면이 열려 있을 때 바로 갱신할 GAS Attribute 목록
	const TArray<FGameplayAttribute>& GetWatchedAttributes()
	{
		static const TArray<FGameplayAttribute> Attributes =
		{
			UNUVitalAttributeSet::GetHealthAttribute(),
			UNUVitalAttributeSet::GetMaxHealthAttribute(),
			UNUVitalAttributeSet::GetSPAttribute(),
			UNUVitalAttributeSet::GetMaxSPAttribute(),
			UNUCombatAttributeSet::GetAttackPowerAttribute(),
			UNUCombatAttributeSet::GetMagicAttackPowerAttribute(),
			UNUCombatAttributeSet::GetDefenseAttribute(),
			UNUCombatAttributeSet::GetSpeedAttribute(),
			UNUCombatAttributeSet::GetCriticalRateAttribute(),
			UNUCombatAttributeSet::GetCriticalDamageAttribute()
		};
		return Attributes;
	}
}

void UNUCharacterStatsWidget::InitializeStats(UNUProgressionComponent* InProgression, FSimpleDelegate InCloseRequested)
{
	Progression = InProgression;
	SetCloseRequested(MoveTemp(InCloseRequested));
}

void UNUCharacterStatsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Progression이 없으면 표시할 성장 상태가 없음
	if (!Progression.IsValid())
	{
		return;
	}

	// 위젯을 다시 열어도 델리게이트가 중복되지 않게 기존 바인딩 제거
	Progression->OnProgressionChanged.RemoveDynamic(this, &ThisClass::HandleProgressionChanged);
	Progression->OnProgressionChanged.AddDynamic(this, &ThisClass::HandleProgressionChanged);
	// 서버의 최종 스탯 응답도 동일한 ViewData 갱신 경로로 처리
	Progression->OnCharacterStatSnapshotChanged.RemoveDynamic(this, &ThisClass::HandleCharacterStatSnapshotChanged);
	Progression->OnCharacterStatSnapshotChanged.AddDynamic(this, &ThisClass::HandleCharacterStatSnapshotChanged);

	// 실시간 HP, SP, 전투 스탯은 PlayerState의 ASC에서 읽음
	const ANUPlayerState* PlayerState = Cast<ANUPlayerState>(Progression->GetOwner());
	JobComponent = PlayerState != nullptr ? PlayerState->GetJobComponent() : nullptr;
	if (JobComponent.IsValid())
	{
		JobComponent->OnJobChanged.RemoveDynamic(this, &ThisClass::HandleJobChanged);
		JobComponent->OnJobChanged.AddDynamic(this, &ThisClass::HandleJobChanged);
	}
	ASC = PlayerState != nullptr ? PlayerState->GetNUASC() : nullptr;
	if (UGameInstance* GI = GetGameInstance()) { GI->GetSubsystem<UNURecoveryNetSubsystem>()->OnDisplayChanged.AddUObject(this, &ThisClass::RefreshValues); }
	BindAttributeDelegates();
	RefreshValues();
}

void UNUCharacterStatsWidget::NativeDestruct()
{
	if (UGameInstance* GI = GetGameInstance()) { GI->GetSubsystem<UNURecoveryNetSubsystem>()->OnDisplayChanged.RemoveAll(this); }
	// 닫힌 위젯이 계속 상태 변경을 받지 않도록 구독 해제
	if (Progression.IsValid())
	{
		Progression->OnProgressionChanged.RemoveDynamic(this, &ThisClass::HandleProgressionChanged);
		Progression->OnCharacterStatSnapshotChanged.RemoveDynamic(this, &ThisClass::HandleCharacterStatSnapshotChanged);
	}
	if (JobComponent.IsValid())
	{
		JobComponent->OnJobChanged.RemoveDynamic(this, &ThisClass::HandleJobChanged);
	}
	UnbindAttributeDelegates();
	JobComponent.Reset();
	ASC.Reset();

	Super::NativeDestruct();
}

void UNUCharacterStatsWidget::HandleProgressionChanged()
{
	RefreshValues();
}

void UNUCharacterStatsWidget::HandleCharacterStatSnapshotChanged()
{
	RefreshValues();
}

void UNUCharacterStatsWidget::HandleJobChanged()
{
	RefreshValues();
}

void UNUCharacterStatsWidget::HandleAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
	RefreshValues();
}

void UNUCharacterStatsWidget::BindAttributeDelegates()
{
	if (!ASC.IsValid())
	{
		return;
	}

	for (const FGameplayAttribute& Attribute : GetWatchedAttributes())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(Attribute)
			.AddUObject(this, &ThisClass::HandleAttributeChanged);
	}
}

void UNUCharacterStatsWidget::UnbindAttributeDelegates()
{
	if (!ASC.IsValid())
	{
		return;
	}

	for (const FGameplayAttribute& Attribute : GetWatchedAttributes())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(Attribute).RemoveAll(this);
	}
}

void UNUCharacterStatsWidget::RefreshValues()
{
	if (!Progression.IsValid())
	{
		return;
	}

	const ANUPlayerState* PlayerState = Cast<ANUPlayerState>(Progression->GetOwner());
	const FString PlayerName = PlayerState != nullptr ? PlayerState->GetPlayerName() : FString();
	ViewData.PlayerName = PlayerName.IsEmpty()
		? NSLOCTEXT("NUCharacterStats", "UnknownPlayer", "플레이어")
		: FText::FromString(PlayerName);
	ViewData.JobName = NSLOCTEXT("NUCharacterStats", "Adventurer", "모험가");
	if (JobComponent.IsValid() && JobComponent->HasJob())
	{
		const UWorld* World = GetWorld();
		const UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
		const UNUProgressionDataSubsystem* Data = GameInstance != nullptr
			? GameInstance->GetSubsystem<UNUProgressionDataSubsystem>()
			: nullptr;
		const FNUJobDefinitionRow* Job = Data != nullptr ? Data->FindJob(JobComponent->GetJobId()) : nullptr;
		if (Job != nullptr)
		{
			ViewData.JobName = Job->DisplayName;
			const FNUJobAdvancementDefinitionRow* Advancement = Data->FindAdvancement(JobComponent->GetAdvancementId());
			if (Advancement != nullptr)
			{
				ViewData.JobName = FText::Format(
					NSLOCTEXT("NUCharacterStats", "JobWithAdvancement", "{0} : {1}"),
					Job->DisplayName,
					Advancement->DisplayName);
			}
		}
	}

	// 레벨, 경험치, Primary Stat은 서버가 확정한 Progression State 사용
	const FNUProgressionState& State = Progression->GetState();
	ViewData.Level = State.Level;
	ViewData.ExperienceIntoLevel = State.ExperienceIntoLevel;
	ViewData.RequiredExperienceToNext = Progression->GetRequiredExperienceToNext();
	ViewData.ExperienceRatio = Progression->GetExperienceProgressRatio();
	ViewData.Strength = State.PrimaryStats.Strength;
	ViewData.Dexterity = State.PrimaryStats.Dexterity;
	ViewData.Intelligence = State.PrimaryStats.Intelligence;
	ViewData.Luck = State.PrimaryStats.Luck;
	ViewData.Vitality = State.PrimaryStats.Vitality;
	ViewData.ParryRate = 0.f;
	ViewData.DodgeRate = 0.f;
	ViewData.DamageReduce = 0.f;
	ViewData.HealBonus = 0.f;
	ViewData.EquipAttackRate = 0.f;
	ViewData.EquipMagicAttackRate = 0.f;
	ViewData.EquipMaxHealthRate = 0.f;
	ViewData.EquipDefenseFlat = 0.f;
	ViewData.bUsingServerSnapshot = Progression->HasCharacterStatSnapshot();
	ViewData.bHasExperienceData = Progression->HasServerProgressionSnapshot();
	ViewData.bRequestPending = Progression->IsCharacterStatRequestPending();
	if (ViewData.bUsingServerSnapshot)
	{
		// 서버 스냅샷은 장비가 반영된 최종값이므로 로컬 장비나 GAS 계산을 다시 더하지 않는다.
		const FNUCharacterStatSnapshot Snapshot = Progression->GetCharacterStatSnapshot();
		if (Snapshot.Level == State.Level && Snapshot.TotalExperience == State.TotalExperience)
		{
			ViewData.RequiredExperienceToNext = Snapshot.RequiredExperienceToNext;
			ViewData.ExperienceRatio = Snapshot.RequiredExperienceToNext > 0
				? FMath::Clamp(static_cast<float>(State.ExperienceIntoLevel) / static_cast<float>(Snapshot.RequiredExperienceToNext), 0.f, 1.f)
				: 1.f;
		}
		ViewData.ParryRate = Snapshot.ParryRate;
		ViewData.DodgeRate = Snapshot.DodgeRate;
		ViewData.DamageReduce = Snapshot.DamageReduce;
		ViewData.HealBonus = Snapshot.HealBonus;
		ViewData.EquipAttackRate = Snapshot.EquipAttackRate;
		ViewData.EquipMagicAttackRate = Snapshot.EquipMagicAttackRate;
		ViewData.EquipMaxHealthRate = Snapshot.EquipMaxHealthRate;
		ViewData.EquipDefenseFlat = Snapshot.EquipDefenseFlat;
	}

	// ASC가 아직 준비되지 않은 경우에도 WBP에는 안전한 기본값을 전달한다.
	ViewData.Health = 0.f;
	ViewData.MaxHealth = 0.f;
	ViewData.SP = 0.f;
	ViewData.MaxSP = 0.f;
	ViewData.AttackPower = 0.f;
	ViewData.MagicAttackPower = 0.f;
	ViewData.Defense = 0.f;
	ViewData.Speed = 0.f;
	ViewData.CriticalRate = 0.f;
	ViewData.CriticalDamage = 0.f;
	if (ViewData.bUsingServerSnapshot)
	{
		const FNUCharacterStatSnapshot Snapshot = Progression->GetCharacterStatSnapshot();
		ViewData.MaxHealth = Snapshot.MaxHealth;
		ViewData.MaxSP = Snapshot.MaxSP;
		ViewData.AttackPower = Snapshot.AttackPower;
		ViewData.MagicAttackPower = Snapshot.MagicAttackPower;
		ViewData.Defense = Snapshot.Defense;
		ViewData.Speed = Snapshot.Speed;
		ViewData.CriticalRate = Snapshot.CriticalRate;
		ViewData.CriticalDamage = Snapshot.CriticalDamage;
	}

	// 현재 HP/SP는 ASC를 유지하고, 최종값이 없는 경우에만 로컬 파생 능력치를 사용
	if (ASC.IsValid())
	{
		if (const UNUVitalAttributeSet* Vital = ASC->GetVitalAttributeSet())
		{
			ViewData.Health = Vital->GetHealth();
			if (UGameInstance* GI = GetGameInstance())
			{
				const UNURecoveryNetSubsystem* Net = GI->GetSubsystem<UNURecoveryNetSubsystem>();
				if (Net->IsResting()) { ViewData.Health = Net->GetDisplayedPlayerHealth(); }
			}
			ViewData.SP = Vital->GetSP();
			if (!ViewData.bUsingServerSnapshot)
			{
				ViewData.MaxHealth = Vital->GetMaxHealth();
				ViewData.MaxSP = Vital->GetMaxSP();
			}
		}
		if (!ViewData.bUsingServerSnapshot)
		{
			if (const UNUCombatAttributeSet* Combat = ASC->GetCombatAttributeSet())
			{
				ViewData.AttackPower = Combat->GetAttackPower();
				ViewData.MagicAttackPower = Combat->GetMagicAttackPower();
				ViewData.Defense = Combat->GetDefense();
				ViewData.Speed = Combat->GetSpeed();
				ViewData.CriticalRate = Combat->GetCriticalRate();
				ViewData.CriticalDamage = Combat->GetCriticalDamage();
			}
		}
	}

	// 모은 ViewData를 WBP의 텍스트와 게이지에 반영
	PlayerNameText->SetText(ViewData.PlayerName);
	JobText->SetText(ViewData.JobName);
	LevelText->SetText(FText::Format(
		NSLOCTEXT("NUCharacterStats", "LevelFormat", "Lv. {0}"),
		FText::AsNumber(ViewData.Level)));
	ExperienceText->SetText(ViewData.bHasExperienceData
		? ViewData.RequiredExperienceToNext > 0
			? FText::Format(
				NSLOCTEXT("NUCharacterStats", "ExperienceFormat", "EXP {0} / {1}"),
				FText::AsNumber(ViewData.ExperienceIntoLevel),
				FText::AsNumber(ViewData.RequiredExperienceToNext))
			: NSLOCTEXT("NUCharacterStats", "MaxLevel", "EXP MAX")
		: NSLOCTEXT("NUCharacterStats", "ExperienceUnavailable", "EXP 정보 없음"));
	ExperienceBar->SetPercent(ViewData.ExperienceRatio);

	StrengthValueText->SetText(FText::AsNumber(ViewData.Strength));
	DexterityValueText->SetText(FText::AsNumber(ViewData.Dexterity));
	IntelligenceValueText->SetText(FText::AsNumber(ViewData.Intelligence));
	LuckValueText->SetText(FText::AsNumber(ViewData.Luck));
	VitalityValueText->SetText(FText::AsNumber(ViewData.Vitality));
	HealthValueText->SetText(FText::FromString(FString::Printf(TEXT("%.1f / %.1f"), ViewData.Health, ViewData.MaxHealth)));
	SPValueText->SetText(FText::FromString(FString::Printf(TEXT("%.1f / %.1f"), ViewData.SP, ViewData.MaxSP)));
	AttackPowerValueText->SetText(FText::FromString(FString::Printf(TEXT("%.2f"), ViewData.AttackPower)));
	MagicAttackPowerValueText->SetText(FText::FromString(FString::Printf(TEXT("%.2f"), ViewData.MagicAttackPower)));
	DefenseValueText->SetText(FText::FromString(FString::Printf(TEXT("%.2f"), ViewData.Defense)));
	SpeedValueText->SetText(FText::FromString(FString::Printf(TEXT("%.2f"), ViewData.Speed)));
	// 어트리뷰트는 0~1 확률과 배율이라 표기할 때 퍼센트로 환산한다
	CriticalRateValueText->SetText(FText::FromString(FString::Printf(TEXT("%.1f%%"), ViewData.CriticalRate * 100.f)));
	CriticalDamageValueText->SetText(FText::FromString(FString::Printf(TEXT("%.1f%%"), ViewData.CriticalDamage * 100.f)));
}
