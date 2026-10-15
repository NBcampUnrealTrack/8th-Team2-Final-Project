// Fill out your copyright notice in the Description page of Project Settings.


#include "NUVitalAttributeSet.h"

#include "GameplayEffectExtension.h"
#include "Battle/Avatar/NUBattleAvatar.h"
#include "Battle/Encounter/NUBattleEncounter.h"
#include "Battle/Interface/NUBattleEventInterface.h"
#include "NUGAS/NUAbilitySystemComponent.h"
#include "NUGAS/NUGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "Battle/Data/NUJobResourceDefinition.h"


void UNUVitalAttributeSet::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME_CONDITION_NOTIFY(ThisClass, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ThisClass, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ThisClass, SP, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ThisClass, MaxSP, COND_None, REPNOTIFY_Always);
}

void UNUVitalAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

void UNUVitalAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

void UNUVitalAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);
	
	if (Attribute == GetMaxHealthAttribute())
	{
		AdjustVitalToMaxChange(GetHealthAttribute(), GetHealth(), OldValue, NewValue);
	}
	else if (Attribute == GetMaxSPAttribute())
	{
		AdjustVitalToMaxChange(GetSPAttribute(), GetSP(), OldValue, NewValue);
	}
	
	// 체력 조건으로 켜지고 끝나는 전투당 1회 상태 (3027 불굴 · 3044), 최대 체력이 바뀌어도 비율이 바뀜
	// 피해 계산 때만 보면 깎였다가 맞기 전에 회복한 것을 놓쳐 두 번 켜짐
	if (Attribute == GetHealthAttribute() || Attribute == GetMaxHealthAttribute())
	{
		if (UNUAbilitySystemComponent* OwnerASC = Cast<UNUAbilitySystemComponent>(GetOwningAbilitySystemComponent()))
		{
			OwnerASC->RefreshJobOnceStates();
		}
	}
}

void UNUVitalAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
	
	// 체력이 변할때만 아래 Death 판정
	bool bHealthChanged = false;
	
	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		// 데미지 메타 Attribute 받았을때 계산 후 처리
		float ActualDamage = GetIncomingDamage();
		SetIncomingDamage(0.f);

		// Execution이 이 분기보다 먼저 실어 보낸 값, 읽고 바로 되돌림
		bool bCritical = GetIncomingCritical() > 0.f;
		SetIncomingCritical(0.f);
		
		// 상성 적용
		ENUElementAffinity Affinity = static_cast<ENUElementAffinity>(
			FMath::RoundToInt(GetIncomingElementAffinity())
			);
		SetIncomingElementAffinity(0.f);

		if (ActualDamage > 0.f)
		{
			UNUAbilitySystemComponent* HitASC = Cast<UNUAbilitySystemComponent>(&Data.Target);
			if (HitASC != nullptr && HitASC->GetBattleEncounter() != nullptr)
			{
				INUBattleEventInterface* Handler = HitASC->GetBattleEncounter()->GetBattleEventHandler();
				if (Handler != nullptr && Handler->InterceptDamage(HitASC->GetBattleEncounter(), HitASC, ActualDamage)) { return; }
			}

			const float DamageBeforeShield = ActualDamage;

			// 보호막이 체력보다 먼저 닳고 남은 만큼만 체력으로 내려감
			// 도트와 반사도 같은 IncomingDamage 로 들어오므로 그것들도 여기서 같이 막힘
			const float Absorbed = IsValid(HitASC) ? HitASC->AbsorbWithShield(ActualDamage) : 0.f;
			ActualDamage -= Absorbed;

			// 흡혈이 실제로 깎인 양을 알아야 해서 감산 전 체력을 들고 있음
			const float HealthBefore = GetHealth();

			const float MaxHealthNow = GetMaxHealth();

			SetHealth(FMath::Clamp(HealthBefore - ActualDamage, 0.f, MaxHealthNow));

			// 인정 임계치가 있는 적(대련 보스)은 그 아래로 내려가지 않음, 쓰러뜨리지 않고 인정 연출 뒤 승리로 끝냄
			// (ANUBattleEncounter::TryFinishByConcession) 피해 숫자는 실제 피해 그대로 띄움
			if (IsValid(HitASC) && HitASC->GetBattleEncounter() != nullptr)
			{
				const float ConcessionFloor = HitASC->GetBattleEncounter()->GetConcessionHealthFloor(HitASC);
				if (ConcessionFloor > 0.f && GetHealth() < ConcessionFloor)
				{
					SetHealth(FMath::Min(ConcessionFloor, HealthBefore));
				}
			}

			// 죽을 피해를 한 번 버티는 패시브와 직업 줄 (2835 마지막 기회 · 3027 불굴 · 3448 불멸)
			// 판정 기준이 맞기 전 체력이라 깎기 전 비율을 넘겨야 함
			// 여기서 살려두면 아래 HandleDeath 가 체력을 보고 지나감
			if (GetHealth() <= KINDA_SMALL_NUMBER && MaxHealthNow > KINDA_SMALL_NUMBER)
			{
				// 버틴 뒤 남는 체력은 줄마다 다름 (3448 불멸은 최대 HP 30%)
				float SurvivalHealth = 1.f;
				
				if (IsValid(HitASC)
					&& HitASC->ConsumeDeathPrevention(HealthBefore / MaxHealthNow, SurvivalHealth))
				{
					SetHealth(FMath::Clamp(SurvivalHealth, 1.f, MaxHealthNow));
				}
			}

			// 보호막이 전부 먹으면 피해 0이 떠서 안 맞은 것처럼 보이므로 흡수량을 띄움
			if (ActualDamage <= KINDA_SMALL_NUMBER && Absorbed > KINDA_SMALL_NUMBER)
			{
				ShowCombatPopup(Data, FNUCombatPopupData(
					ENUCombatPopupType::ShieldAbsorb, FMath::RoundToInt(Absorbed))
					);
			}
			else
			{
				ShowCombatPopup(Data, FNUCombatPopupData(
					bCritical ? ENUCombatPopupType::Critical : ENUCombatPopupType::Damage,
					FMath::RoundToInt(ActualDamage),
					Affinity)
					);
			}

			// 오버킬만큼은 회복하지 않는다 - 1 남은 적을 500으로 때렸을 때 250을 회복하면 안 됨
			// 보호막에 막힌 양도 빠짐
			// 넷 전투는 서버가 흡혈을 Heal 이벤트로 보내므로 이 계산을 타지 않는다
			ApplyLifesteal(Data, FMath::Min(ActualDamage, HealthBefore));

			// 맞고 나서 거는 피격시 패시브 (2801 점액)
			// 여기는 피해 GE 가 실제로 적용될 때 한 번 도는 자리라 타격마다 한 번씩 판정함
			UNUAbilitySystemComponent* HitAttackerASC = Cast<UNUAbilitySystemComponent>(
				Data.EffectSpec.GetEffectContext().GetInstigatorAbilitySystemComponent());

			if (IsValid(HitASC) && IsValid(HitAttackerASC) && HitASC != HitAttackerASC)
			{
				HitASC->TriggerOnHitTakenPassives(HitAttackerASC);

				// 반사는 때린 쪽이 든 디버프라 때린 쪽에게 되돌아감
				HitAttackerASC->TriggerReflectOnAttack(DamageBeforeShield);
				
				// 직업 자원의 기본 축적 — 여기는 피해가 실제로 들어간 자리라 타격마다 한 번 호출함
				// 쌓는 쪽과 담기는 쪽이 다른 자원을 구분함 (마크 · 약점은 맞는 쪽에 붙음)
				// 맞는 쪽은 지금 행동하는 중이 아니라, 자신의 행동 컨텍스트로 눌러선 안 됨
				HitASC->TriggerJobResourceGain(ENUJobResourceGainTrigger::OnHitTaken, nullptr, false);
				HitAttackerASC->TriggerJobResourceGain(ENUJobResourceGainTrigger::OnAttackHit, HitASC);
				
				// 단발 판정은 타수가 하나일 때만, 표가 "단발 공격 적중 시" 라고 적은 자리다
				// 행동 밖에서 들어온 피해(도트 · 반사)는 타수가 0 이라 빠짐
				if (HitAttackerASC->GetCurrentActionHitCount() == 1)
				{
					HitAttackerASC->TriggerJobResourceGain(ENUJobResourceGainTrigger::OnSingleHit, HitASC);
				}
				
				if (Data.EffectSpec.CapturedSourceTags.GetSpecTags().HasTagExact(NU::Damage::Type::Magical))
				{
					HitAttackerASC->TriggerJobResourceGain(ENUJobResourceGainTrigger::OnMagicDamage, HitASC);
				}
			}
		}
		bHealthChanged = true;
	}
	else if (Data.EvaluatedData.Attribute == GetIncomingHealingAttribute())
	{
		float ActualHealing = GetIncomingHealing();
		SetIncomingHealing(0.f);

		// 받는 회복량 증감 (3034 극점 -30%), 스킬 · 아이템 · 흡혈 · 턴 회복이 전부 이 자리를 지남
		if (const UNUAbilitySystemComponent* HealedASC =
			Cast<UNUAbilitySystemComponent>(GetOwningAbilitySystemComponent()))
		{
			ActualHealing *= HealedASC->GetHealReceivedMultiplier();
		}

		if (ActualHealing > 0.f)
		{
			SetHealth(FMath::Clamp(GetHealth() + ActualHealing, 0.f, GetMaxHealth()));

			ShowCombatPopup(Data, FNUCombatPopupData(
				ENUCombatPopupType::Heal, FMath::RoundToInt(ActualHealing))
				);
		}
		bHealthChanged = true;
	}
	else if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		// 도트, 디버프 등으로 직접 감소하는 경우 처리
		SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
		bHealthChanged = true;
	}
	else if (Data.EvaluatedData.Attribute == GetSPAttribute())
	{
		// 패링/회피/매직 실드, 반복 기본 공격 등 SP 소모 GameplayEffect가 직접 감산한 뒤 클램프한다.
		SetSP(FMath::Clamp(GetSP(), 0.f, GetMaxSP()));
	}

	if (bHealthChanged)
	{
		HandleDeath(Data);	
	}
}

