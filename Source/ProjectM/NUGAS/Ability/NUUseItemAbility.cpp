// Fill out your copyright notice in the Description page of Project Settings.


#include "NUUseItemAbility.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Battle/Avatar/NUBattleAvatar.h"
#include "Battle/Encounter/NUBattleEncounter.h"
#include "Inventory/NUInventoryTags.h"
#include "NUGAS/NUAbilitySystemComponent.h"
#include "NUGAS/NUGameplayTags.h"

UNUUseItemAbility::UNUUseItemAbility()
{
	AbilityId = NU::Ability::Battle::UseItem;
	
	FAbilityTriggerData TriggerData;
	TriggerData.TriggerTag = NU::Ability::Battle::UseItem;
	TriggerData.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(TriggerData);
	
	TargetPolicy = ENUTargetPolicy::SingleAlly;	// 실제 타겟팅 정책은 아이템 정의로 판정됨
	ActionPriority = 0;
	Motion = ENUBattleActionMotion::InPlace;
	
	ApproachTime = 0.f;
	ActionTime = 0.6f;
	ReturnTime = 0.f;
	HitTimeNormalized = 0.5f;
	
	ItemMagnitude = 0.f;
}

void UNUUseItemAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// Super의 ActivateAbility를 처리하기 전에 아이템 정의
	if (TriggerEventData)
	{
		if (const UClass* PayloadClass = Cast<UClass>(TriggerEventData->OptionalObject.Get()))
		{
			ItemEffectClass = const_cast<UClass*>(PayloadClass);
		}
		ItemMagnitude = TriggerEventData->EventMagnitude;

		// 아이템 분류, 교감 아이템이면 GE 대신 포획 판정으로
		ItemTags = TriggerEventData->InstigatorTags;
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

// 이 어빌리티는 모든 아이템이 공용으로 쓰므로 TargetPolicy가 SingleAlly로 고정되어 있음
// 실제 타겟 정책은 아이템 정의에 있고, 인카운터가 그걸로 대상을 확정해 EventData.Target 으로 보내고 있음
// 여기서 정책으로 다시 해결하면 교감석처럼 적을 겨냥하는 아이템이 아군에게 적용됨, 기존엔 해당 문제 발생해서 수정함
void UNUUseItemAbility::ResolveEffectTargets(TArray<AActor*>& OutTargets) const
{
	OutTargets.Reset();

	if (IsValid(CachedTargetActor))
	{
		OutTargets.Add(CachedTargetActor);
		return;
	}

	// 명령을 낸 뒤 대상이 사라진 경우, 기존 ApplyRuntimeEffect 폴백과 같은 규칙으로 시전자에게
	if (AActor* SelfActor = GetAvatarActorFromActorInfo())
	{
		OutTargets.Add(SelfActor);
	}
}

void UNUUseItemAbility::ApplyRuntimeEffect(AActor* TargetActor)
{
	// 교감 아이템은 적용할 GE가 없음
	// 포획 판정이 효과로 작용함
	if (ItemTags.HasTag(NU::Item::Commune))
	{
		ApplyCaptureEffect(TargetActor);
		return;
	}

	if (!ItemEffectClass)
	{
		UE_LOG(LogTemp, Error, TEXT("UseItem: ItemEffectClass가 없습니다 (OptionalObject 페이로드 확인)"));
		return;
	}
	
	UNUAbilitySystemComponent* SourceASC = 
		Cast<UNUAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo());
	if (!IsValid(SourceASC))
	{
		UE_LOG(LogTemp, Error, TEXT("UseItem: SourceASC 없음"));
		return;
	}
	
	AActor* ActualTarget = IsValid(TargetActor) 
		? TargetActor
		: GetAvatarActorFromActorInfo();
	
	UNUAbilitySystemComponent* TargetASC = Cast<UNUAbilitySystemComponent>(
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(ActualTarget));
	if (!IsValid(TargetASC))
	{
		UE_LOG(LogTemp, Error, TEXT("UseItem: TargetASC 없음 (Target=%s)"), *GetNameSafe(ActualTarget));
		return;
	}
	
	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(ItemEffectClass, GetAbilityLevel());
	if (!SpecHandle.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("UseItem: SpecHandle 생성 실패 (GE=%s)"), *GetNameSafe(ItemEffectClass));
		return;
	}
	
	SpecHandle.Data->SetSetByCallerMagnitude(NU::Data::Item::Heal, ItemMagnitude);
	SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
}

void UNUUseItemAbility::ApplyCaptureEffect(AActor* TargetActor)
{
	ANUBattleEncounter* Encounter = GetBattleEncounter();
	ANUBattleAvatar* SourceAvatar = Cast<ANUBattleAvatar>(GetAvatarActorFromActorInfo());
	ANUBattleAvatar* TargetAvatar = Cast<ANUBattleAvatar>(TargetActor);

	if (!IsValid(Encounter) || !SourceAvatar || !IsValid(TargetAvatar))
	{
		UE_LOG(LogTemp, Error, TEXT("UseItem: 포획에 필요한 정보가 없습니다 (Target=%s)"),
			*GetNameSafe(TargetActor));
		return;
	}

	// 판정과 실행은 인카운터가 처리함
	// ItemMagnitude 는 아이템 표의 UseMagnitude, 교감석이면 표기 포획률(5/10/20/50)임
	TArray<bool> Shakes;
	ENUCaptureResult Result = Encounter->TryCapture(
		SourceAvatar->GetCombatantId(), TargetAvatar->GetCombatantId(), ItemMagnitude, Shakes);

	// 확률을 굴렸으면(흔들림이 있으면) 결과 글자는 아바타가 흔들림 끝에 띄움
	// 자리 없음·포획 불가 같은 경우 Shakes가 비어있으므로 여기서 바로 띄움
	if (Shakes.Num() <= 0)
	{
		TargetAvatar->MulticastShowCombatPopup(
			FNUCombatPopupData(ENUCombatPopupType::CaptureFail));
	}

	// 실패 시 로그
	if (Result != ENUCaptureResult::Success)
	{
		UE_LOG(LogTemp, Warning, TEXT("포획 실패 : Result=%d, CombatantId=%d, 흔들림 %d회"),
			static_cast<int32>(Result), TargetAvatar->GetCombatantId(), Shakes.Num());
	}
}
