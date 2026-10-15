// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Common/NU_CombatantStatusWidget.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Battle/Encounter/NUBattleEncounter.h"
#include "Battle/Avatar/NUBattleEnemyAvatar.h"
#include "Battle/NUBattleTypes.h"
#include "CharacterState/NUSpiritState.h"
#include "Naia/Data/NUNaiaDataSubsystem.h"
#include "Naia/Data/NUNaiaDefinition.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "NUGAS/Attribute/NUVitalAttributeSet.h"
#include "NUGAS/NUAbilitySystemComponent.h"
#include "UI/Common/NU_StatusIconWidget.h"
#include "UI/Common/NU_JobResourceGaugeWidget.h"
#include "UI/Common/NU_JobResourceIconWidget.h"

namespace
{
	TArray<FGameplayAttribute> GetCombatantStatusVitalAttributes()
	{
		return {
			UNUVitalAttributeSet::GetHealthAttribute(),
			UNUVitalAttributeSet::GetMaxHealthAttribute(),
			UNUVitalAttributeSet::GetSPAttribute(),
			UNUVitalAttributeSet::GetMaxSPAttribute()
		};
	}

	// 아이콘을 다시 그릴 필요가 있는지만 가리면 되므로 화면에 나오는 값만 비교한다.
	// 두 목록 모두 StatusEffectId 오름차순이라 자리끼리 맞대면 된다.
	bool AreStatusViewsEqual(const TArray<FNUStatusEffectView>& Left, const TArray<FNUStatusEffectView>& Right)
	{
		if (Left.Num() != Right.Num())
		{
			return false;
		}

		for (int32 Index = 0; Index < Left.Num(); ++Index)
		{
			if (Left[Index].StatusEffectId != Right[Index].StatusEffectId
				|| Left[Index].BadgeNumber != Right[Index].BadgeNumber
				|| Left[Index].bBadgeIsStack != Right[Index].bBadgeIsStack)
			{
				return false;
			}
		}

		return true;
	}

	// 화면에 나오는 값만 비교함, 두 목록 모두 번호순으로 정렬되어 옴 (ASC 쪽 주석 참고)
	bool AreJobResourceViewsEqual(const TArray<FNUJobResourceView>& Left, const TArray<FNUJobResourceView>& Right)
	{
		if (Left.Num() != Right.Num())
		{
			return false;
		}

		for (int32 Index = 0; Index < Left.Num(); ++Index)
		{
			if (Left[Index].ResourceId != Right[Index].ResourceId
				|| Left[Index].bIsState != Right[Index].bIsState
				|| Left[Index].Amount != Right[Index].Amount
				|| Left[Index].MaxAmount != Right[Index].MaxAmount
				|| Left[Index].RemainingTurns != Right[Index].RemainingTurns
				|| Left[Index].bStatePending != Right[Index].bStatePending)
			{
				return false;
			}
		}

		return true;
	}
}

void UNU_CombatantStatusWidget::SetCombatant(ANUBattleEncounter* Encounter, int32 InCombatantId)
{
	UE_LOG(LogTemp, Warning, TEXT("[NU_CombatantStatusWidget] SetCombatant On"));
	UnbindVitalAttributes();
	UnbindStatusEffects();
	UnbindJobResources();
	CombatantId = InCombatantId;
	CachedASC = nullptr;
	SetIdentityDirect(FText::GetEmpty(), 0, false);

	FNUBattleParticipant Participant;
	if (Encounter && Encounter->GetParticipant(InCombatantId, Participant))
	{
		if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Participant.ASCOwner))
		{
			CachedASC = Cast<UNUAbilitySystemComponent>(ASI->GetAbilitySystemComponent());
		}

		if (Participant.Team == ENUBattleTeam::Enemy)
		{
			int32 NaiaId = FCString::Atoi(*Participant.EnemySpec.EnemyId.ToString());
			int32 Level = 0;
			if (const ANUSpiritState* Spirit = Cast<ANUSpiritState>(Participant.ASCOwner))
			{
				NaiaId = Spirit->GetSpiritId();
				Level = Spirit->GetCurrentLevel();
			}
			else if (const ANUBattleEnemyAvatar* Enemy = Cast<ANUBattleEnemyAvatar>(Participant.AvatarActor))
			{
				Level = Enemy->GetLevel();
			}
			const UNUNaiaDataSubsystem* Data = GetGameInstance()
				? GetGameInstance()->GetSubsystem<UNUNaiaDataSubsystem>() : nullptr;
			const FNUNaiaDefinition* Definition = Data ? Data->FindDefinitionById(NaiaId) : nullptr;
			SetIdentityDirect(Definition ? Definition->KoreanName : FText::GetEmpty(), Level, true);
		}
	}

	BindVitalAttributes();
	BindStatusEffects();
	BindJobResources();
	RefreshFromASC();
	RebuildStatusIcons();
	RebuildJobResources(true);
}

