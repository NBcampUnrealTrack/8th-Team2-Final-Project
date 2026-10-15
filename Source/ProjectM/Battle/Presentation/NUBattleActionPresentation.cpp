// Fill out your copyright notice in the Description page of Project Settings.

#include "Battle/Presentation/NUBattleActionPresentation.h"

#include "AbilitySystemGlobals.h"
#include "Battle/Avatar/NUBattleAvatar.h"
#include "GameplayCueManager.h"
#include "Battle/Data/NUActiveSkillDefinition.h"
#include "Battle/Data/NUBattleDataSettings.h"
#include "Battle/Data/NUBattleDataSubsystem.h"
#include "Battle/Data/NUBattleSkillInfo.h"
#include "Battle/Data/NUSkillVFXProfile.h"
#include "Battle/Formation/NUBattleFormationMath.h"
#include "GameFramework/Actor.h"
#include "NUGAS/Ability/NUBattleGameplayAbility.h"

namespace
{
	// 넷 전투의 중앙 이동 · 대상 접근 거리, 로컬 인카운터(ANUBattleEncounter)의 기본값과 같음
	constexpr float NetActionLineOffset = 80.f;
	constexpr float NetTargetApproachDistance = 120.f;
}

void NUBattlePresentation::ApplyNetPreset(FNUBattleActionMotionSpec& Spec, bool bIsAlly, int32 HitCount)
{
	const UNUBattleDataSettings* Settings = GetDefault<UNUBattleDataSettings>();
	const FNUNetActionMotionPreset& Preset = bIsAlly
		? Settings->NetAllyActionMotion
		: Settings->NetEnemyActionMotion;

	Spec.Motion = Preset.Motion;
	Spec.TelegraphTime = Preset.TelegraphTime;
	Spec.ApproachTime = Preset.ApproachTime;
	Spec.ActionTime = Preset.ActionTime;
	Spec.ReturnTime = Preset.ReturnTime;
	Spec.HitTimeNormalized = Preset.HitTimeNormalized;
	Spec.HitCount = HitCount;
	Spec.HitInterval = Preset.HitInterval;
}

bool NUBattlePresentation::ApplySkillPresentation(FNUBattleActionMotionSpec& Spec, const UNUBattleDataSubsystem* Data,
	int32 SkillId)
{
	const UNUBattleDataSettings* Settings = GetDefault<UNUBattleDataSettings>();

	// 로컬 전투(UNUBattleGameplayAbility::BuildActionMotionSpec)과 같은 순서로,
	// 어빌리티 CDO의 기본 연출을 먼저 깔고 DT_SkillVFX 행이 있으면 그 위에 덮어씀
	// 기본 공격의 검기와 공격 애니메이션은 표에 행이 없어 이 기본값이 유일함
	if (UClass* DefaultAbilityClass = Settings->NetDefaultActionAbility.LoadSynchronous())
	{
		if (const UNUBattleGameplayAbility* DefaultAbility = DefaultAbilityClass->GetDefaultObject<UNUBattleGameplayAbility>())
		{
			DefaultAbility->ApplyPresentationDefaults(Spec);
		}
	}

	if (!IsValid(Data) || SkillId <= 0)
	{
		return false;
	}

	// 피해 줄이 하나도 없으면 중앙으로 가지 않음 (버프 · 디버프 · 회복), 나이아 · 직업 스킬 모두
	FNUBattleSkillInfo SkillInfo;
	if (Data->TryGetBattleSkillInfo(SkillId, SkillInfo) && !SkillInfo.bDealsDamage)
	{
		Spec.Motion = ENUBattleActionMotion::InPlace;
	}

	const FNUSkillVFXProfile* Profile = Data->FindSkillVFXProfile(SkillId);
	if (!Profile)
	{
		return false;
	}

	// 속성색은 나이아 · 보스 정의에만 있음, 직업 스킬은 행의 고정색으로 감
	const FNUActiveSkillDefinition* Definition = Data->FindActiveSkillDefinition(SkillId);

	Spec.CastVFX = Profile->CastVFX;
	Spec.ProjectileVFX = Profile->ProjectileVFX;
	Spec.ImpactVFX = Profile->ImpactVFX;
	Spec.bHasSkillVFXProfile = true;	// 직업 기본 공격 연출보다 이김
	Spec.EffectColor = Profile->bUseElementColor && Definition
		? Settings->ResolveElementColor(Definition->AttackElement)
		: Profile->FixedColor;
	Spec.AttackElement = Definition ? Definition->AttackElement : FGameplayTag();

	return true;
}

