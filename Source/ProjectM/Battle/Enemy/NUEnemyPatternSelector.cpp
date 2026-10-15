// Fill out your copyright notice in the Description page of Project Settings.


#include "Battle/Enemy/NUEnemyPatternSelector.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Battle/Enemy/NUEnemyPatternTypes.h"
#include "Battle/Encounter/NUBattleEncounter.h"
#include "NUGAS/NUAbilitySystemComponent.h"
#include "NUGAS/Attribute/NUVitalAttributeSet.h"

namespace
{
	// 상대 진영에 이 태그를 가진 살아있는 전투원이 있는지 확인
	bool AnyOpponentHasTag(const FNUEnemyPatternContext& Context, const FGameplayTag& Tag)
	{
		if (!IsValid(Context.Encounter))
		{
			return false;
		}

		const ENUBattleTeam OpponentTeam = Context.SourceTeam == ENUBattleTeam::Ally
			? ENUBattleTeam::Enemy
			: ENUBattleTeam::Ally;

		TArray<int32> OpponentIds;
		Context.Encounter->GetLivingCombatantIds(OpponentTeam, OpponentIds);

		for (int32 OpponentId : OpponentIds)
		{
			FNUBattleParticipant Opponent;
			if (!Context.Encounter->GetParticipant(OpponentId, Opponent))
			{
				continue;
			}

			const UAbilitySystemComponent* OpponentASC =
				UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Opponent.ASCOwner);

			if (IsValid(OpponentASC) && OpponentASC->HasMatchingGameplayTag(Tag))
			{
				return true;
			}
		}