void UNU_CombatantStatusWidget::NativeDestruct()
{
	UnbindVitalAttributes();
	UnbindStatusEffects();
	UnbindJobResources();

	Super::NativeDestruct();
}

void UNU_CombatantStatusWidget::BindVitalAttributes()
{
	UNUAbilitySystemComponent* ASC = CachedASC.Get();
	if (!IsValid(ASC))
	{
		return;
	}

	for (const FGameplayAttribute& Attribute : GetCombatantStatusVitalAttributes())
	{
		const FDelegateHandle Handle = ASC->GetGameplayAttributeValueChangeDelegate(Attribute)
			.AddUObject(this, &ThisClass::HandleVitalAttributeChanged);

		VitalDelegateHandles.Emplace(Attribute, Handle);
	}
}

void UNU_CombatantStatusWidget::UnbindVitalAttributes()
{
	if (UNUAbilitySystemComponent* ASC = CachedASC.Get())
	{
		for (const TPair<FGameplayAttribute, FDelegateHandle>& Pair : VitalDelegateHandles)
		{
			ASC->GetGameplayAttributeValueChangeDelegate(Pair.Key).Remove(Pair.Value);
		}
	}
	VitalDelegateHandles.Empty();
}

void UNU_CombatantStatusWidget::HandleVitalAttributeChanged(const FOnAttributeChangeData& Data)
{
	RefreshFromASC();
}

void UNU_CombatantStatusWidget::SetPendingSpend(float InPendingSpend)
{
	PendingSPSpend = InPendingSpend;

	// 표시값만 다시 계산함 - Health/MaxHealth/ActualSP/MaxSP 는 그대로라 재조회가 필요 없음
	// RefreshFromASC를 다시 부르면 ASC가 없는 넷 전투 아바타는 값이 전부 0으로 지워짐
	// (SetVitalsDirect로 채운 서버 스냅샷 값을 잃어버리는 문제였음)
	ApplyVitalsToDisplay();
}

void UNU_CombatantStatusWidget::RefreshFromASC()
{
	const UNUAbilitySystemComponent* ASC = CachedASC.Get();
	const UNUVitalAttributeSet* Vital = ASC ? ASC->GetVitalAttributeSet() : nullptr;

	Health = Vital ? Vital->GetHealth() : 0.f;
	MaxHealth = Vital ? Vital->GetMaxHealth() : 0.f;
	ActualSP = Vital ? Vital->GetSP() : 0.f;
	MaxSP = Vital ? Vital->GetMaxSP() : 0.f;

	ApplyVitalsToDisplay();
}

void UNU_CombatantStatusWidget::SetVitalsDirect(int32 InHealth, int32 InMaxHealth, int32 InSP, int32 InMaxSP)
{
	// 넷 전투 전용 - ASC가 없어 서버 스냅샷 값을 여기서 직접 채움
	Health = static_cast<float>(InHealth);
	MaxHealth = static_cast<float>(InMaxHealth);
	ActualSP = static_cast<float>(InSP);
	MaxSP = static_cast<float>(InMaxSP);

	ApplyVitalsToDisplay();
}

