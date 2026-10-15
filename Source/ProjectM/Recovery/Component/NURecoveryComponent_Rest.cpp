#include "Recovery/Component/NURecoveryComponent.h"

#include "Audio/NUSFXSubsystem.h"
#include "Audio/NUSFXTags.h"
#include "Battle/Net/NUBattleNetSubsystem.h"
#include "CharacterState/NUPlayerState.h"
#include "CharacterState/NUSpiritState.h"
#include "Naia/Component/NUNaiaRosterComponent.h"
#include "NUGAS/Attribute/NUVitalAttributeSet.h"
#include "NUGAS/GameplayEffect/NUGE_FullRecovery.h"
#include "NUGAS/NUAbilitySystemComponent.h"
#include "NUGAS/NUGameplayTags.h"
#include "Recovery/Data/NURecoverySettings.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

// 로컬 실행용 대상 검증과 HP 회복
// 온라인 회복량과 대상 판정은 서버가 담당한다.
bool UNURecoveryComponent::GatherHealthTargets(TArray<UNUAbilitySystemComponent*>& OutTargets, bool& bNeedsRecovery) const
{
	bNeedsRecovery = false;
	OutTargets.Reset();
	const ANUPlayerState* PS = GetPlayerState();
	if (!IsValid(PS) || IsUsingExternalServer() || HasPendingDeath()) { return false; }
	const UGameInstance* GI = GetWorld()->GetGameInstance();
	const UNUBattleNetSubsystem* Battle = GI ? GI->GetSubsystem<UNUBattleNetSubsystem>() : nullptr;
	if (Battle && Battle->IsInBattle()) { return false; }
	OutTargets.Add(PS->GetNUASC());
	const UNUNaiaRosterComponent* Roster = PS->GetNaiaRosterComponent();
	if (!IsValid(Roster)) { return false; }
	for (ANUSpiritState* Spirit : Roster->GetParty())
	{
		if (!IsValid(Spirit)) { return false; }
		OutTargets.Add(Spirit->GetNUASC());
	}
	for (UNUAbilitySystemComponent* ASC : OutTargets)
	{
		if (!IsValid(ASC) || !ASC->IsAlive() || ASC->GetBattleEncounter() != nullptr) { return false; }
		const float Health = ASC->GetNumericAttribute(UNUVitalAttributeSet::GetHealthAttribute());
		const float MaxHealth = ASC->GetNumericAttribute(UNUVitalAttributeSet::GetMaxHealthAttribute());
		if (!FMath::IsFinite(Health) || !FMath::IsFinite(MaxHealth) || Health <= 0.0f || MaxHealth <= 0.0f) { return false; }
		bNeedsRecovery |= Health < MaxHealth;
	}
	for (const FNUNaiaInstance& Entry : Roster->GetStorage())
	{
		if (!FMath::IsFinite(Entry.HealthRatio) || Entry.HealthRatio <= 0.0f || Entry.HealthRatio > 1.0f) { return false; }
		bNeedsRecovery |= Entry.HealthRatio < 1.0f;
	}
	return true;
}

bool UNURecoveryComponent::CanRecoverHealth() const
{
	TArray<UNUAbilitySystemComponent*> Targets;
	bool bNeedsRecovery = false;
	return GatherHealthTargets(Targets, bNeedsRecovery) && bNeedsRecovery;
}

bool UNURecoveryComponent::RecoverHealth(float HealthRatio, TSubclassOf<UGameplayEffect> EffectClass, AActor* Source)
{
	if (bApplyingHealth || !CanExecuteAuthorityRecovery() || !EffectClass || !FMath::IsFinite(HealthRatio) || HealthRatio <= 0.0f) { return false; }
	TGuardValue<bool> ApplyingGuard(bApplyingHealth, true);
	TArray<UNUAbilitySystemComponent*> Targets;
	bool bNeedsRecovery = false;
	if (!GatherHealthTargets(Targets, bNeedsRecovery)) { return false; }
	TArray<FGameplayEffectSpecHandle> Specs;
	TArray<float> ExpectedHealth;
	TArray<float> OriginalBaseHealth;
	// 모든 대상과 Spec을 먼저 검증한다. 외부 서버의 결제 트랜잭션을 대체하지 않는다.
	for (UNUAbilitySystemComponent* ASC : Targets)
	{
		if (!IsValid(ASC->GetOwnerActor()) || !ASC->GetOwnerActor()->HasAuthority()) { return false; }
		const float MaxHealth = ASC->GetNumericAttribute(UNUVitalAttributeSet::GetMaxHealthAttribute());
		const float Health = ASC->GetNumericAttribute(UNUVitalAttributeSet::GetHealthAttribute());
		const float TargetHealth = FMath::Min(MaxHealth, Health + MaxHealth * HealthRatio);
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddSourceObject(Source);
		FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(EffectClass, 1.0f, Context);
		if (!Spec.IsValid()) { return false; }
		Spec.Data->SetSetByCallerMagnitude(NU::Data::Recovery::Health, TargetHealth);
		Specs.Add(Spec);
		ExpectedHealth.Add(TargetHealth);
		OriginalBaseHealth.Add(ASC->GetNumericAttributeBase(UNUVitalAttributeSet::GetHealthAttribute()));
	}
	for (int32 Index = 0; Index < Targets.Num(); ++Index)
	{
		Targets[Index]->ApplyGameplayEffectSpecToSelf(*Specs[Index].Data.Get());
		if (!FMath::IsNearlyEqual(Targets[Index]->GetNumericAttribute(UNUVitalAttributeSet::GetHealthAttribute()), ExpectedHealth[Index]))
		{
			// 로컬 HP 적용 실패 시 이미 적용한 HP를 복구한다. 실패한 요청에는 비용을 청구하지 않는다.
			for (int32 Applied = 0; Applied <= Index; ++Applied) { Targets[Applied]->SetNumericAttributeBase(UNUVitalAttributeSet::GetHealthAttribute(), OriginalBaseHealth[Applied]); }
			return false;
		}
	}
	return GetPlayerState()->GetNaiaRosterComponent()->RecoverStorageHealth(HealthRatio);
}

