// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Common/NU_PlayerVitalWidget.h"

#include "AbilitySystemComponent.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "NUGAS/Attribute/NUVitalAttributeSet.h"
#include "NUGAS/NUAbilitySystemComponent.h"
#include "Recovery/Net/NURecoveryNetSubsystem.h"
#include "Engine/GameInstance.h"
#include "CharacterProgression/Component/NUJobComponent.h"
#include "CharacterProgression/Component/NUProgressionComponent.h"
#include "CharacterProgression/Data/NUProgressionDataSubsystem.h"
#include "CharacterSelect/NUCharacterManager.h"
#include "CharacterSelect/UI/NUCharacterSlotWidget.h"
#include "CharacterState/NUPlayerState.h"
#include "Components/Overlay.h"

namespace
{
	TArray<FGameplayAttribute> GetPlayerVitalAttributes()
	{
		return {
			UNUVitalAttributeSet::GetHealthAttribute(),
			UNUVitalAttributeSet::GetMaxHealthAttribute(),
			UNUVitalAttributeSet::GetSPAttribute(),
			UNUVitalAttributeSet::GetMaxSPAttribute()
		};
	}
}

void UNU_PlayerVitalWidget::InitializeWidget()
{
	Super::InitializeWidget();

	if (UGameInstance* GI = GetGameInstance()) { GI->GetSubsystem<UNURecoveryNetSubsystem>()->OnDisplayChanged.AddUObject(this, &ThisClass::RefreshFromASC); }
	BindVitalAttributes();
	RefreshFromASC();
	RefreshIdentity();
}

void UNU_PlayerVitalWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (HealthGhostBar != nullptr && GhostPercent > HealthPercent)
	{
		if (GhostHold > 0.f)
		{
			GhostHold -= InDeltaTime;
		}
		else
		{
			GhostPercent = FMath::FInterpTo(GhostPercent, HealthPercent, InDeltaTime, 5.f);
			if (GhostPercent - HealthPercent < 0.002f)
			{
				GhostPercent = HealthPercent;
			}
		}
		HealthGhostBar->SetPercent(GhostPercent);
	}

	// 레벨·직업이 바뀌는 순간을 따로 구독하지 않고 가끔 다시 읽는다 (값 몇 개라 가볍다)
	IdentityTimer -= InDeltaTime;
	if (IdentityTimer <= 0.f)
	{
		IdentityTimer = 1.f;
		RefreshIdentity();
	}
}

void UNU_PlayerVitalWidget::RefreshIdentity()
{
	if (PlayerNameText == nullptr && PlayerLevelText == nullptr && PlayerJobText == nullptr && PortraitBox == nullptr)
	{
		return;
	}
	const UGameInstance* GI = GetGameInstance();
	const UNUCharacterManager* Manager = GI != nullptr ? GI->GetSubsystem<UNUCharacterManager>() : nullptr;
	FNUCharacterSummary Selected;
	const bool bSelected = Manager != nullptr && Manager->GetSelectedCharacter(Selected);

	int32 Level = bSelected ? Selected.Level : 0;
	int32 JobId = bSelected ? Selected.JobId : 0;
	const ANUPlayerState* State = GetOwningPlayerState<ANUPlayerState>();
	if (State != nullptr)
	{
		if (const UNUProgressionComponent* Progression = State->GetProgressionComponent())
		{
			Level = FMath::Max(Level, Progression->GetLevel());
		}
		if (const UNUJobComponent* Job = State->GetJobComponent())
		{
			JobId = Job->GetJobId() > 0 ? Job->GetJobId() : JobId;
		}
	}

	if (PlayerNameText != nullptr)
	{
		// 허용 길이(한글 8자 / 영문 14자)를 넘는 이름(서버 없이 시험할 때의 계정 ID 등)은 잘라서 "…"
		const FString Name = bSelected && !Selected.Name.IsEmpty() ? Selected.Name
			: State != nullptr ? State->GetPlayerName() : FString();
		PlayerNameText->SetText(FText::FromString(NUCharacterName::Shorten(Name)));
	}
	if (PlayerLevelText != nullptr)
	{
		PlayerLevelText->SetText(Level > 0 ? FText::AsNumber(Level) : FText::GetEmpty());
	}
	if (PlayerJobText != nullptr)
	{
		// 직업 이름은 직업 표(DT_JobDefinition)가 정본 --- 선택창 줄과 같은 규칙
		FText JobName = NSLOCTEXT("NUCharacter", "JobNone", "노비스");
		if (JobId > 0)
		{
			const UNUProgressionDataSubsystem* Data = GI != nullptr ? GI->GetSubsystem<UNUProgressionDataSubsystem>() : nullptr;
			const FNUJobDefinitionRow* Row = Data != nullptr ? Data->FindJob(JobId) : nullptr;
			JobName = Row != nullptr && !Row->DisplayName.IsEmpty() ? Row->DisplayName
				: FText::Format(NSLOCTEXT("NUCharacter", "JobUnknown", "직업 {0}"), FText::AsNumber(JobId));
		}
		PlayerJobText->SetText(JobName);
	}
	if (PortraitBox != nullptr && PortraitJobId != JobId)
	{
		// 고른 캐릭터가 없으면(맵을 바로 연 시험 플레이) 필드 캐릭터와 같은 기본 노비스 얼굴
		if (!bSelected && Manager != nullptr)
		{
			Manager->GetSelectedOrTestCharacter(Selected);
		}
		PortraitJobId = JobId;
		Selected.JobId = JobId;
		UNUCharacterSlotWidget::BuildPortrait(this, PortraitBox, Selected, false, 0.f, PortraitPixelScale, true, nullptr, nullptr);
	}
}