FVector NUBattlePresentation::ResolveNetActionLocation(ENUBattleActionMotion Motion, const FVector& SourceWorld,
	bool bSourceIsAlly, const FVector& AllyCenter, const FVector& EnemyCenter, const FVector& TargetWorld,
	bool bHasTarget, const FVector& ArenaLocation)
{
	if (Motion == ENUBattleActionMotion::InPlace)
	{
		return SourceWorld;
	}

	FVector Advance = FVector::ZeroVector;
	if (!NUBattleFormation::ResolveAdvanceDirection(AllyCenter, EnemyCenter, bSourceIsAlly, Advance))
	{
		return SourceWorld;
	}

	// 대상이 없는 접근은 중앙으로 다시 요청함 (NUBattleFormation::ResolveActionLocation 의 약속)
	const ENUBattleActionMotion ResolvedMotion = Motion == ENUBattleActionMotion::MoveToTarget && !bHasTarget
		? ENUBattleActionMotion::MoveToCenter
		: Motion;

	return NUBattleFormation::ResolveActionLocation(
		ResolvedMotion,
		SourceWorld,
		bHasTarget ? TargetWorld : FVector::ZeroVector,
		bHasTarget,
		ArenaLocation,
		Advance,
		NetActionLineOffset,
		NetTargetApproachDistance
		);
}

FVector NUBattlePresentation::GetFormationLocation(const AActor* Actor)
{
	if (const ANUBattleAvatar* Avatar = Cast<ANUBattleAvatar>(Actor))
	{
		return Avatar->GetHomeTransform().GetLocation();
	}

	return IsValid(Actor) ? Actor->GetActorLocation() : FVector::ZeroVector;
}

void NUBattlePresentation::ApplyFallbackImpact(FNUBattleActionMotionSpec& Spec)
{
	if (!Spec.ImpactVFX.IsNull()
		|| Spec.Motion == ENUBattleActionMotion::InPlace
		|| Spec.ProjectileTargetLocation.IsNearlyZero())
	{
		return;
	}

	const UNUBattleDataSettings* Settings = GetDefault<UNUBattleDataSettings>();
	if (Settings->FallbackImpactVFX.IsNull())
	{
		return;
	}

	Spec.ImpactVFX = Settings->FallbackImpactVFX;
	if (Spec.ProjectileVFX.IsNull())
	{
		Spec.ProjectileVFX = Settings->FallbackProjectileVFX;
	}
}

void NUBattlePresentation::ComputeTimeline(const FNUBattleActionMotionSpec& Spec, float& OutFirstHitDelay,
	float& OutTotalDuration)
{
	// 제자리 행동은 아바타가 접근 단계를 건너뛰므로(ANUBattleAvatar::BeginActionMotion) 접근 시간을 더하지 않음
	// 더하면 피격 큐 · 회복 숫자가 모션보다 접근 시간만큼 늦게 뜸
	const float ApproachTime = Spec.Motion == ENUBattleActionMotion::InPlace ? 0.f : Spec.ApproachTime;

	OutFirstHitDelay = Spec.TelegraphTime
		+ ApproachTime
		+ Spec.ActionTime * FMath::Clamp(Spec.HitTimeNormalized, 0.f, 1.f);

	OutTotalDuration = Spec.TelegraphTime
		+ ApproachTime
		+ Spec.ActionTime
		+ (FMath::Max(1, Spec.HitCount) - 1) * Spec.HitInterval
		+ Spec.ReturnTime;
}

void NUBattlePresentation::ExecuteBattleCue(AActor* Target, AActor* Instigator, const FGameplayTag& CueTag,
	float Magnitude)
{
	if (!IsValid(Target))
	{
		return;
	}

	UGameplayCueManager* CueManager = UAbilitySystemGlobals::Get().GetGameplayCueManager();
	if (!IsValid(CueManager))
	{
		return;
	}

	// 큐 블루프린트가 EffectContext 의 Instigator 위치로 밀려날 방향을 정하므로 반드시 채움
	// 비워 두면 방향이 0이 되어 밀려남만 조용히 사라짐(나머지 연출은 그대로 나옴)
	// 행동자를 못 찾으면 대상 자신을 넣어 예전 폴백과 같은 결과가 되게 함
	AActor* ResolvedInstigator = IsValid(Instigator) ? Instigator : Target;

	FGameplayEffectContextHandle Context(UAbilitySystemGlobals::Get().AllocGameplayEffectContext());
	Context.AddInstigator(ResolvedInstigator, ResolvedInstigator);

	FGameplayCueParameters Parameters;
	Parameters.EffectContext = Context;
	Parameters.Instigator = ResolvedInstigator;
	Parameters.EffectCauser = ResolvedInstigator;
	Parameters.Location = Target->GetActorLocation();

	// 로컬에서는 IncomingDamage·IncomingHealing 어트리뷰트가 실리는 자리
	Parameters.RawMagnitude = Magnitude;

	// 즉발 GE 가 부르는 것과 같은 이벤트 종류
	CueManager->HandleGameplayCue(Target, CueTag, EGameplayCueEvent::Executed, Parameters);
}

FVector NUBattlePresentation::GetFallbackFormationOffset(ENUBattleFormationRow Row, int32 SlotIndex)
{
	// 로컬 인카운터(ANUBattleEncounter)의 GetAllySpawnTransform/GetEnemySpawnTransform 하드코딩 오프셋과 같은 값
	if (Row == ENUBattleFormationRow::Enemy)
	{
		return FVector(-SlotIndex * 220.f, -300.f, 50.f);
	}

	const float FormationY = Row == ENUBattleFormationRow::Naia ? 230.f : 450.f;
	return FVector(0.f, FormationY, 50.f);
}
