// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

struct FNUBattleActionContext;
struct FNUJobSkillDefinition;
struct FNUJobSkillEffectRow;
struct FNUSpendAllSPSkill;
class UNUBattleDataSubsystem;

// 직업 스킬 정의와 효과 행을 행동 컨텍스트로 조립하는 곳
// NUActiveSkill 과 같은 일을 하지만 읽는 표가 달라 분리함
namespace NUJobSkill
{
	// 이 스킬을 표만으로 실행할 수 있는지
	// 메뉴에 올릴지 정할 때와 실제로 조립할 때 똑같이 처리해야해서 분리함
	PROJECTM_API bool CanRunFromTable(const FNUJobSkillDefinition& Definition,
		const TArray<FNUJobSkillEffectRow>* Effects);

	// 아군에게 거는 스킬(단일 · 전체 아군)이 아군 쪽 줄을 받을 사람이 있는지
	// 솔로(캐릭터 + 나이아)는 Ally 줄(자신 · 나이아 제외)을 받을 사람이 없어 SP 와 턴만 버림, 그런 스킬은 메뉴에서 뺌 (3413 · 3428 · 3429)
	// 적을 때리는 스킬은 아군 줄이 덤이라 막지 않음. 두 인자는 자신을 뺀 살아 있는 같은 편 기준
	PROJECTM_API bool HasAllySideRecipient(const FNUJobSkillDefinition& Definition,
		const TArray<FNUJobSkillEffectRow>* Effects, bool bOtherCharacterAlive, bool bNaiaAlive);

	// 기준이 되는 피해 줄의 계수, 1.0 = 1배, 피해 줄이 없으면 0
	// CasterCard 는 시전자가 이번 턴 뽑은 카드, 그 카드의 피해 줄이 있으면 그것이 기준 (3302 점괘 투사)
	PROJECTM_API float FindDamageCoefficient(const TArray<FNUJobSkillEffectRow>* Effects, FName CasterCard = NAME_None);

	// 시전자가 뽑은 카드로 계수가 갈리는 피해 줄인지 (3302 점괘 투사 — 대길 · 길 · 흉 줄 중 그 턴 카드의 하나만 돎)
	PROJECTM_API bool IsCardDamageRow(const FNUJobSkillEffectRow& Row);

	// 남은 SP 를 전부 쓰는 직업 스킬이면 그 설정 (3203 나이아 개화), 아니면 nullptr
	// 표에 칸이 없어 설정(NU Battle Data 의 SpendAllSPSkills)이 정함
	PROJECTM_API const FNUSpendAllSPSkill* FindSpendAllSPSkill(int32 SkillId);

	// 남은 SP 를 전부 쓴 스킬의 계수, 1.0 = 1배 — 평시 계수표(쓴 SP)에 스킬 배율을 곱함 (SP 40 -> 402%)
	PROJECTM_API float ComputeSpendAllSPCoefficient(const FNUSpendAllSPSkill& Skill, int32 SpentSP);

	// 효과 줄 중 피해 줄이 하나라도 있는지, 없으면 중앙으로 안 나가고 제자리에서 씀 (버프 · 디버프 · 회복)
	// 로컬 행동 조립과 넷 재생이 같은 판단을 하도록 한 곳에 둠
	PROJECTM_API bool HasDamageRow(const TArray<FNUJobSkillEffectRow>* Effects);

	// 전투 하나에서 이 스킬을 쓸 수 있는 횟수, 0 = 제한 없음
	// 스킬의 어느 줄에 적혀 있든 가장 큰 값을 씀, 안 도는 줄에만 한도가 있는 스킬도 한도는 지켜야 함 (3228 심층 명상)
	PROJECTM_API int32 FindMaxUsesPerBattle(const TArray<FNUJobSkillEffectRow>* Effects);

	// 데이터로 실행할 수 있는 스킬이면 true
	// 실패하면 Out 은 부르는 쪽이 채워 둔 값 그대로 남는다
	// CasterCard 는 시전자가 이번 턴 뽑은 카드, 카드로 계수가 갈리는 스킬이 그 카드의 줄을 고름
	PROJECTM_API bool BuildActionContext(int32 SkillId, const UNUBattleDataSubsystem& Data,
		FNUBattleActionContext& Out, FName CasterCard = NAME_None);
}
