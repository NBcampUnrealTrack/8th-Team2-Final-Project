#include "Battle/Data/NUJobSkillContext.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Battle/NUBattleActionContext.h"
#include "Battle/Data/NUBattleDataSettings.h"
#include "Battle/Data/NUBattleDataSubsystem.h"
#include "Battle/Data/NUBattleSkillInfo.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "NUGAS/NUGameplayTags.h"
#include "UObject/StrongObjectPtr.h"

// 직업 시작 스킬 중 표만으로 안 돌던 둘 — 3302 점괘 투사(카드별 피해 줄) · 3203 나이아 개화(남은 SP 전부)
// 실제 표(DT_JobSkillDefinition · DT_JobSkillEffect)와 설정(DefaultGame.ini)을 읽어 행동 조립을 확인함
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNUJobStartingSkillContextTest, "ProjectM.Battle.JobSkill.StartingSkillContext",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNUJobStartingSkillContextTest::RunTest(const FString& Parameters)
{
	// 표는 게임 인스턴스 서브시스템이 들고 있어 시험용 인스턴스를 띄움
	TStrongObjectPtr<UGameInstance> GameInstance(NewObject<UGameInstance>(GEngine));
	GameInstance->InitializeStandalone();
	ON_SCOPE_EXIT
	{
		GameInstance->Shutdown();
	};

	const UNUBattleDataSubsystem* Data = GameInstance->GetSubsystem<UNUBattleDataSubsystem>();
	if (!TestNotNull(TEXT("전투 데이터 서브시스템"), Data))
	{
		return false;
	}

	// 3302 점괘 투사 — 그 턴 카드의 줄이 계수를 정하고 마법 피해 (대길 260 · 길 190 · 흉 130)
	const TPair<const TCHAR*, float> CardCases[] = {
		{ TEXT("대길"), 2.6f }, { TEXT("길"), 1.9f }, { TEXT("흉"), 1.3f } };

	for (const TPair<const TCHAR*, float>& Case : CardCases)
	{
		FNUBattleActionContext Context;
		const bool bBuilt = NUJobSkill::BuildActionContext(3302, *Data, Context, FName(Case.Key));

		TestTrue(FString::Printf(TEXT("3302 %s 조립"), Case.Key), bBuilt);
		TestEqual(FString::Printf(TEXT("3302 %s 계수"), Case.Key), Context.DamageCoefficient, Case.Value, 0.001f);
		TestTrue(FString::Printf(TEXT("3302 %s 마법 피해"), Case.Key), Context.DamageType == NU::Damage::Type::Magical);
		TestEqual(FString::Printf(TEXT("3302 %s 피해 항목 수"), Case.Key), Context.Effects.Num(), 1);
	}

	// 카드가 없으면 기준 줄이 없어 조립하지 않음 (뽑기 전 · 넷 전투)
	{
		FNUBattleActionContext Context;
		TestFalse(TEXT("3302 카드 없음"), NUJobSkill::BuildActionContext(3302, *Data, Context));
	}

	// 3203 나이아 개화 — 피해 줄이 없어도 설정으로 돌고, 계수는 쓴 SP 로 정해짐
	{
		FNUBattleActionContext Context;
		TestTrue(TEXT("3203 조립"), NUJobSkill::BuildActionContext(3203, *Data, Context));
		TestTrue(TEXT("3203 남은 SP 전부"), Context.bSpendAllSP);
		TestTrue(TEXT("3203 마법 피해"), Context.DamageType == NU::Damage::Type::Magical);
		TestTrue(TEXT("3203 중앙으로"), Context.Motion == ENUBattleActionMotion::MoveToCenter);
		TestEqual(TEXT("3203 피해 항목 수"), Context.Effects.Num(), 1);
	}

	// 표 설명의 예시 — SP 40 -> 402% · 59 -> 654% · 71 -> 824%
	if (const FNUSpendAllSPSkill* Bloom = NUJobSkill::FindSpendAllSPSkill(3203))
	{
		TestEqual(TEXT("3203 SP 40"), NUJobSkill::ComputeSpendAllSPCoefficient(*Bloom, 40), 4.02f, 0.01f);
		TestEqual(TEXT("3203 SP 59"), NUJobSkill::ComputeSpendAllSPCoefficient(*Bloom, 59), 6.54f, 0.01f);
		TestEqual(TEXT("3203 SP 71"), NUJobSkill::ComputeSpendAllSPCoefficient(*Bloom, 71), 8.24f, 0.01f);
		TestEqual(TEXT("3203 SP 0"), NUJobSkill::ComputeSpendAllSPCoefficient(*Bloom, 0), 0.f);
	}
	else
	{
		AddError(TEXT("설정에 3203 의 SpendAllSPSkills 줄이 없음"));
	}

	// 메뉴 — 두 스킬 모두 실행할 수 있고 피해를 줌
	for (const int32 SkillId : { 3203, 3302 })
	{
		FNUBattleSkillInfo Info;
		TestTrue(FString::Printf(TEXT("%d 스킬 정보"), SkillId), Data->TryGetBattleSkillInfo(SkillId, Info));
		TestTrue(FString::Printf(TEXT("%d 메뉴에 오름"), SkillId), Info.bExecutable);
		TestTrue(FString::Printf(TEXT("%d 피해를 줌"), SkillId), Info.bDealsDamage);
	}

	// 기존 스킬은 그대로 — 3002 발현 170% · 3216 연쇄 폭발 320%(약점 5 면 440%) · 3325 태양 436%(대길이면 872%)
	{
		FNUBattleActionContext Context;
		TestTrue(TEXT("3002 조립"), NUJobSkill::BuildActionContext(3002, *Data, Context));
		TestEqual(TEXT("3002 계수"), Context.DamageCoefficient, 1.7f, 0.001f);
	}
	{
		FNUBattleActionContext Context;
		TestTrue(TEXT("3216 조립"), NUJobSkill::BuildActionContext(3216, *Data, Context, FName(TEXT("대길"))));
		TestEqual(TEXT("3216 계수"), Context.DamageCoefficient, 3.2f, 0.001f);
		TestTrue(TEXT("3216 약점 교체 계수"), Context.Effects.ContainsByPredicate([](const FNUAbilityEffectEntry& Entry)
		{
			return FMath::IsNearlyEqual(Entry.ConditionalCoefficient, 4.4f, 0.001f);
		}));
	}
	{
		FNUBattleActionContext Plain;
		FNUBattleActionContext Great;
		TestTrue(TEXT("3325 조립"), NUJobSkill::BuildActionContext(3325, *Data, Plain, FName(TEXT("길"))));
		TestTrue(TEXT("3325 대길 조립"), NUJobSkill::BuildActionContext(3325, *Data, Great, FName(TEXT("대길"))));
		TestEqual(TEXT("3325 길 계수"), Plain.DamageCoefficient, 4.36f, 0.001f);
		TestEqual(TEXT("3325 대길 계수"), Great.DamageCoefficient, 8.72f, 0.001f);
	}

	return true;
}

#endif