		return false;
	}

	FNUEnemySelectedAction MakeSelectedAction(const FNUEnemyPatternRule& Rule)
	{
		FNUEnemySelectedAction Result;
		Result.AbilityId = Rule.AbilityId;
		Result.SkillId = Rule.SkillId;
		Result.bOverrideParryTiming = Rule.bOverrideParryTiming;
		Result.ChallengeDuration = Rule.ChallengeDuration;
		Result.SuccessAnchor = Rule.SuccessAnchor;
		Result.SuccessWindow = Rule.SuccessWindow;
		return Result;
	}
	
	FNUEnemyActionKey MakeActionKey(const FNUEnemyPatternRule& Rule)
	{
		return FNUEnemyActionKey(Rule.AbilityId, Rule.SkillId);
	}
	
	// 패턴의 조건이 만족하는지 확인함
	bool CheckCondition(const FNUEnemyPatternContext& Context, const FNUEnemyPatternCondition& Condition)
	{
		bool bResult = false;

		switch (Condition.Type)
		{
		case ENUEnemyConditionType::SelfHPRatioBelow:
		case ENUEnemyConditionType::SelfHPRatioAbove:
			{
				const UNUVitalAttributeSet* Vital = Context.SourceASC->GetVitalAttributeSet();
				const float MaxHealth = IsValid(Vital) ? Vital->GetMaxHealth() : 0.f;

				// 판정할 수 없으면 bInvert 와 무관하게 후보에서 제외
				if (MaxHealth <= KINDA_SMALL_NUMBER)
				{
					return false;
				}

				const float Ratio = Vital->GetHealth() / MaxHealth;
				bResult = Condition.Type == ENUEnemyConditionType::SelfHPRatioBelow
					? Ratio <= Condition.Value
					: Ratio >= Condition.Value;
				break;
			}

		case ENUEnemyConditionType::TurnNumberAfter:
			bResult = Context.TurnNumber >= FMath::RoundToInt(Condition.Value);
			break;

		case ENUEnemyConditionType::TurnNumberBefore:
			bResult = Context.TurnNumber <= FMath::RoundToInt(Condition.Value);
			break;

		case ENUEnemyConditionType::SelfHasTag:
			if (!Condition.Tag.IsValid())
			{
				return false;
			}
			bResult = Context.SourceASC->HasMatchingGameplayTag(Condition.Tag);
			break;

		case ENUEnemyConditionType::AnyOpponentHasTag:
			if (!Condition.Tag.IsValid())
			{
				return false;
			}
			bResult = AnyOpponentHasTag(Context, Condition.Tag);
			break;
		}

		return Condition.bInvert ? !bResult : bResult;
	}

	// 패턴을 적용하기 위한 모든 조건을 확인 (체력 50퍼 이하 & 특정 태그 필요 등)
	// 하나라도 만족하지 않으면 실패, 아예 없으면 통과
	bool AllConditionsPass(const FNUEnemyPatternContext& Context,
		const TArray<FNUEnemyPatternCondition>& Conditions)
	{
		for (const FNUEnemyPatternCondition& Condition : Conditions)
		{
			if (!CheckCondition(Context, Condition))
			{
				return false;
			}
		}

		return true;
	}

	// 현재 보스가 어떤 페이즈인지 결정함
	int32 ResolvePhaseIndex(const FNUEnemyPatternContext& Context)
	{
		int32 Result = INDEX_NONE;

		// 위에서부터 확인해 마지막으로 만족한 것이 현재 페이즈
		for (int32 Index = 0; Index < Context.Pattern->Phases.Num(); ++Index)
		{
			if (AllConditionsPass(Context, Context.Pattern->Phases[Index].EnterConditions))
			{
				Result = Index;
			}
		}

		// 한 번 올라간 페이즈는 되돌아가지 않음
		// 회복으로 진입 조건을 만족해도 다시 진입하지 않게
		return FMath::Max(Result, Context.Runtime->PhaseIndex);
	}

	// 적 공격 규칙 유효한지 확인
	bool IsRuleAvailable(const FNUEnemyPatternContext& Context, const FNUEnemyPatternRule& Rule,
		const FGameplayTag& CurrentPhaseTag)
	{
		// AbilityId, Weight 유효하지 않으면 false
		if (!Rule.AbilityId.IsValid() || Rule.Weight <= 0.f)
		{
			return false;
		}

		// 페이즈 태그를 비운 규칙은 모든 페이즈에서 후보
		if (Rule.PhaseTag.IsValid() && Rule.PhaseTag != CurrentPhaseTag)
		{
			return false;
		}

		// 쿨타임과 사용횟수가 같은 키를 확인하므로 한번만 만든 뒤 재사용함
		const FNUEnemyActionKey ActionKey = MakeActionKey(Rule);
		
		// 쿨타임이 있다면 
		if (Rule.CooldownTurns > 0)
		{
			if (int32* LastUsedTurn = Context.Runtime->LastUsedTurn.Find(ActionKey))
			{
				if (Context.TurnNumber - *LastUsedTurn < Rule.CooldownTurns)
				{
					return false;
				}
			}
		}

		// 한 전투에서 사용 가능한 횟수가 정해져있다면
		if (Rule.MaxUsesPerBattle > 0)
		{
			int32* UseCount = Context.Runtime->UseCount.Find(ActionKey);
			if (UseCount && *UseCount >= Rule.MaxUsesPerBattle)
			{
				return false;
			}
		}

		// 스킬 부여와 무관하게 스킬을 지금 갖고 있는지 확인
		// 액티브 스킬(SkillId>0)은 공용 실행기를 첫 실행 시점에 지연 부여하므로 이 검사 대상이 아님
		// 유효성은 실행 직전 ResolveActionContext 의 BuildActionContext 가 확인함
		if (Rule.SkillId == 0 
			&& !Context.SourceASC->FindBattleAbilityHandle(Rule.AbilityId).IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("적 패턴: 보유하지 않은 어빌리티라 후보에서 제외 : %s"),
				*Rule.AbilityId.ToString());
			return false;
		}
		
		return AllConditionsPass(Context, Rule.Conditions);
	}

	// 마지막 사용한 턴, 사용횟수 기록
	void RecordUse(const FNUEnemyPatternContext& Context, const FNUEnemyActionKey& ActionKey)
	{
		Context.Runtime->LastUsedTurn.Add(ActionKey, Context.TurnNumber);
		++Context.Runtime->UseCount.FindOrAdd(ActionKey);
	}
}