TArray<int64> UNURecoveryComponent::GetRestRosterIds() const
{
	TArray<int64> Ids;
	const ANUPlayerState* PS = GetPlayerState();
	const UNUNaiaRosterComponent* Roster = PS ? PS->GetNaiaRosterComponent() : nullptr;
	if (!Roster) { return Ids; }
	for (ANUSpiritState* Spirit : Roster->GetParty()) { Ids.Add(IsValid(Spirit) ? Spirit->GetInstanceId() : 0); }
	Ids.Add(0); // 파티와 보관함 경계. 실제 인스턴스 ID는 0을 사용하지 않는다.
	for (const FNUNaiaInstance& Entry : Roster->GetStorage()) { Ids.Add(Entry.InstanceId); }
	return Ids;
}

// 로컬 무료 휴식 시작 / 경과 시간 정산
bool UNURecoveryComponent::StartRest(AActor* Source)
{
	const float Duration = GetDefault<UNURecoverySettings>()->FullRecoveryDurationSeconds;
	if (bResting || !CanExecuteAuthorityRecovery() || !CanRecoverHealth() || !IsValid(Source) || !FMath::IsFinite(Duration) || Duration < 1.0f) { return false; }
	APawn* Pawn = GetPlayerState()->GetPawn();
	if (!IsValid(Pawn)) { return false; }
	RestPawn = Pawn;
	RestSource = Source;
	RestRosterIds = GetRestRosterIds();
	GetPlayerState()->GetNaiaRosterComponent()->OnRosterChanged.AddUniqueDynamic(this, &ThisClass::HandleRestRosterChanged);
	LastRestTime = GetWorld()->GetTimeSeconds();
	bResting = true;
	SetComponentTickEnabled(true);
	GetOwner()->ForceNetUpdate();
	UNUSFXSubsystem::Play(this, NU::SFX::Field::Rest);
	Notify(NSLOCTEXT("NURecovery", "RestStarted", "무료 휴식을 시작했습니다."));
	return true;
}

bool UNURecoveryComponent::ApplyRestElapsed()
{
	const double Now = GetWorld()->GetTimeSeconds();

	// 틱 간격이 지연되어도 실제 경과 시간만큼 회복한다.
	const double Elapsed = FMath::Max(0.0, Now - LastRestTime);
	LastRestTime = Now;
	const float Duration = GetDefault<UNURecoverySettings>()->FullRecoveryDurationSeconds;
	return FMath::IsFinite(Duration) && Duration >= 1.0f && (Elapsed <= 0.0 || RecoverHealth(static_cast<float>(Elapsed / Duration), UNUGE_FullRecovery::StaticClass(), RestSource.Get()));
}

void UNURecoveryComponent::StopRest()
{
	if (!CanExecuteAuthorityRecovery()) { return; }
	FinishRest(true);
}

void UNURecoveryComponent::FinishRest(bool bApplyRemainder)
{
	if (!bResting) { return; }
	bResting = false;
	// 사망/전투/외부 서버 전환 시에는 추가 회복을 적용하지 않는다.
	if (bApplyRemainder && CanExecuteAuthorityRecovery() && RestSource.IsValid() && GetRestRosterIds() == RestRosterIds) { ApplyRestElapsed(); }
	SetComponentTickEnabled(false);
	GetPlayerState()->GetNaiaRosterComponent()->OnRosterChanged.RemoveDynamic(this, &ThisClass::HandleRestRosterChanged);
	RestSource.Reset();
	RestPawn.Reset();
	RestRosterIds.Reset();
	GetOwner()->ForceNetUpdate();
	Notify(NSLOCTEXT("NURecovery", "RestStopped", "무료 휴식을 종료했습니다."));
}

void UNURecoveryComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bResting) { return; }
	if (!CanExecuteAuthorityRecovery() || !RestSource.IsValid() || !RestPawn.IsValid() || GetPlayerState()->GetPawn() != RestPawn.Get()
		|| !GetPlayerState()->GetNUASC()->IsAlive() || GetPlayerState()->GetNUASC()->GetBattleEncounter() != nullptr)
	{
		FinishRest(false);
		return;
	}

	// 1초 이상 누적되면 실제 경과 시간만큼 HP를 갱신한다.
	if (GetWorld()->GetTimeSeconds() - LastRestTime >= 1.0)
	{
		if (!ApplyRestElapsed() || !CanRecoverHealth()) { FinishRest(false); }
	}
}

void UNURecoveryComponent::HandleRestRosterChanged()
{
	if (bResting && GetRestRosterIds() != RestRosterIds) { FinishRest(false); }
}