void UNU_CombatantStatusWidget::SetIdentityDirect(const FText& InName, int32 InLevel, bool bEnemy)
{
	if (CombatantNameText)
	{
		CombatantNameText->SetText(InName);
	}
	if (CombatantLevelText)
	{
		CombatantLevelText->SetText(InLevel > 0
			? FText::Format(NSLOCTEXT("NUBattle", "CombatantLevel", "Lv. {0}"), FText::AsNumber(InLevel))
			: FText::GetEmpty());
	}
	if (CombatantIdentity)
	{
		CombatantIdentity->SetVisibility(bEnemy ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (SB_SP)
	{
		SB_SP->SetVisibility(bEnemy ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (HealthBar)
	{
		HealthBar->SetFillColorAndOpacity(bEnemy ? EnemyHealthColor : AllyHealthColor);
	}
}

void UNU_CombatantStatusWidget::SetStatusViewsDirect(const TArray<FNUStatusEffectView>& InViews)
{
	// 넷 전투 전용 - ASC 가 없어 서버가 준 목록을 그대로 들고 있음
	// 전투원마다 매 턴 여러 번 불리므로 안 바뀌었으면 아이콘을 다시 만들지 않는다 ---
	// 다시 만들면 아이콘 위젯이 들고 있던 상태(툴팁·애니메이션)가 매번 날아간다.
	if (AreStatusViewsEqual(DirectStatusViews, InViews))
	{
		return;
	}

	DirectStatusViews = InViews;

	RebuildStatusIcons();
}

void UNU_CombatantStatusWidget::SetJobResourceViewsDirect(const TArray<FNUJobResourceView>& InGauges,
	const TArray<FNUJobResourceView>& InBadges)
{
	// 넷 전투 전용 - ASC 가 없어 서버가 준 목록을 그대로 들고 있음
	// 매 턴 여러 번 불리므로 안 바뀌었으면 다시 만들지 않음 (RebuildJobResources 가 지난번과 견줌)
	ServerJobResourceGauges = InGauges;
	ServerJobResourceBadges = InBadges;

	RebuildJobResources();
}

void UNU_CombatantStatusWidget::ApplyVitalsToDisplay()
{
	// 예정 소모를 미리 깎아서 보여준다 — ←/→ 로 투입량을 조절하면 실제로 줄었다 차는 것처럼 보인다.
	SP = FMath::Max(0.f, ActualSP - PendingSPSpend);

	HealthPercent = MaxHealth > 0.f ? Health / MaxHealth : 0.f;
	SPPercent = MaxSP > 0.f ? SP / MaxSP : 0.f;

	if (IsValid(HealthBar))
	{
		HealthBar->SetPercent(HealthPercent);
	}

	if (IsValid(HealthText))
	{
		HealthText->SetText(FText::FromString(FString::Printf(TEXT("HP  %.0f / %.0f"), Health, MaxHealth)));
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

void UNU_CombatantStatusWidget::BindStatusEffects()
{
	UNUAbilitySystemComponent* ASC = CachedASC.Get();
	if (!IsValid(ASC))
	{
		return;
	}

	StatusEffectsDelegateHandle =
		ASC->OnStatusEffectsChanged.AddUObject(this, &ThisClass::HandleStatusEffectsChanged);
}

void UNU_CombatantStatusWidget::UnbindStatusEffects()
{
	if (UNUAbilitySystemComponent* ASC = CachedASC.Get())
	{
		ASC->OnStatusEffectsChanged.Remove(StatusEffectsDelegateHandle);
	}

	// 핸들은 전역 고유값이라 남겨둬도 오작동하지는 않지만, VitalDelegateHandles 를 비우는 것과 짝을 맞춘다.
	StatusEffectsDelegateHandle.Reset();
}

void UNU_CombatantStatusWidget::HandleStatusEffectsChanged()
{
	RebuildStatusIcons();
}

void UNU_CombatantStatusWidget::RebuildStatusIcons()
{
	if (!IsValid(Panel_StatusIcons) || !StatusIconWidgetClass)
	{
		return;
	}

	Panel_StatusIcons->ClearChildren();

	TArray<FNUStatusEffectView> Views;
	if (const UNUAbilitySystemComponent* ASC = CachedASC.Get())
	{
		// 목록은 StatusEffectId 오름차순으로 이미 정렬되어 온다 (ASC 쪽 주석 참고).
		ASC->GetActiveStatusViews(Views);
	}
	else
	{
		// 넷 전투 아바타는 ASC 바인딩이 아예 없다 - 서버가 준 목록을 그대로 쓴다.
		// 정렬과 배지 규칙은 넘어오기 전에 ASC 경로와 같은 규약으로 이미 맞춰져 있다.
		Views = DirectStatusViews;
	}

	// 자르는 곳을 여기 하나로 둔다 --- WBP 가 따로 자르면 잘린 개수와 +n 이 어긋난다
	const int32 VisibleCount = FMath::Min(Views.Num(), FMath::Max(0, MaxVisibleStatusIcons));

	for (int32 Index = 0; Index < VisibleCount; ++Index)
	{
		UNU_StatusIconWidget* IconWidget =
			WidgetTree->ConstructWidget<UNU_StatusIconWidget>(StatusIconWidgetClass);

		if (IsValid(IconWidget))
		{
			IconWidget->SetStatusEffect(Views[Index]);
			Panel_StatusIcons->AddChild(IconWidget);
		}
	}

	if (IsValid(Txt_StatusOverflow))
	{
		const int32 HiddenCount = Views.Num() - VisibleCount;

		Txt_StatusOverflow->SetVisibility(
			HiddenCount > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

		if (HiddenCount > 0)
		{
			Txt_StatusOverflow->SetText(FText::FromString(FString::Printf(TEXT("+%d"), HiddenCount)));
		}
	}
}

void UNU_CombatantStatusWidget::BindJobResources()
{
	UNUAbilitySystemComponent* ASC = CachedASC.Get();
	if (!IsValid(ASC))
	{
		return;
	}

	JobResourcesDelegateHandle =
		ASC->OnJobResourcesChanged.AddUObject(this, &ThisClass::HandleJobResourcesChanged);
}

void UNU_CombatantStatusWidget::UnbindJobResources()
{
	if (UNUAbilitySystemComponent* ASC = CachedASC.Get())
	{
		ASC->OnJobResourcesChanged.Remove(JobResourcesDelegateHandle);
	}

	JobResourcesDelegateHandle.Reset();
}

void UNU_CombatantStatusWidget::HandleJobResourcesChanged()
{
	RebuildJobResources();
}

void UNU_CombatantStatusWidget::RebuildJobResources(bool bForce)
{
	TArray<FNUJobResourceView> Gauges;
	TArray<FNUJobResourceView> Badges;
	if (const UNUAbilitySystemComponent* ASC = CachedASC.Get())
	{
		ASC->GetJobResourceViews(Gauges, Badges);
	}
	else
	{
		// 넷 전투 아바타는 ASC 바인딩이 없음 - 서버가 준 목록을 그대로 씀, 아직 안 왔으면 비어 있어 접힌 채
		Gauges = ServerJobResourceGauges;
		Badges = ServerJobResourceBadges;
	}

	if (!bForce
		&& AreJobResourceViewsEqual(ShownJobResourceGauges, Gauges)
		&& AreJobResourceViewsEqual(ShownJobResourceBadges, Badges))
	{
		return;
	}

	ShownJobResourceGauges = Gauges;
	ShownJobResourceBadges = Badges;

	if (IsValid(Panel_JobResources))
	{
		Panel_JobResources->ClearChildren();

		if (JobResourceGaugeWidgetClass)
		{
			for (const FNUJobResourceView& View : Gauges)
			{
				if (UNU_JobResourceGaugeWidget* Gauge =
					WidgetTree->ConstructWidget<UNU_JobResourceGaugeWidget>(JobResourceGaugeWidgetClass))
				{
					Gauge->SetJobResource(View);
					Panel_JobResources->AddChild(Gauge);
				}
			}
		}

		// 비었을 때 접어 둬야 자원이 없는 전투원(적 · 나이아)의 바 모양이 그대로임
		Panel_JobResources->SetVisibility(Panel_JobResources->GetChildrenCount() > 0
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}

	if (IsValid(Panel_JobResourceIcons))
	{
		Panel_JobResourceIcons->ClearChildren();

		if (JobResourceIconWidgetClass)
		{
			for (const FNUJobResourceView& View : Badges)
			{
				if (UNU_JobResourceIconWidget* Icon =
					WidgetTree->ConstructWidget<UNU_JobResourceIconWidget>(JobResourceIconWidgetClass))
				{
					Icon->SetJobResource(View);
					Panel_JobResourceIcons->AddChild(Icon);
				}
			}
		}

		Panel_JobResourceIcons->SetVisibility(Panel_JobResourceIcons->GetChildrenCount() > 0
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}
}

void UNU_CombatantStatusWidget::SetTargeted(bool bInTargeted)
{
	if (bTargeted == bInTargeted)
	{
		return;
	}
	bTargeted = bInTargeted;
	TargetedTime = 0.f;
	SetRenderTransformPivot(FVector2D(0.5f, 1.f));
	SetRenderScale(FVector2D(bTargeted ? 1.08f : 1.f));
	if (TargetMark != nullptr)
	{
		TargetMark->SetVisibility(bTargeted ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		TargetMark->SetRenderOpacity(1.f);
	}
}

void UNU_CombatantStatusWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (bTargeted && TargetMark != nullptr)
	{
		// 대상 표시가 천천히 숨쉰다 (위쪽 화살표는 대상 인디케이터 액터가 따로 그린다)
		TargetedTime += InDeltaTime;
		TargetMark->SetRenderOpacity(0.7f + 0.3f * FMath::Sin(TargetedTime * 5.f));
	}
}

void UNU_CombatantStatusWidget::SetDimmed(bool bInDimmed)
{
	SetRenderOpacity(bInDimmed ? 0.4f : 1.f);
}