FNUEnemySelectedAction NUEnemyPattern::SelectAbility(const FNUEnemyPatternContext& Context)
{
	if (!Context.Pattern || !Context.Runtime || !IsValid(Context.SourceASC))
	{
		return FNUEnemySelectedAction();
	}

	// 1. 페이즈 갱신
	int32 NewPhaseIndex = ResolvePhaseIndex(Context);
	bool bPhaseChanged = NewPhaseIndex != Context.Runtime->PhaseIndex;
	Context.Runtime->PhaseIndex = NewPhaseIndex;

	FGameplayTag CurrentPhaseTag;
	if (Context.Pattern->Phases.IsValidIndex(NewPhaseIndex))
	{
		const FNUEnemyPhase& CurrentPhase = Context.Pattern->Phases[NewPhaseIndex];
		CurrentPhaseTag = CurrentPhase.PhaseTag;

		// 페이즈가 변했을때만 진입
		// 진입한 턴에 강제로 낼 행동이 있으면 규칙보다 먼저
		if (bPhaseChanged
			&& CurrentPhase.OnEnterAbilityId.IsValid()
			&& Context.SourceASC->FindBattleAbilityHandle(CurrentPhase.OnEnterAbilityId).IsValid())
		{
			RecordUse(Context, FNUEnemyActionKey(CurrentPhase.OnEnterAbilityId, 0));
			
			FNUEnemySelectedAction Result;
			Result.AbilityId = CurrentPhase.OnEnterAbilityId;
			return Result;
		}
	}

	// 2. 공격 패턴 후보 수집 후 최고 우선순위만 남김
	TArray<const FNUEnemyPatternRule*> Candidates;
	int32 BestPriority = MIN_int32;

	for (const FNUEnemyPatternRule& Rule : Context.Pattern->Rules)
	{
		if (!IsRuleAvailable(Context, Rule, CurrentPhaseTag))
		{
			continue;
		}

		if (Rule.Priority > BestPriority)
		{
			BestPriority = Rule.Priority;
			Candidates.Reset();
		}

		if (Rule.Priority == BestPriority)
		{
			Candidates.Add(&Rule);
		}
	}

	// 3. 같은 우선도 안에서 가중치 이용해서 선택
	if (!Candidates.IsEmpty())
	{
		float TotalWeight = 0.f;
		for (const FNUEnemyPatternRule* Candidate : Candidates)
		{
			TotalWeight += Candidate->Weight;
		}

		float Roll = FMath::FRand() * TotalWeight;
		for (const FNUEnemyPatternRule* Candidate : Candidates)
		{
			Roll -= Candidate->Weight;
			if (Roll <= 0.f)
			{
				RecordUse(Context, MakeActionKey(*Candidate));
				return MakeSelectedAction(*Candidate);
			}
		}

		// 부동소수 오차로 전부 순회해도 0.f 이상이 될 경우 마지막 선택
		RecordUse(Context, MakeActionKey(*Candidates.Last()));
		return MakeSelectedAction(*Candidates.Last());
	}

	// 4. 선택 가능한 패턴이 없다면 대체 행동
	// 대체 행동에는 쿨타임과 사용 횟수를 기록하지 않음
	const FGameplayTag& Fallback = Context.Pattern->FallbackAbilityId;
	if (Fallback.IsValid() && Context.SourceASC->FindBattleAbilityHandle(Fallback).IsValid())
	{
		FNUEnemySelectedAction Result;
		Result.AbilityId = Fallback;
		return Result;
	}

	return FNUEnemySelectedAction();
}