void UNU_PlayerVitalWidget::ReleaseWidget()
{
	if (UGameInstance* GI = GetGameInstance()) { GI->GetSubsystem<UNURecoveryNetSubsystem>()->OnDisplayChanged.RemoveAll(this); }
	UnbindVitalAttributes();

	Super::ReleaseWidget();
}

void UNU_PlayerVitalWidget::BindVitalAttributes()
{
	UNUAbilitySystemComponent* ASC = GetNUASC();
	if (!IsValid(ASC))
	{
		return;
	}

	for (const FGameplayAttribute& Attribute : GetPlayerVitalAttributes())
	{
		const FDelegateHandle Handle = ASC->GetGameplayAttributeValueChangeDelegate(Attribute)
			.AddUObject(this, &ThisClass::HandleVitalAttributeChanged);

		VitalDelegateHandles.Emplace(Attribute, Handle);
	}
}

void UNU_PlayerVitalWidget::UnbindVitalAttributes()
{
	if (UNUAbilitySystemComponent* ASC = GetNUASC())
	{
		for (const TPair<FGameplayAttribute, FDelegateHandle>& Pair : VitalDelegateHandles)
		{
			ASC->GetGameplayAttributeValueChangeDelegate(Pair.Key).Remove(Pair.Value);
		}
	}
	VitalDelegateHandles.Empty();
}

void UNU_PlayerVitalWidget::HandleVitalAttributeChanged(const FOnAttributeChangeData& Data)
{
	RefreshFromASC();
}

void UNU_PlayerVitalWidget::RefreshFromASC()
{
	const UNUAbilitySystemComponent* ASC = GetNUASC();
	const UNUVitalAttributeSet* Vital = ASC ? ASC->GetVitalAttributeSet() : nullptr;
	if (Vital == nullptr)
	{
		return;
	}

	Health = Vital->GetHealth();
	if (UGameInstance* GI = GetGameInstance())
	{
		const UNURecoveryNetSubsystem* Net = GI->GetSubsystem<UNURecoveryNetSubsystem>();
		if (Net->IsResting()) { Health = Net->GetDisplayedPlayerHealth(); }
	}
	MaxHealth = Vital->GetMaxHealth();
	SP = Vital->GetSP();
	MaxSP = Vital->GetMaxSP();

	HealthPercent = MaxHealth > 0.f ? Health / MaxHealth : 0.f;
	SPPercent = MaxSP > 0.f ? SP / MaxSP : 0.f;

	if (IsValid(HealthValueText))
	{
		HealthValueText->SetText(FText::AsNumber(FMath::RoundToInt(Health)));
	}
	if (IsValid(SPValueText))
	{
		SPValueText->SetText(FText::AsNumber(FMath::RoundToInt(SP)));
	}
	if (IsValid(HealthGhostBar))
	{
		// 깎였으면 잠깐 남겨 둔다 (틱에서 따라 내려감), 회복이면 바로 붙인다
		if (GhostPercent < 0.f || HealthPercent >= GhostPercent)
		{
			GhostPercent = HealthPercent;
			HealthGhostBar->SetPercent(GhostPercent);
		}
		else
		{
			GhostHold = 0.45f;
		}
	}

	if (IsValid(HealthText))
	{
		HealthText->SetText(FText::FromString(FString::Printf(TEXT("HP  %.0f / %.0f"), Health, MaxHealth)));
	}

	if (IsValid(HealthBar))
	{
		HealthBar->SetPercent(HealthPercent);
	}

	if (IsValid(SPBar))
	{
		SPBar->SetPercent(SPPercent);
	}

	if (IsValid(SPText))
	{
		SPText->SetText(FText::FromString(FString::Printf(TEXT("SP  %.0f / %.0f"), SP, MaxSP)));
	}

	OnVitalsChanged();
}
