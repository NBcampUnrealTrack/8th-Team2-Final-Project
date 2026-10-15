// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Battle/NUBattleTypes.h"
#include "Battle/Formation/NUBattleFormationTypes.h"

class AActor;
class UNUBattleDataSubsystem;

// 넷 전투가 행동 연출을 정하는 규칙, 넷 액션 큐와 VFX 뷰어가 같이 씀
// 뷰어가 게임과 다르게 보이면 폴리싱 판단이 틀어지므로 규칙을 한 곳에만 둠
namespace NUBattlePresentation
{
	// 진영별 넷 프리셋(DefaultGame.ini NetAllyActionMotion · NetEnemyActionMotion)을 스펙에 채움
	PROJECTM_API void ApplyNetPreset(FNUBattleActionMotionSpec& Spec, bool bIsAlly, int32 HitCount);

	// 기본 행동 어빌리티(NetDefaultActionAbility)의 기본 연출을 깔고,
	// 피해 줄이 없는 스킬은 제자리, DT_SkillVFX 행이 있으면 세 칸과 색을 덮어씀
	// 스킬 표 행을 찾았으면 true, SkillId 가 0 이하면 기본 연출만 깔고 false
	PROJECTM_API bool ApplySkillPresentation(FNUBattleActionMotionSpec& Spec, const UNUBattleDataSubsystem* Data,
		int32 SkillId);

	// 전투원이 진형에서 서는 자리(아바타의 HomeTransform), 아바타가 아니면 지금 위치
	// 행동 위치 · 투사체 도착점은 이 값으로 잡음 - 지금 위치를 쓰면 아직 돌아오는 중인 전투원(단계 전환마다 몇 프레임씩
	// 늦게 돌아옴)을 읽어 다음 행동의 임팩트 · 중앙이 어긋남
	PROJECTM_API FVector GetFormationLocation(const AActor* Actor);

	// 넷 전투의 행동 위치, 진형 중심으로 전진 방향을 잡음
	// 전진 방향을 못 만들면(두 진형 중심이 수평으로 겹침) 제자리
	PROJECTM_API FVector ResolveNetActionLocation(
		ENUBattleActionMotion Motion,
		const FVector& SourceWorld,
		bool bSourceIsAlly,
		const FVector& AllyCenter,
		const FVector& EnemyCenter,
		const FVector& TargetWorld,
		bool bHasTarget,
		const FVector& ArenaLocation
		);

	// 자기 임팩트가 없는 공격에 대체 임팩트(NUBattleDataSettings::FallbackImpactVFX)를 채움, 투사체가 없으면 운반체도 채움
	// 제자리 행동(버프 · 회복 · 아이템)과 대상이 없는 행동(도주)은 건드리지 않음
	// 아바타가 직업 기본 공격 연출로 바꾼 뒤(ResolveActionVFX)에 불러야 직업 임팩트를 덮지 않음
	PROJECTM_API void ApplyFallbackImpact(FNUBattleActionMotionSpec& Spec);

	// 첫 타격까지의 시간과 행동 전체 길이, 예고 · 접근 · 행동 · 반복 타격 · 복귀를 더함
	PROJECTM_API void ComputeTimeline(const FNUBattleActionMotionSpec& Spec, float& OutFirstHitDelay,
		float& OutTotalDuration);

	// 즉발 GE 가 부르는 것과 같은 방식으로 전투 큐를 띄움 (피격 · 회복 등)
	// 큐 블루프린트가 Instigator 위치로 밀려날 방향을 정하므로 비우면 대상 자신을 넣음
	PROJECTM_API void ExecuteBattleCue(AActor* Target, AActor* Instigator, const FGameplayTag& CueTag, float Magnitude);

	// 세트에 진형 자리가 없을 때의 대체 오프셋(아레나 기준), 로컬 인카운터의 하드코딩 오프셋과 같은 값
	PROJECTM_API FVector GetFallbackFormationOffset(ENUBattleFormationRow Row, int32 SlotIndex);
}