void UNUVitalAttributeSet::ApplyLifesteal(const FGameplayEffectModCallbackData& Data, float DamageDealt)
{
	if (DamageDealt <= 0.f)
	{
		return;
	}

	// 시전자를 먼저 잡음, 흡혈 패시브가 비율에 더해지므로 비율보다 앞서야 함
	// 회복 GE·큐·팝업은 스킬 회복과 같은 경로를 쓰므로 SendHeal 을 그대로 부름
	// (SendHeal 이 죽은 대상은 걸러내므로 시전자가 반격에 쓰러진 경우도 안전함)
	UNUAbilitySystemComponent* SourceASC = Cast<UNUAbilitySystemComponent>(
		Data.EffectSpec.GetContext().GetOriginalInstigatorAbilitySystemComponent());

	if (!IsValid(SourceASC))
	{
		return;
	}

	// 스킬이 실어 보낸 비율에 공격시 흡혈 패시브(2806)를 더함, 같은 축이라 한 숫자로 합침
	const float Ratio = Data.EffectSpec.GetSetByCallerMagnitude(
		NU::Data::Damage::Lifesteal, false, 0.f)
		+ SourceASC->SumLifestealBonus();

	if (Ratio <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	SourceASC->SendHeal(SourceASC, SourceASC,
		ENUStatusAffectedStat::Health, DamageDealt * Ratio, false);
}

void UNUVitalAttributeSet::ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetMaxHealthAttribute() || Attribute == GetMaxSPAttribute())
	{
		NewValue = FMath::Max(1.f, NewValue);
	}
	else if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
	else if (Attribute == GetSPAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxSP());
	}
}

void UNUVitalAttributeSet::AdjustVitalToMaxChange(const FGameplayAttribute& VitalAttribute, float CurrentVital,
	float OldMax, float NewMax)
{
	UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();

	// PostAttributeChange는 복제 경로에서도 호출되므로 서버만 처리
	if (!ASC || !ASC->IsOwnerActorAuthoritative()) return;

	// 현재 값이 최대치보다 크면, 최대치로 낮춤
	if (CurrentVital > NewMax)
	{
		ASC->SetNumericAttributeBase(VitalAttribute, NewMax);
		return;
	}

	// 지금 당장은 최대치를 깎는 디버프를 걸면 풀렸을때 로직상 따라서 회복될 수 있음
	// 해당 디버프 추가하려면 바꿔야함
	
	// 최대치가 증가하기 전에 가득 차 있었다면 새 최대치까지 채운다 (장비 장착 등)
	// 초기화 시점에선 현재치, 최대치가 둘다 0이라 최대값을 적용할때 현재값이 따라갈 수 있으므로 차단
	// OldMax 가 0이면 초기화
	if (NewMax > OldMax && OldMax > KINDA_SMALL_NUMBER 
		&& FMath::IsNearlyEqual(CurrentVital, OldMax))
	{
		ASC->SetNumericAttributeBase(VitalAttribute, NewMax);
	}
}

void UNUVitalAttributeSet::ShowCombatPopup(const FGameplayEffectModCallbackData& Data,
	const FNUCombatPopupData& PopupData) const
{
	// 반올림해서 0이 된 미세 수치는 띄우지 않음
	if (PopupData.Amount <= 0) return;

	// 전투 진입 시 Encounter가 SetCurrentAvatar로 넣어둔 전투 아바타가 들어 있음
	if (ANUBattleAvatar* Avatar = Cast<ANUBattleAvatar>(Data.Target.GetAvatarActor()))
	{
		Avatar->MulticastShowCombatPopup(PopupData);
	}
}

void UNUVitalAttributeSet::HandleDeath(const FGameplayEffectModCallbackData& Data)
{
	// 사망 판정
	if (GetHealth() > KINDA_SMALL_NUMBER) return;
	if (Data.Target.HasMatchingGameplayTag(NU::State::Dead)) return;
	
	AActor* Killer = Data.EffectSpec.GetEffectContext().GetInstigator();
	AActor* DeadActor = Data.Target.GetAvatarActor();
	
	// 처치시 패시브를 받을 쪽, DeadEvent 처리 중에 사라질 수 있어 미리 잡아둠
	UNUAbilitySystemComponent* KillerASC = Cast<UNUAbilitySystemComponent>(
		Data.EffectSpec.GetEffectContext().GetInstigatorAbilitySystemComponent());
		
	FGameplayEventData Payload;
	Payload.Instigator = Killer;
	Payload.Target = DeadActor;
		
	// 사망한 액터에게 DeadEvent 전송
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		DeadActor,
		NU::Event::Dead,
		Payload
		);
	
	// 처치자에게 처치시 패시브를 알림 (2839 사냥본능)
	// 사망 판정이 전투원당 한 번만 도는 자리라 여기서 부름
	// 자기 피해로 쓰러진 것은 처치가 아니고, 도트로 죽은 경우의 처치자는 그 도트를 건 쪽
	if (IsValid(KillerASC) && KillerASC != &Data.Target)
	{
		KillerASC->ApplyOnKillPassives();
	}

	// 쓰러진 쪽의 전투불능시 패시브 (2802 포자 살포)
	// 2835 마지막 기회로 살아남으면 이 함수가 체력을 보고 위에서 이미 돌아가므로 여기까지 안 온다
	if (UNUAbilitySystemComponent* DeadASC = Cast<UNUAbilitySystemComponent>(&Data.Target))
	{
		DeadASC->TriggerOnDefeatedPassives();

		// 쓰러진 쪽이 든 자원(문양)은 "가장 많이 쌓인 적" 판정에서 빠지므로, 그걸 세던 상대의 스탯 줄을 다시 셈 (3207)
		// 사망으로 사라진 문양은 지맥 붕괴를 켜지 않음
		DeadASC->RefreshOpponentResourceScaledStats(NAME_None, true);
	}
}

void UNUVitalAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, Health, OldValue);
}

void UNUVitalAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, MaxHealth, OldValue);
}

void UNUVitalAttributeSet::OnRep_SP(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, SP, OldValue);
}

void UNUVitalAttributeSet::OnRep_MaxSP(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(ThisClass, MaxSP, OldValue);
}
