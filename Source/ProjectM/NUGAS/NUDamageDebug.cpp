// 피해 계산 디버그 콘솔 명령 (nu.Damage.*) — 개발 전용, Shipping 제외.
// 등록 방식은 self-registering static (FAutoConsoleCommandWithWorldAndArgs), nu.Inv.* 와 같음.
//
// 합연산 풀은 여러 통로가 하나로 합쳐지는 자리라 화면만 봐서는 무엇이 얼마나 들어갔는지 알 수 없다.
// 값이 0이 나올 때 "패시브가 없는 것" 인지 "조건이 안 맞는 것" 인지 가르려고 만든 것이다.

#if !UE_BUILD_SHIPPING

#include "AbilitySystemBlueprintLibrary.h"
#include "Battle/Data/NUBattleDataSubsystem.h"
#include "Battle/Encounter/NUBattleEncounter.h"
#include "Battle/Data/NUActiveSkillDefinition.h"
#include "Battle/Data/NUJobSkillContext.h"
#include "Battle/Data/NUActiveSkillContext.h"
#include "Battle/Data/NUPassiveDefinition.h"
#include "Battle/Data/NUJobSkillDefinition.h"
#include "Battle/Data/NUBattleSkillInfo.h"
#include "Engine/GameInstance.h"
#include "CharacterProgression/Component/NUPlayerSkillComponent.h"
#include "CharacterProgression/Data/NUPlayerSkillTypes.h"
#include "CharacterState/NUPlayerState.h"
#include "CharacterState/NUSpiritState.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "NUGAS/Attribute/NUCombatAttributeSet.h"
#include "NUGAS/Attribute/NUVitalAttributeSet.h"
#include "NUGAS/GameplayEffect/NUStatusGameplayEffect.h"
#include "NUGAS/NUAbilitySystemComponent.h"
#include "NUGAS/NUGameplayTags.h"
#include "Naia/Component/NUNaiaRosterComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogNUDamageDebug, Log, All);

namespace NUDamageDebug
{

// 화면에 찍을 속성 목록. 상성표와 같은 여섯 개다
const TArray<TPair<const TCHAR*, FGameplayTag>>& GetElements()
{
	static const TArray<TPair<const TCHAR*, FGameplayTag>> Elements
	{
		{ TEXT("불"),   NU::Element::Fire },
		{ TEXT("물"),   NU::Element::Water },
		{ TEXT("풀"),   NU::Element::Grass },
		{ TEXT("번개"), NU::Element::Lightning },
		{ TEXT("땅"),   NU::Element::Earth },
		{ TEXT("바람"), NU::Element::Wind },
	};

	return Elements;
}

ANUPlayerState* FindLocalPlayerState(UWorld* World)
{
	if (!IsValid(World))
	{
		return nullptr;
	}

	APlayerController* Controller = World->GetFirstPlayerController();
	return IsValid(Controller) ? Cast<ANUPlayerState>(Controller->PlayerState) : nullptr;
}

// 캐릭터와 파티 나이아의 ASC 를 이름과 함께 모은다
void CollectLocalCombatants(UWorld* World, TArray<TPair<FString, UNUAbilitySystemComponent*>>& OutCombatants)
{
	ANUPlayerState* PlayerState = FindLocalPlayerState(World);
	if (!IsValid(PlayerState))
	{
		return;
	}

	if (UNUAbilitySystemComponent* ASC =
		Cast<UNUAbilitySystemComponent>(PlayerState->GetAbilitySystemComponent()))
	{
		OutCombatants.Emplace(TEXT("캐릭터"), ASC);
	}

	const UNUNaiaRosterComponent* Roster = PlayerState->GetNaiaRosterComponent();
	if (!IsValid(Roster))
	{
		return;
	}

	for (const TObjectPtr<ANUSpiritState>& Spirit : Roster->GetParty())
	{
		if (!IsValid(Spirit))
		{
			continue;
		}

		if (UNUAbilitySystemComponent* ASC =
			Cast<UNUAbilitySystemComponent>(Spirit->GetAbilitySystemComponent()))
		{
			OutCombatants.Emplace(FString::Printf(TEXT("나이아 %s"), *GetNameSafe(Spirit)), ASC);
		}
	}

	// 전투 중이면 적도 붙인다. 2833 동기화처럼 상대 목록을 읽는 것은
	// 적에게 패시브를 붙여보지 않으면 확인할 방법이 없다
	// 캐릭터 -> 나이아 -> 적 순서라 기본 선택(0번 = 캐릭터)은 그대로다
	if (OutCombatants.Num() == 0)
	{
		return;
	}

	const ANUBattleEncounter* Encounter = OutCombatants[0].Value->GetBattleEncounter();
	if (!IsValid(Encounter))
	{
		return;
	}

	TArray<int32> EnemyIds;
	Encounter->GetLivingCombatantIds(ENUBattleTeam::Enemy, EnemyIds);

	for (int32 CombatantId : EnemyIds)
	{
		FNUBattleParticipant Participant;
		if (!Encounter->GetParticipant(CombatantId, Participant))
		{
			continue;
		}

		if (UNUAbilitySystemComponent* ASC = Cast<UNUAbilitySystemComponent>(
				UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Participant.ASCOwner)))
		{
			OutCombatants.Emplace(FString::Printf(TEXT("적 %d"), CombatantId), ASC);
		}
	}
}

// 전투원 하나를 고른다. 인자 어디에 naia / enemy 가 있어도 찾는다
// 자리로 찾지 않는 이유는 앞 인자가 선택이라 "0.3 naia" 와 "0.3 2 naia" 가 둘 다 들어오기 때문
// 아무 것도 안 붙이면 캐릭터
UNUAbilitySystemComponent* PickCombatant(UWorld* World, const TArray<FString>& Args,
	const TCHAR* CommandName, FString* OutLabel = nullptr)
{
	TArray<TPair<FString, UNUAbilitySystemComponent*>> Combatants;
	CollectLocalCombatants(World, Combatants);

	if (Combatants.Num() == 0)
	{
		UE_LOG(LogNUDamageDebug, Warning, TEXT("[%s] 전투원을 찾지 못했다"), CommandName);
		return nullptr;
	}

	auto HasKeyword = [&Args](const TCHAR* Keyword)
	{
		return Args.ContainsByPredicate([Keyword](const FString& Arg)
		{
			return Arg.Equals(Keyword, ESearchCase::IgnoreCase);
		});
	};

	const TCHAR* Prefix = nullptr;
	const TCHAR* NotFound = nullptr;

	if (HasKeyword(TEXT("naia")))
	{
		Prefix = TEXT("나이아");
		NotFound = TEXT("파티에 나이아가 없다");
	}
	else if (HasKeyword(TEXT("enemy")))
	{
		Prefix = TEXT("적");
		NotFound = TEXT("전투 중이 아니거나 살아 있는 적이 없다");
	}

	int32 Index = 0;

	if (Prefix)
	{
		Index = Combatants.IndexOfByPredicate(
			[Prefix](const TPair<FString, UNUAbilitySystemComponent*>& Entry)
			{
				return Entry.Key.StartsWith(Prefix);
			});

		if (Index == INDEX_NONE)
		{
			UE_LOG(LogNUDamageDebug, Warning, TEXT("[%s] %s"), CommandName, NotFound);
			return nullptr;
		}
	}

	if (OutLabel)
	{
		*OutLabel = Combatants[Index].Key;
	}

	return Combatants[Index].Value;
}

void DumpCombatant(UWorld* World, const FString& Label, UNUAbilitySystemComponent* ASC)
{
	UGameInstance* GameInstance = IsValid(World) ? World->GetGameInstance() : nullptr;
	const UNUBattleDataSubsystem* Data =
		IsValid(GameInstance) ? GameInstance->GetSubsystem<UNUBattleDataSubsystem>() : nullptr;

	UE_LOG(LogNUDamageDebug, Display, TEXT("--- %s ---"), *Label);

	// 저체력 조건이 걸렸는지 안 걸렸는지를 가르려면 지금 비율을 같이 봐야 한다
	if (const UNUVitalAttributeSet* Vital = ASC->GetVitalAttributeSet())
	{
		const float MaxHealth = Vital->GetMaxHealth();
		const float Ratio = MaxHealth > KINDA_SMALL_NUMBER ? Vital->GetHealth() / MaxHealth : 0.f;
		UE_LOG(LogNUDamageDebug, Display, TEXT("  HP %.0f / %.0f (%.0f%%)"),
			Vital->GetHealth(), MaxHealth, Ratio * 100.f);
		
		// 2834 에너지 순환은 아이콘이 안 뜬다 — 상한이 올라갔다 내려오는 것을 이 줄로만 볼 수 있다
		UE_LOG(LogNUDamageDebug, Display, TEXT("  SP %.0f / %.0f"), Vital->GetSP(), Vital->GetMaxSP());
	}

	const TArray<int32>& PassiveIds = ASC->GetPassiveSkillIds();
	if (PassiveIds.Num() == 0)
	{
		UE_LOG(LogNUDamageDebug, Display, TEXT("  패시브 없음"));
	}

	// ★ 정의표의 값 칸은 읽지 않는다.
	//   패시브가 새 효과표로 옮겨간 뒤로 정의표의 값·속성·조건 칸은 아무도 안 읽고,
	//   게다가 단위까지 다르다 — 정의표는 0.05, 효과표는 5 다.
	//   그 칸을 찍으면 "표에는 이렇게 적혀 있는데 왜 다르게 도나" 로 사람을 헤매게 한다.
	for (int32 PassiveId : PassiveIds)
	{
		const FNUPassiveDefinition* Definition =
			IsValid(Data) ? Data->FindPassiveDefinition(PassiveId) : nullptr;

		UE_LOG(LogNUDamageDebug, Display, TEXT("  패시브 %d %s"), PassiveId,
			Definition ? *Definition->KoreanName.ToString() : TEXT("(정의 없음)"));

		for (const FNUOwnedPassiveEffect& Owned : ASC->GetOwnedPassiveEffects())
		{
			if (Owned.PassiveId != PassiveId)
			{
				continue;
			}

			const FNUPassiveEffectRow& Effect = Owned.Row;

			// 확률은 사건이 일어나는 자리에서만 굴리므로, 여기서는 표에 적힌 값을 그대로 보여준다
			UE_LOG(LogNUDamageDebug, Display,
				TEXT("    #%d %s: 값 %g%s(계산값 %g), 속성 %s, 상태이상 %d, 발동 확률 %.0f%%"),
				Effect.EffectIndex,
				*UEnum::GetDisplayValueAsText(Effect.EffectType).ToString(),
				Effect.Value, Effect.bIsPercent ? TEXT("%") : TEXT(""), Effect.GetScaledValue(),
				Effect.Element.IsValid() ? *Effect.Element.ToString() : TEXT("없음"),
				Effect.StatusEffectId,
				Effect.TriggerChance * 100.f);
		}

		// 전투당 1회 패시브는 남았는지 다 썼는지가 화면에 안 보인다
		if (ASC->HasUsedOncePerBattle(PassiveId))
		{
			UE_LOG(LogNUDamageDebug, Display, TEXT("    -> 전투당 1회: 이번 전투에서 이미 씀"));
		}
	}

	UE_LOG(LogNUDamageDebug, Display, TEXT("  상시 주는 피해 +%.1f%%"),
		ASC->SumAlwaysDamageBonus() * 100.f);

	// 전투당 1회 상태(3027 불굴 · 3044)는 아이콘이 없어 켜졌는지 · 끝났는지 여기서만 보인다
	for (const int32 SkillId : ASC->GetUsedJobOnceStates())
	{
		UE_LOG(LogNUDamageDebug, Display, TEXT("  전투당 1회 상태 %d : %s"), SkillId,
			ASC->IsJobOnceStateActive(SkillId) ? TEXT("켜짐") : TEXT("끝남 — 이번 전투에 다시 안 켜짐"));
	}

	// 액티브 전투당 N회 — 한도는 표가, 쓴 횟수는 ASC 가 들고 있음 (3310 역방향 · 3328 대역전 · 3329 은자의 등불 · 3228 심층 명상)
	for (const TPair<int32, int32>& Use : ASC->GetActiveSkillUseCounts())
	{
		const int32 MaxUses = IsValid(Data)
			? NUJobSkill::FindMaxUsesPerBattle(Data->FindJobSkillEffects(Use.Key))
			: 0;

		if (MaxUses > 0)
		{
			UE_LOG(LogNUDamageDebug, Display, TEXT("  액티브 전투당 N회 %d : %d/%d"), Use.Key, Use.Value, MaxUses);
		}
	}

	// 자원이 켜는 상태 (3104 번아웃) — 아이콘이 없어 켜졌는지 · 언제까지인지 여기서만 보인다
	for (const FNUJobStateInstance& State : ASC->GetJobStates())
	{
		UE_LOG(LogNUDamageDebug, Display, TEXT("  상태 %s : %s (%d~%d턴)"), *State.StateName.ToString(),
			ASC->IsJobStateActive(State.StateName) ? TEXT("켜짐") : TEXT("꺼짐 — 대기 중이거나 지난 뒤"),
			State.StartTurn, State.EndTurn);
	}

	// 상태를 켜는 "가장 많이 쌓인 적" 판정이 마지막으로 본 값 (3207 지맥 붕괴) - 1 이상에서 0 이 될 때 켜지므로 이 값이 기준
	for (const TPair<FName, int32>& Seen : ASC->GetLastSeenMostStacked())
	{
		UE_LOG(LogNUDamageDebug, Display, TEXT("  마지막으로 본 최다 %s : %d"), *Seen.Key.ToString(), Seen.Value);
	}

	// 공격마다 값이 갈려 어트리뷰트에 안 보이는 치명타 줄 (3015 잃은 HP · 3039 동조율)
	// 단발 같은 조건 줄은 공격 순간에만 켜져서 여기엔 안 잡힘
	const float PerAttackCritRate =
		ASC->SumJobPerAttackCriticalBonus(ENUStatusAffectedStat::CriticalRate, nullptr);
	const float PerAttackCritDamage =
		ASC->SumJobPerAttackCriticalBonus(ENUStatusAffectedStat::CriticalDamage, nullptr);

	if (PerAttackCritRate != 0.f || PerAttackCritDamage != 0.f)
	{
		UE_LOG(LogNUDamageDebug, Display, TEXT("  공격마다 붙는 치명타 확률 +%.1f%%p · 피해 +%.1f%%p (조건 줄 제외)"),
			PerAttackCritRate * 100.f, PerAttackCritDamage * 100.f);
	}

	// 속성별로 따로 물어야 어느 속성에서 켜지는지 보인다
	for (const TPair<const TCHAR*, FGameplayTag>& Element : GetElements())
	{
		FGameplayTagContainer AttackTags;
		AttackTags.AddTag(Element.Value);

		const float Bonus = ASC->SumElementPowerBonus(AttackTags);
		if (Bonus != 0.f)
		{
			UE_LOG(LogNUDamageDebug, Display, TEXT("  속성 위력 %s +%.1f%%"),
				Element.Key, Bonus * 100.f);
		}
	}

	const float Lifesteal = ASC->SumLifestealBonus();
	if (Lifesteal != 0.f)
	{
		UE_LOG(LogNUDamageDebug, Display, TEXT("  공격시 흡혈 +%.1f%%"), Lifesteal * 100.f);
	}

	// 받는 피해 쪽. 2838 민첩한 대응이 상대 속도와 견주므로 양쪽을 다 찍어야 켜지는지가 보인다
	const float SelfSpeed = ASC->GetNumericAttribute(UNUCombatAttributeSet::GetSpeedAttribute());

	UE_LOG(LogNUDamageDebug, Display,
		TEXT("  속도 %.0f | 받는 피해 배율 x%.3f (상대가 느릴 때) / x%.3f (상대가 빠를 때)"),
		SelfSpeed,
		ASC->CalcDamageTakenMultiplier(SelfSpeed - 1.f),
		ASC->CalcDamageTakenMultiplier(SelfSpeed + 1.f));

	// 보호막은 남은 양과 만료 턴이 따로 놀아서 총량만 봐서는 언제 사라질지 알 수 없다
	if (ASC->GetShields().Num() > 0)
	{
		UE_LOG(LogNUDamageDebug, Display, TEXT("  보호막 합계 %.0f"), ASC->GetShieldAmount());

		for (const FNUShieldInstance& Shield : ASC->GetShields())
		{
			UE_LOG(LogNUDamageDebug, Display, TEXT("    스킬 %d : %.0f 남음, %s"),
				Shield.SourceSkillId, Shield.Amount,
				Shield.RemainingTurns > 0
					? *FString::Printf(TEXT("%d턴"), Shield.RemainingTurns)
					: TEXT("전투 끝까지"));
		}
	}

	for (const FNUReflectInstance& Reflect : ASC->GetReflects())
	{
		UE_LOG(LogNUDamageDebug, Display, TEXT("  반사 스킬 %d : 가한 피해의 %.0f%%, %s"),
			Reflect.SourceSkillId, Reflect.Ratio * 100.f,
			Reflect.RemainingTurns > 0
				? *FString::Printf(TEXT("%d턴"), Reflect.RemainingTurns)
				: TEXT("전투 끝까지"));
	}

	// 속성 디버프도 상태이상 목록에 안 떠서 여기서만 보인다 (2109 수중 감옥)
	for (const FNUElementAfflictionInstance& Affliction : ASC->GetElementAfflictions())
	{
		UE_LOG(LogNUDamageDebug, Display, TEXT("  속성 디버프 스킬 %d : %s%s%s, %s"),
			Affliction.SourceSkillId,
			*Affliction.Element.ToString(),
			!FMath::IsNearlyEqual(Affliction.DamageTakenMultiplier, 1.f)
				? *FString::Printf(TEXT(" 받는 피해 x%.2f"), Affliction.DamageTakenMultiplier)
				: TEXT(""),
			Affliction.bBlocksSkill ? TEXT(" 스킬 봉인") : TEXT(""),
			Affliction.RemainingTurns > 0
				? *FString::Printf(TEXT("%d턴"), Affliction.RemainingTurns)
				: TEXT("전투 끝까지"));
	}

	// 자기 구속 상태는 아이콘도 없고 상태이상 목록에도 안 떠서 여기서만 보인다 (2210 개화)
	// 방출이 예약돼 있는지가 특히 중요함 — 안 걸려 있으면 그냥 두 턴을 버린 것이 된다
	for (const FNUSelfBoundState& State : ASC->GetSelfBoundStates())
	{
		UE_LOG(LogNUDamageDebug, Display, TEXT("  자기 구속 스킬 %d : %d턴 남음%s%s%s"),
			State.SourceSkillId, State.RemainingTurns,
			State.bBlocksAction ? TEXT(", 행동 불가") : TEXT(""),
			State.bReleaseOnEnd ? TEXT(", 풀릴 때 방출") : TEXT(""),
			State.Lifesteal > 0.f
				? *FString::Printf(TEXT(", 흡혈 %.0f%%"), State.Lifesteal * 100.f)
				: TEXT(""));
	}
}

static FAutoConsoleCommandWithWorldAndArgs GDumpBonusCmd(
	TEXT("nu.Damage.Bonus"),
	TEXT("합연산 풀에 들어가는 값을 전투원별로 찍는다: nu.Damage.Bonus"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		TArray<TPair<FString, UNUAbilitySystemComponent*>> Combatants;
		CollectLocalCombatants(World, Combatants);

		if (Combatants.Num() == 0)
		{
			UE_LOG(LogNUDamageDebug, Warning, TEXT("[nu.Damage.Bonus] 로컬 전투원을 찾지 못했다"));
			return;
		}

		for (const TPair<FString, UNUAbilitySystemComponent*>& Combatant : Combatants)
		{
			DumpCombatant(World, Combatant.Key, Combatant.Value);
		}

		// 대상에 따라 갈리는 직업 줄은 상대를 정해야 값이 나와서 상대마다 따로 찍는다
		// (3202 원소 지배 — 대상의 약점 1 · 3 · 5중첩, 3101 선행 — 속도 차이, 3219 · 3246 — 약점 5중첩 곱연산)
		bool bJobHeader = false;

		for (const TPair<FString, UNUAbilitySystemComponent*>& Attacker : Combatants)
		{
			for (const TPair<FString, UNUAbilitySystemComponent*>& Target : Combatants)
			{
				// 편이 다른 쌍만, 적은 이름이 "적" 으로 시작함 (CollectLocalCombatants)
				if (Attacker.Key.StartsWith(TEXT("적")) == Target.Key.StartsWith(TEXT("적")))
				{
					continue;
				}

				const float JobBonus = Attacker.Value->SumJobDamageBonus(Target.Value);
				const float JobMultiplier = Attacker.Value->GetJobDamageMultiplier(Target.Value);

				// 공격마다 붙는 치명타도 대상에 따라 갈림 (3201 약점 1당 +3%p)
				const float JobCritRate =
					Attacker.Value->SumJobPerAttackCriticalBonus(ENUStatusAffectedStat::CriticalRate, Target.Value);
				const float JobCritDamage =
					Attacker.Value->SumJobPerAttackCriticalBonus(ENUStatusAffectedStat::CriticalDamage, Target.Value);

				if (FMath::IsNearlyZero(JobBonus) && FMath::IsNearlyEqual(JobMultiplier, 1.f)
					&& FMath::IsNearlyZero(JobCritRate) && FMath::IsNearlyZero(JobCritDamage))
				{
					continue;
				}

				if (!bJobHeader)
				{
					UE_LOG(LogNUDamageDebug, Display, TEXT("--- 상대별 직업 주는 피해 ---"));
					bJobHeader = true;
				}

				UE_LOG(LogNUDamageDebug, Display,
					TEXT("  %s -> %s : 합연산 +%.1f%% · 곱연산 x%.3f · 공격마다 치명타 확률 +%.1f%%p · 피해 +%.1f%%p"),
					*Attacker.Key, *Target.Key, JobBonus * 100.f, JobMultiplier,
					JobCritRate * 100.f, JobCritDamage * 100.f);
			}
		}
	}));

// 자원표가 상태를 켜는 규칙을 들고 있어 상태 이름으로 자원을 찾음
static FAutoConsoleCommandWithWorldAndArgs GJobStateCmd(
	TEXT("nu.Damage.State"),
	TEXT("직업 자원이 켜는 상태를 지금 턴부터 자원표 기간만큼 켠다, 자원 값은 안 건드린다(테스트용): nu.Damage.State <상태 이름> [naia|enemy]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogNUDamageDebug, Warning,
				TEXT("[nu.Damage.State] 사용법: nu.Damage.State <상태 이름> [naia|enemy]  (예: 번아웃)"));
			return;
		}

		UGameInstance* GameInstance = IsValid(World) ? World->GetGameInstance() : nullptr;
		const UNUBattleDataSubsystem* Data =
			IsValid(GameInstance) ? GameInstance->GetSubsystem<UNUBattleDataSubsystem>() : nullptr;
		if (!IsValid(Data))
		{
			UE_LOG(LogNUDamageDebug, Warning, TEXT("[nu.Damage.State] 전투 데이터 서브시스템이 없다"));
			return;
		}

		const FName StateName(*Args[0]);
		const FNUJobResourceDefinition* Definition = nullptr;

		for (const TPair<FName, FNUJobResourceDefinition>& Pair : Data->GetJobResources())
		{
			if (Pair.Value.StateName == StateName)
			{
				Definition = &Pair.Value;
				break;
			}
		}

		if (!Definition)
		{
			UE_LOG(LogNUDamageDebug, Warning,
				TEXT("[nu.Damage.State] 자원표 StateName 에 없는 이름이다 : %s"), *Args[0]);
			return;
		}

		FString Label;
		UNUAbilitySystemComponent* ASC = PickCombatant(World, Args, TEXT("nu.Damage.State"), &Label);
		if (!IsValid(ASC))
		{
			return;
		}

		ASC->EnterJobState(*Definition, true);

		UE_LOG(LogNUDamageDebug, Display,
			TEXT("[nu.Damage.State] %s 에 상태 %s 를 켰다 — nu.Damage.Bonus"), *Label, *Args[0]);
	}));

static FAutoConsoleCommandWithWorldAndArgs GGrantPassiveCmd(
	TEXT("nu.Damage.GrantPassive"),
	TEXT("패시브를 임시로 붙인다(테스트용): nu.Damage.GrantPassive <PassiveId> [naia|enemy]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogNUDamageDebug, Warning,
				TEXT("[nu.Damage.GrantPassive] 사용법: nu.Damage.GrantPassive <PassiveId> [naia|enemy]"));
			return;
		}

		FString Label;
		UNUAbilitySystemComponent* ASC =
			PickCombatant(World, Args, TEXT("nu.Damage.GrantPassive"), &Label);

		if (!IsValid(ASC))
		{
			return;
		}

		const int32 PassiveId = FCString::Atoi(*Args[0]);

		// 목록을 통째로 다시 넣는 세터라 기존 것에 더해서 넘긴다
		TArray<int32> PassiveIds = ASC->GetPassiveSkillIds();
		PassiveIds.AddUnique(PassiveId);
		ASC->SetPassiveSkillIds(PassiveIds);

		UE_LOG(LogNUDamageDebug, Display, TEXT("[nu.Damage.GrantPassive] %s 에 패시브 %d 를 붙였다"),
			*Label, PassiveId);
	}));


static FAutoConsoleCommandWithWorldAndArgs GGrantSkillCmd(
	TEXT("nu.Damage.GrantSkill"),
	TEXT("액티브 스킬을 임시로 붙인다(테스트용): nu.Damage.GrantSkill <SkillId> [naia|enemy]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogNUDamageDebug, Warning,
				TEXT("[nu.Damage.GrantSkill] 사용법: nu.Damage.GrantSkill <SkillId> [naia|enemy]"));
			return;
		}
		
		FString Label;
		UNUAbilitySystemComponent* ASC =
			PickCombatant(World, Args, TEXT("nu.Damage.GrantSkill"), &Label);
		
		if (!IsValid(ASC))
		{
			return;
		}
		
		const int32 SkillId = FCString::Atoi(*Args[0]);
		
		// 표에 없는 번호를 붙이면 고를 수는 있는데 발동에서 조용히 떨어짐, 여기서 걸러줌
		UGameInstance* GameInstance = IsValid(World) ? World->GetGameInstance() : nullptr;
		const UNUBattleDataSubsystem* Data =
			IsValid(GameInstance) ? GameInstance->GetSubsystem<UNUBattleDataSubsystem>() : nullptr;
		
		const FNUActiveSkillDefinition* Definition =
			IsValid(Data) ? Data->FindActiveSkillDefinition(SkillId) : nullptr;
		
		if (!Definition)
		{
			UE_LOG(LogNUDamageDebug, Warning,
				TEXT("[nu.Damage.GrantSkill] 액티브 스킬 표에 없는 번호다 : %d"), SkillId);
			return;
		}
		
		// 나이아는 목록이 두 벌이라 한 번에 같이 넣고, 적은 ASC 목록만 보면 됨
		if (ANUSpiritState* Naia = Cast<ANUSpiritState>(ASC->GetOwner()))
		{
			Naia->GrantActiveSkillForDebug(SkillId);
		}
		else
		{
			TArray<int32> SkillIds = ASC->GetActiveSkillIds();
			SkillIds.AddUnique(SkillId);
			ASC->SetActiveSkillIds(SkillIds);
		}
		
		UE_LOG(LogNUDamageDebug, Display,
			TEXT("[nu.Damage.GrantSkill] %s 에 %d %s 를 붙였다 (SP %d, 위력 %.0f, %s)"),
			*Label, SkillId, *Definition->KoreanName.ToString(),
			Definition->SPCost,
			NUActiveSkill::GatherDamageParams(Data->FindSkillEffects(SkillId)).Coefficient * 100.f,
			*UEnum::GetDisplayValueAsText(Definition->TargetPolicy).ToString());
	}));

// 성장 노드를 찍지 않고 직업 스킬만 시험하려고 둔 것이다
// ★ 여기 붙여도 성장 상태는 안 바뀐다 — 다음 RefreshProgressionGrants 가 덮어쓴다
static FAutoConsoleCommandWithWorldAndArgs GGrantJobCmd(
	TEXT("nu.Damage.GrantJob"),
	TEXT("직업 스킬을 임시로 붙인다(테스트용): nu.Damage.GrantJob <SkillId> [naia|enemy]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogNUDamageDebug, Warning,
				TEXT("[nu.Damage.GrantJob] 사용법: nu.Damage.GrantJob <SkillId> [naia|enemy]"));
			return;
		}
		
		FString Label;
		UNUAbilitySystemComponent* ASC =
			PickCombatant(World, Args, TEXT("nu.Damage.GrantJob"), &Label);
		if (!IsValid(ASC))
		{
			return;
		}
		
		UGameInstance* GameInstance = IsValid(World) ? World->GetGameInstance() : nullptr;
		const UNUBattleDataSubsystem* Data =
			IsValid(GameInstance) ? GameInstance->GetSubsystem<UNUBattleDataSubsystem>() : nullptr;
		
		const int32 SkillId = FCString::Atoi(*Args[0]);
		
		// 표에 없는 번호를 넣으면 조용히 아무 일도 안 한다, 그걸 여기서 막는다
		if (!IsValid(Data) || Data->FindJobSkillEffects(SkillId) == nullptr)
		{
			UE_LOG(LogNUDamageDebug, Warning,
				TEXT("[nu.Damage.GrantJob] 직업 효과표에 없는 번호다 : %d"), SkillId);
			return;
		}
		
		const FNUJobSkillDefinition* Definition = Data->FindJobSkillDefinition(SkillId);
		const FString SkillName = Definition ? Definition->Name_KR.ToString() : TEXT("?");
		
		// ★ 보유 목록은 ASC 가 아니라 PlayerSkillComponent 가 들고 있다
		// ASC 에만 넣으면 상시(패시브) 층은 켜지는데 명령창 메뉴와 보유 스킬 창은 그대로 빈다
		// --- "붙였다" 로그는 뜨는데 스킬이 안 보이던 것이 이 어긋남이었다
		ANUPlayerState* PlayerState = Cast<ANUPlayerState>(ASC->GetOwnerActor());
		UNUPlayerSkillComponent* Skills = IsValid(PlayerState)
			? PlayerState->GetPlayerSkillComponent()
			: nullptr;
		
		if (IsValid(Skills))
		{
			static const FName ActiveType(TEXT("액티브"));
			static const FName UltimateType(TEXT("필살기"));
			const bool bActive = Definition
				&& (Definition->SkillType == ActiveType || Definition->SkillType == UltimateType);
			
			// RefreshProgressionGrants 는 ProgressionNode 로 붙은 것만 지우므로 Debug 는 살아남는다
			// GrantSourceId 가 0 이면 ValidateState 가 거부해서 스킬 번호를 그대로 넣는다
			FNUPlayerSkillState NewState = Skills->GetState();
			
			if (bActive)
			{
				FNUOwnedActiveSkill Entry;
				Entry.SkillId = SkillId;
				Entry.GrantSource = ENUPlayerSkillGrantSource::Debug;
				Entry.GrantSourceId = SkillId;
				NewState.OwnedActiveSkills.AddUnique(Entry);
			}
			else
			{
				FNUOwnedPassiveSkill Entry;
				Entry.SkillId = SkillId;
				Entry.GrantSource = ENUPlayerSkillGrantSource::Debug;
				Entry.GrantSourceId = SkillId;
				NewState.OwnedPassiveSkills.AddUnique(Entry);
			}
			
			// ApplyState 가 ASC 로도 밀어 주므로 상시 층은 그대로 켜진다
			if (!Skills->ApplyState(NewState))
			{
				UE_LOG(LogNUDamageDebug, Warning,
					TEXT("[nu.Damage.GrantJob] 보유 목록이 %d 를 거부했다 — LogNUPlayerSkill 을 볼 것"),
					SkillId);
				return;
			}
			
			UE_LOG(LogNUDamageDebug, Display,
				TEXT("[nu.Damage.GrantJob] %s 에 %d %s 를 %s 로 붙였다 (액티브 %d · 패시브 %d)"),
				*Label, SkillId, *SkillName,
				bActive ? TEXT("액티브") : TEXT("패시브"),
				Skills->GetOwnedActiveSkillIds().Num(),
				Skills->GetOwnedPassiveSkillIds().Num());
			
			// 메뉴에 오르는지는 따로 갈린다 --- 필살기와 자해 스킬은 아직 막혀 있다
			if (bActive)
			{
				FNUBattleSkillInfo Info;
				const bool bKnown = Data->TryGetBattleSkillInfo(SkillId, Info);
				UE_LOG(LogNUDamageDebug, Display,
					TEXT("    메뉴에 오르나 : %s"),
					bKnown && Info.bExecutable
						? TEXT("예")
						: TEXT("아니오 — 필살기(AG 없음)·자해(지불 자리 없음)·도는 줄 없음 중 하나"));

				if (bKnown && Info.MaxUsesPerBattle > 0)
				{
					UE_LOG(LogNUDamageDebug, Display,
						TEXT("    한도 : 전투당 %d회"), Info.MaxUsesPerBattle);
				}
			}
			
			return;
		}
		
		// 나이아·적은 보유 목록 자체가 없다, 상시 층만 시험할 수 있다
		TArray<int32> SkillIds = ASC->GetJobSkillIds();
		SkillIds.AddUnique(SkillId);
		ASC->SetJobSkillIds(SkillIds);
		
		UE_LOG(LogNUDamageDebug, Display,
			TEXT("[nu.Damage.GrantJob] %s 에 %d %s 를 상시 층으로만 붙였다 (보유 %d개) — nu.Damage.JobRows %d"),
			*Label, SkillId, *SkillName,
			SkillIds.Num(), SkillId);
	}));


// 디버그로 붙인 것만 뗀다, 성장 노드로 얻은 것은 그대로 둔다
static FAutoConsoleCommandWithWorld GClearJobCmd(
	TEXT("nu.Damage.ClearJob"),
	TEXT("디버그로 붙인 직업 스킬을 전부 뗀다"),
	FConsoleCommandWithWorldDelegate::CreateStatic([](UWorld* World)
	{
		FString Label;
		UNUAbilitySystemComponent* ASC =
			PickCombatant(World, TArray<FString>(), TEXT("nu.Damage.ClearJob"), &Label);
		ANUPlayerState* PlayerState = IsValid(ASC)
			? Cast<ANUPlayerState>(ASC->GetOwnerActor())
			: nullptr;
		UNUPlayerSkillComponent* Skills = IsValid(PlayerState)
			? PlayerState->GetPlayerSkillComponent()
			: nullptr;
		
		if (!IsValid(Skills))
		{
			UE_LOG(LogNUDamageDebug, Warning, TEXT("[nu.Damage.ClearJob] 보유 목록을 찾지 못했다"));
			return;
		}
		
		FNUPlayerSkillState NewState = Skills->GetState();
		const int32 Removed =
			NewState.OwnedActiveSkills.RemoveAll([](const FNUOwnedActiveSkill& Entry)
			{
				return Entry.GrantSource == ENUPlayerSkillGrantSource::Debug;
			})
			+ NewState.OwnedPassiveSkills.RemoveAll([](const FNUOwnedPassiveSkill& Entry)
			{
				return Entry.GrantSource == ENUPlayerSkillGrantSource::Debug;
			});
		
		Skills->ApplyState(NewState);
		UE_LOG(LogNUDamageDebug, Display,
			TEXT("[nu.Damage.ClearJob] %s 에서 디버그로 붙인 %d개를 뗐다"), *Label, Removed);
	}));


// 자원 축을 눈으로 보려고 둔 것이다 — 인자가 없으면 현재 칸을 찍고, 있으면 그만큼 더한다
// 자원은 스킬로만 쌓이므로(획득 트리거는 아직 없다) 조건·계수를 시험하려면 이 명령이 필요하다
static FAutoConsoleCommandWithWorldAndArgs GJobResourceCmd(
	TEXT("nu.Damage.Resource"),
	TEXT("직업 자원을 보거나 더한다. nu.Damage.Resource [자원이름] [양] [대상]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		// 이름·양을 뺀 나머지가 대상 인자다, 다른 명령과 같은 자리에서 고른다
		TArray<FString> PickArgs = Args;
		if (PickArgs.Num() > 0)
		{
			PickArgs.RemoveAt(0, FMath::Min(2, PickArgs.Num()));
		}
		
		FString Label;
		UNUAbilitySystemComponent* ASC =
			PickCombatant(World, PickArgs, TEXT("nu.Damage.Resource"), &Label);
		
		if (!IsValid(ASC))
		{
			return;
		}
		
		if (Args.Num() >= 2)
		{
			const FName Name(*Args[0]);
			const int32 Delta = FCString::Atoi(*Args[1]);
			const int32 Moved = ASC->AddJobResource(Name, Delta);
			
			int32 Amount = 0;
			ASC->TryGetJobResource(Name, Amount);
			
			UE_LOG(LogNUDamageDebug, Display,
				TEXT("[nu.Damage.Resource] %s 의 %s %+d -> %d / %d (실제 %+d)"),
				*Label, *Name.ToString(), Delta, Amount, ASC->GetJobResourceMax(Name), Moved);
			
			return;
		}
		
		const TArray<FNUJobResourceInstance>& Resources = ASC->GetJobResources();
		
		if (Resources.Num() == 0)
		{
			UE_LOG(LogNUDamageDebug, Display, TEXT("[nu.Damage.Resource] %s 에 쌓인 자원이 없다"), *Label);
			return;
		}
		
		UE_LOG(LogNUDamageDebug, Display, TEXT("[nu.Damage.Resource] %s 의 자원 %d종"),
			*Label, Resources.Num());
		
		for (const FNUJobResourceInstance& Instance : Resources)
		{
			const int32 Max = ASC->GetJobResourceMax(Instance.ResourceName);
			
			UE_LOG(LogNUDamageDebug, Display, TEXT("       %s %d / %d%s"),
				*Instance.ResourceName.ToString(), Instance.Amount, Max,
				ASC->IsJobResourceAtMax(Instance.ResourceName) ? TEXT("  (가득)") : TEXT(""));
		}
	}));


// 자원을 원하는 만큼 채우려고 둔 것이다 — 값을 안 주면 상한까지, 주면 그 값으로 맞춘다
// nu.Damage.Resource 는 더하기만 하고 획득 배수 · 턴당 상한(3230)을 거쳐 깎이므로 조건 · 계수 시험에 불편했다
static FAutoConsoleCommandWithWorldAndArgs GFillJobResourceCmd(
	TEXT("nu.Damage.FillResource"),
	TEXT("직업 자원을 채운다: nu.Damage.FillResource <자원이름> [값] [naia|enemy] — 값을 안 주면 상한까지, 대상에게 붙는 자원은 대상을 안 주면 적"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogNUDamageDebug, Warning,
				TEXT("[nu.Damage.FillResource] 사용법: nu.Damage.FillResource <자원이름> [값] [naia|enemy]"));
			return;
		}
		
		UGameInstance* GameInstance = IsValid(World) ? World->GetGameInstance() : nullptr;
		const UNUBattleDataSubsystem* Data =
			IsValid(GameInstance) ? GameInstance->GetSubsystem<UNUBattleDataSubsystem>() : nullptr;
		
		const FName Name(*Args[0]);
		const FNUJobResourceDefinition* Definition = IsValid(Data) ? Data->FindJobResource(Name) : nullptr;
		
		if (!Definition)
		{
			UE_LOG(LogNUDamageDebug, Warning, TEXT("[nu.Damage.FillResource] 자원표에 없는 이름이다 : %s"),
				*Name.ToString());
			return;
		}
		
		// 공용 `자원` · 속도차이는 쌓는 규칙이 코드에 없어 칸을 만들 수 없음
		if (Definition->IsDerived() || Definition->IsResolvedByJobNode())
		{
			UE_LOG(LogNUDamageDebug, Warning,
				TEXT("[nu.Damage.FillResource] %s 는 전직 노드나 파생값이 정하는 자원이라 못 채운다"), *Name.ToString());
			return;
		}
		
		// 대상에게 붙는 자원(마크 · 약점 · 문양)은 대상을 안 주면 적에게 채움, 자기에게 채우면 아무 데도 안 쓰임
		TArray<FString> PickArgs = Args;
		const bool bNamedTarget = Args.ContainsByPredicate([](const FString& Arg)
		{
			return Arg.Equals(TEXT("naia"), ESearchCase::IgnoreCase)
				|| Arg.Equals(TEXT("enemy"), ESearchCase::IgnoreCase);
		});
		
		if (!bNamedTarget && Definition->OwnerScope == ENUJobResourceOwner::Target)
		{
			PickArgs.Add(TEXT("enemy"));
		}
		
		FString Label;
		UNUAbilitySystemComponent* ASC =
			PickCombatant(World, PickArgs, TEXT("nu.Damage.FillResource"), &Label);
		
		if (!IsValid(ASC))
		{
			return;
		}
		
		const int32 Max = ASC->GetJobResourceMax(Name);
		const bool bHasValue = Args.Num() >= 2 && Args[1].IsNumeric();
		
		// 상한이 없는 자원(템포)은 어디까지 채울지 모름
		if (!bHasValue && Max <= 0)
		{
			UE_LOG(LogNUDamageDebug, Warning,
				TEXT("[nu.Damage.FillResource] %s 는 상한이 없는 자원이라 값을 같이 줘야 한다"), *Name.ToString());
			return;
		}
		
		const int32 Wanted = bHasValue ? FCString::Atoi(*Args[1]) : Max;
		
		int32 Before = 0;
		ASC->TryGetJobResource(Name, Before);
		
		// 획득 배수 · 턴당 상한을 안 거치고 모자란 만큼만 더함, 0 과 상한 사이로 자르는 것은 AddJobResource 가 함
		ASC->AddJobResource(Name, Wanted - Before, false);
		
		// 칸이 없던 자원은 시작값으로 칸을 만든 뒤 더해서 어긋날 수 있어 한 번 더 맞춤
		int32 After = 0;
		ASC->TryGetJobResource(Name, After);
		
		if (After != Wanted)
		{
			ASC->AddJobResource(Name, Wanted - After, false);
			ASC->TryGetJobResource(Name, After);
		}
		
		UE_LOG(LogNUDamageDebug, Display, TEXT("[nu.Damage.FillResource] %s 의 %s %d -> %d%s"),
			*Label, *Name.ToString(), Before, After,
			Max > 0 ? *FString::Printf(TEXT(" / %d"), Max) : TEXT(""));
	}));


// 이번 턴 공격 횟수 조건(3050 연쇄 — 6번 공격하면 7회차부터 x1.4)을 시험하려고 둔 것이다
// 기본 공격이 1타에 SP 10 이라 기본 SP(30)로는 한 턴 7타가 안 나온다
// 명령을 고르는 동안 맞춰 두면 그 턴의 행동이 이어서 센다, 턴이 닫히면 0 으로 돌아감
static FAutoConsoleCommandWithWorldAndArgs GHitsThisTurnCmd(
	TEXT("nu.Damage.Hits"),
	TEXT("이번 턴 공격 횟수를 보거나 맞춘다: nu.Damage.Hits [횟수] [naia|enemy] — 기준에 닿는 순간 도는 줄(3105)은 기준보다 하나 적게 맞추고 때릴 것"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		FString Label;
		UNUAbilitySystemComponent* ASC = PickCombatant(World, Args, TEXT("nu.Damage.Hits"), &Label);
		
		if (!IsValid(ASC))
		{
			return;
		}
		
		const int32 Before = ASC->GetHitsDealtThisTurn();
		const bool bHasValue = Args.Num() >= 1 && Args[0].IsNumeric();
		
		if (bHasValue)
		{
			ASC->SetHitsDealtThisTurn(FCString::Atoi(*Args[0]));
		}
		
		UE_LOG(LogNUDamageDebug, Display, TEXT("[nu.Damage.Hits] %s 의 이번 턴 공격 %d회%s"),
			*Label, ASC->GetHitsDealtThisTurn(),
			bHasValue ? *FString::Printf(TEXT(" (%d 에서 맞춤)"), Before) : TEXT(""));
	}));


// 체력 조건 줄(3001 구간 · 3027 전투당 1회 상태 · 처형기 대상 35%)을 맞아서 깎이기를 기다리지 않고 시험하려고 둔 것이다
// 피해가 아니라 값을 바로 넣으므로 사망 판정 · 피격 패시브 · 죽음 방지는 안 탄다, 그래서 1 밑으로는 안 내림
// 체력 조건 상태는 체력이 바뀔 때 다시 판정하므로(PostAttributeChange) 이 명령으로도 켜지고 끝난다
static FAutoConsoleCommandWithWorldAndArgs GSetHealthCmd(
	TEXT("nu.Damage.SetHP"),
	TEXT("체력을 최대 체력의 퍼센트로 맞춘다: nu.Damage.SetHP <퍼센트> [naia|enemy] — 피해가 아니라 사망 · 피격 판정은 안 탐"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 1 || !Args[0].IsNumeric())
		{
			UE_LOG(LogNUDamageDebug, Warning, TEXT("[nu.Damage.SetHP] 사용법: nu.Damage.SetHP <퍼센트> [naia|enemy]"));
			return;
		}
		
		FString Label;
		UNUAbilitySystemComponent* ASC = PickCombatant(World, Args, TEXT("nu.Damage.SetHP"), &Label);
		
		if (!IsValid(ASC))
		{
			return;
		}
		
		const float MaxHealth = ASC->GetNumericAttribute(UNUVitalAttributeSet::GetMaxHealthAttribute());
		const float Before = ASC->GetNumericAttribute(UNUVitalAttributeSet::GetHealthAttribute());
		const float Percent = FMath::Clamp(FCString::Atof(*Args[0]), 0.f, 100.f);
		
		ASC->SetNumericAttributeBase(UNUVitalAttributeSet::GetHealthAttribute(),
			FMath::Max(1.f, MaxHealth * Percent / 100.f));
		
		UE_LOG(LogNUDamageDebug, Display, TEXT("[nu.Damage.SetHP] %s 의 HP %.0f -> %.0f / %.0f"),
			*Label, Before, ASC->GetNumericAttribute(UNUVitalAttributeSet::GetHealthAttribute()), MaxHealth);
	}));


// 표에 아직 값이 없어 스킬로는 두 축을 켤 수 없다, 통로만 따로 시험하려고 둔 것이다
static FAutoConsoleCommandWithWorldAndArgs GGrantShieldCmd(
	TEXT("nu.Damage.Shield"),
	TEXT("보호막을 임시로 건다: nu.Damage.Shield <최대HP비율> [턴] [naia|enemy]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogNUDamageDebug, Warning,
				TEXT("[nu.Damage.Shield] 사용법: nu.Damage.Shield <최대HP비율> [턴] [naia]"));
			return;
		}

		UNUAbilitySystemComponent* ASC = PickCombatant(World, Args, TEXT("nu.Damage.Shield"));
		if (!IsValid(ASC))
		{
			return;
		}

		const float Ratio = FCString::Atof(*Args[0]);
		const int32 Turns = Args.Num() >= 2 && Args[1].IsNumeric() ? FCString::Atoi(*Args[1]) : 0;

		// 스킬 번호 0으로 걸면 다시 걸 때마다 같은 칸이 갱신된다
		ASC->ApplyShield(ASC, 0, Ratio, true, Turns);

		UE_LOG(LogNUDamageDebug, Display, TEXT("[nu.Damage.Shield] 보호막 %.0f 을 걸었다 (%d턴)"),
			ASC->GetShieldAmount(), Turns);
	}));

static FAutoConsoleCommandWithWorldAndArgs GGrantReflectCmd(
	TEXT("nu.Damage.Reflect"),
	TEXT("반사를 임시로 건다(걸린 쪽이 공격하면 되받음): nu.Damage.Reflect <비율> [턴] [naia|enemy]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		UNUAbilitySystemComponent* ASC = PickCombatant(World, Args, TEXT("nu.Damage.Reflect"));
		if (!IsValid(ASC))
		{
			return;
		}

		// 비율을 안 주면 100%, 표와 같은 백분율로 받는다 (50 = 50%)
		const float Ratio = Args.Num() >= 1 && Args[0].IsNumeric() ? FCString::Atof(*Args[0]) / 100.f : 1.f;
		const int32 Turns = Args.Num() >= 2 && Args[1].IsNumeric() ? FCString::Atoi(*Args[1]) : 0;

		ASC->ApplyReflect(ASC, 0, Ratio, Turns);

		// 걸린 전투원이 공격했을 때 되받으므로, 시험하려면 이 전투원으로 공격해야 한다
		UE_LOG(LogNUDamageDebug, Display,
			TEXT("[nu.Damage.Reflect] 가한 피해의 %.0f%% 를 되받게 걸었다 (%d턴)"),
			Ratio * 100.f, Turns);
	}));

// 새 패시브 효과표가 실제로 들어왔는지, 한 패시브의 줄을 빠짐없이 읽는지 보는 자리
// 2808 · 2809 · 2830 · 2831 이 두 줄짜리다 — 한 줄만 나오면 표나 임포트가 잘못된 것이다
static FAutoConsoleCommandWithWorldAndArgs GDumpPassiveRowsCmd(
	TEXT("nu.Damage.PassiveRows"),
	TEXT("패시브 효과 행을 찍는다: nu.Damage.PassiveRows [PassiveId]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		UGameInstance* GameInstance = IsValid(World) ? World->GetGameInstance() : nullptr;
		const UNUBattleDataSubsystem* Data =
			IsValid(GameInstance) ? GameInstance->GetSubsystem<UNUBattleDataSubsystem>() : nullptr;
		if (!IsValid(Data))
		{
			UE_LOG(LogNUDamageDebug, Warning, TEXT("[nu.Damage.PassiveRows] 전투 데이터 서브시스템이 없다"));
			return;
		}

		// 번호를 안 주면 대역 전체를 훑는다, 표가 통째로 안 들어왔을 때 바로 보인다
		const int32 First = Args.Num() >= 1 ? FCString::Atoi(*Args[0]) : 2801;
		const int32 Last = Args.Num() >= 1 ? First : 2840;

		int32 TotalRows = 0;

		for (int32 PassiveId = First; PassiveId <= Last; ++PassiveId)
		{
			const TArray<FNUPassiveEffectRow>* Effects = Data->FindPassiveEffects(PassiveId);
			if (!Effects || Effects->Num() == 0)
			{
				continue;
			}

			const FNUPassiveDefinition* Definition = Data->FindPassiveDefinition(PassiveId);
			const FString Label = Definition ? Definition->KoreanName.ToString() : TEXT("?");

			UE_LOG(LogNUDamageDebug, Display, TEXT("--- %d %s (%d줄) ---"),
				PassiveId, *Label, Effects->Num());

			for (const FNUPassiveEffectRow& Effect : *Effects)
			{
				TotalRows += 1;

				UE_LOG(LogNUDamageDebug, Display,
					TEXT("  #%d %s / %s / %s  값 %g%s(계산값 %g)  트리거 %s(%.0f%%) %s %.2f  조건 %s %s  전투당 %d  %s"),
					Effect.EffectIndex,
					*UEnum::GetDisplayValueAsText(Effect.EffectType).ToString(),
					*UEnum::GetDisplayValueAsText(Effect.TargetScope).ToString(),
					*UEnum::GetDisplayValueAsText(Effect.StackType).ToString(),
					Effect.Value, Effect.bIsPercent ? TEXT("%") : TEXT(""), Effect.GetScaledValue(),
					*UEnum::GetDisplayValueAsText(Effect.Trigger).ToString(),
					Effect.TriggerChance * 100.f,
					*UEnum::GetDisplayValueAsText(Effect.ThresholdCompare).ToString(),
					Effect.TriggerThreshold,
					*UEnum::GetDisplayValueAsText(Effect.ConditionType).ToString(),
					*Effect.ConditionValue,
					Effect.MaxPerBattle,
					Effect.ImplementBy == ENUEffectImplementBy::Code ? TEXT("[코드 필요]") : TEXT(""));
			}
		}

		if (TotalRows == 0)
		{
			UE_LOG(LogNUDamageDebug, Warning,
				TEXT("[nu.Damage.PassiveRows] 행이 하나도 없다 — DT_PassiveSkillEffect 를 만들고 "
					"프로젝트 설정의 PassiveEffectTable 에 지정했는지 확인할 것"));
			return;
		}

		UE_LOG(LogNUDamageDebug, Display, TEXT("[nu.Damage.PassiveRows] 모두 %d줄"), TotalRows);
	}));

// 액티브 효과 줄도 패시브와 같은 방식으로 확인한다
// 행 구조체가 바뀌면 DataTable 이 조용히 안 읽히는데, 그때 이 명령만 텅 빈다
static FAutoConsoleCommandWithWorldAndArgs GDumpSkillRowsCmd(
	TEXT("nu.Damage.SkillRows"),
	TEXT("액티브 효과 행을 찍는다: nu.Damage.SkillRows [SkillId]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		UGameInstance* GameInstance = IsValid(World) ? World->GetGameInstance() : nullptr;
		const UNUBattleDataSubsystem* Data =
			IsValid(GameInstance) ? GameInstance->GetSubsystem<UNUBattleDataSubsystem>() : nullptr;
		if (!IsValid(Data))
		{
			UE_LOG(LogNUDamageDebug, Warning, TEXT("[nu.Damage.SkillRows] 전투 데이터 서브시스템이 없다"));
			return;
		}
		
		// 번호를 안 주면 대역 전체를 훑는다, 표가 통째로 안 들어왔을 때 바로 보인다
		const int32 First = Args.Num() >= 1 ? FCString::Atoi(*Args[0]) : 2001;
		const int32 Last = Args.Num() >= 1 ? First : 2510;
		
		int32 TotalRows = 0;
		
		for (int32 SkillId = First; SkillId <= Last; ++SkillId)
		{
			const TArray<FNUActiveEffectRow>* Effects = Data->FindSkillEffects(SkillId);
			if (!Effects || Effects->Num() == 0)
			{
				continue;
			}
			
			const FNUActiveSkillDefinition* Definition = Data->FindActiveSkillDefinition(SkillId);
			const FString Label = Definition ? Definition->KoreanName.ToString() : TEXT("?");
			
			// 피해 줄이 들고 있는 계수와 속성을 같이 찍는다
			UE_LOG(LogNUDamageDebug, Display, TEXT("--- %d %s (%d줄)  위력 %.0f  %s ---"),
				SkillId, *Label, Effects->Num(),
				NUActiveSkill::GatherDamageParams(Effects).Coefficient * 100.f,
				Definition ? *Definition->AttackElement.ToString() : TEXT("?"));
			
			for (const FNUActiveEffectRow& Effect : *Effects)
			{
				TotalRows += 1;
				
				UE_LOG(LogNUDamageDebug, Display,
					TEXT("  #%d %s / %s / %s  값 %g%s(계산값 %g)  지속 %s %d턴  조건 %s %s  확률 %.0f%%  상태 %d  %s"),
					Effect.EffectIndex,
					*UEnum::GetDisplayValueAsText(Effect.EffectType).ToString(),
					*UEnum::GetDisplayValueAsText(Effect.TargetScope).ToString(),
					*UEnum::GetDisplayValueAsText(Effect.StackType).ToString(),
					Effect.Value, Effect.bIsPercent ? TEXT("%") : TEXT(""), Effect.GetScaledValue(),
					*UEnum::GetDisplayValueAsText(Effect.DurationScope).ToString(),
					Effect.GetDurationTurns(),
					*UEnum::GetDisplayValueAsText(Effect.ConditionType).ToString(),
					*Effect.ConditionValue,
					Effect.Chance * 100.f,
					Effect.StatusEffectId,
					Effect.ImplementBy == ENUEffectImplementBy::Code ? TEXT("[코드 필요]") : TEXT(""));
			}
		}
		
		if (TotalRows == 0)
		{
			UE_LOG(LogNUDamageDebug, Warning,
				TEXT("[nu.Damage.SkillRows] 행이 하나도 없다 — DT_ActiveSkillEffect 를 "
					"FNUActiveEffectRow 로 다시 임포트했는지 확인할 것"));
			return;
		}
		
		UE_LOG(LogNUDamageDebug, Display, TEXT("[nu.Damage.SkillRows] 모두 %d줄"), TotalRows);
	}));

// 직업 스킬은 한 행이 왜 안 도는지가 네 갈래다 — 표가 덜 왔거나(Pending), 전용 구현이
// 필요하거나(Code*), 상시가 아니거나, 비례 기준을 못 세거나.
// 그래서 "안 돈다" 를 줄마다 이유까지 찍는다. 이게 없으면 표에서 찾을 방법이 없다

// `무엇 1당` 의 기준을 런타임이 실제로 셀 수 있는지
// ★ 자원표는 이미 왔다 — 여기서 걸리는 것은 "표 대기" 가 아니라 못 세는 기준이다.
//   UNUAbilitySystemComponent 의 JobValueSteps 와 같은 규칙이어야 한다
static bool CanCountJobValuePer(const UNUBattleDataSubsystem& Data, FName ValuePer)
{
	if (ValuePer == TEXT("잃은HP") || ValuePer == TEXT("최대HP") || ValuePer == TEXT("속도초과분"))
	{
		return true;
	}
	
	// 파생값(속도차이)과 전직 갈래가 정하는 자리(공용 `자원`)는 칸이 없어 셀 수 없다
	const FNUJobResourceDefinition* Definition = Data.FindJobResource(ValuePer);
	
	return Definition && !Definition->IsDerived() && !Definition->IsResolvedByJobNode();
}
// 아군 범위 줄 중 받는 쪽이 피해 계산 때 모으는 종류인지, 스탯 GE 나 보호막 줄은 아직 전파 대기
// NUAbilitySystemComponent 의 IsPerAttackCriticalRow 와 같은 규칙이어야 한다
static bool IsPropagatedAtHit(const FNUJobSkillEffectRow& Row)
{
	if (Row.TargetScope != ENUJobTargetScope::Ally
		&& Row.TargetScope != ENUJobTargetScope::AllyAndNaia
		&& Row.TargetScope != ENUJobTargetScope::Party)
	{
		return false;
	}

	if (Row.EffectType == ENUJobEffectType::DamageDealtMod || Row.EffectType == ENUJobEffectType::DamageTakenMod)
	{
		return true;
	}

	static const FName MaxHealthName(TEXT("최대HP"));

	return Row.EffectType == ENUJobEffectType::StatModifier
		&& NUStatusEffect::IsRatioStat(Row.AffectedStat)
		&& (Row.ConditionType != ENUEffectCondition::None
			|| (!Row.ValuePer.IsNone() && Row.ValuePer != MaxHealthName));
}

static FAutoConsoleCommandWithWorldAndArgs GDumpJobRowsCmd(
	TEXT("nu.Damage.JobRows"),
	TEXT("직업 스킬 효과 행을 찍는다: nu.Damage.JobRows [SkillId]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		UGameInstance* GameInstance = IsValid(World) ? World->GetGameInstance() : nullptr;
		const UNUBattleDataSubsystem* Data =
			IsValid(GameInstance) ? GameInstance->GetSubsystem<UNUBattleDataSubsystem>() : nullptr;
		if (!IsValid(Data))
		{
			UE_LOG(LogNUDamageDebug, Warning, TEXT("[nu.Damage.JobRows] 전투 데이터 서브시스템이 없다"));
			return;
		}
		
		// 번호를 안 주면 대역 전체를 훑는다 (나이아나이트 3001 ~ 드루이드 3451)
		const int32 First = Args.Num() >= 1 ? FCString::Atoi(*Args[0]) : 3001;
		const int32 Last = Args.Num() >= 1 ? First : 3451;
		
		int32 TotalRows = 0;
		int32 AlwaysRows = 0;
		
		for (int32 SkillId = First; SkillId <= Last; ++SkillId)
		{
			const TArray<FNUJobSkillEffectRow>* Effects = Data->FindJobSkillEffects(SkillId);
			if (!Effects || Effects->Num() == 0)
			{
				continue;
			}
			
			const FNUJobSkillDefinition* Definition = Data->FindJobSkillDefinition(SkillId);
			const FString Label = Definition ? Definition->Name_KR.ToString() : TEXT("?");
			
			UE_LOG(LogNUDamageDebug, Display, TEXT("--- %d %s (%d줄)  계수 %.0f  %s ---"),
				SkillId, *Label, Effects->Num(),
				NUJobSkill::FindDamageCoefficient(Effects) * 100.f,
				Definition ? *Definition->SkillType.ToString() : TEXT("?"));
			
			for (const FNUJobSkillEffectRow& Effect : *Effects)
			{
				TotalRows += 1;
				
				// 왜 안 도는지를 한 마디로 적는다, 순서가 곧 판정 순서다
				FString Why;
				if (Effect.ImplementBy == ENUEffectImplementBy::Pending)
				{
					Why = TEXT("[표 대기]");
				}
				else if (Effect.ImplementBy != ENUEffectImplementBy::Data)
				{
					// 표는 Code_* 로 보내지만 전용 구현이 이미 붙은 갈래가 있다
					if (Effect.EffectType == ENUJobEffectType::CoefficientAdd)
					{
						Why = TEXT("[계수 가산 — 발동할 때 더함]");
					}
					else if (Effect.EffectType == ENUJobEffectType::DeathPrevent)
					{
						Why = TEXT("[죽을 피해를 맞을 때 판정]");
					}
					else if ((Effect.EffectType == ENUJobEffectType::ResourceGainCap
							|| (Effect.EffectType == ENUJobEffectType::ResourceGainMod
								&& Effect.DurationScope == ENUEffectDurationScope::Permanent))
						&& Effect.HasGatingCondition())
					{
						Why = TEXT("[획득 변조 — 판정 자리가 없는 조건이라 안 붙음]");
					}
					else if (Effect.EffectType == ENUJobEffectType::ResourceGainCap
						|| (Effect.EffectType == ENUJobEffectType::ResourceGainMod
							&& Effect.DurationScope == ENUEffectDurationScope::Permanent))
					{
						Why = TEXT("[획득 변조 — 가지고만 있어도 붙음]");
					}
					else if (Effect.ImplementBy == ENUEffectImplementBy::CodeResource
						&& (Effect.EffectType == ENUJobEffectType::ResourceGain
							|| Effect.EffectType == ENUJobEffectType::ResourceGainMod
							|| Effect.EffectType == ENUJobEffectType::ResourceDecayImmune))
					{
						Why = TEXT("[행동당 한 번 — 발동할 때 적용]");
					}
					else if (Effect.EffectType == ENUJobEffectType::ForceCritical
						&& !Effect.HasGatingCondition())
					{
						Why = TEXT("[반드시 치명타 — 피해 항목에 실음]");
					}
					else if (Effect.EffectType == ENUJobEffectType::DefenseIgnore
						&& !Effect.HasGatingCondition())
					{
						Why = TEXT("[방어력 무시 — 행동 전체에 실음]");
					}
					else if (Effect.EffectType == ENUJobEffectType::ResourceTimerReset
						&& !Effect.HasGatingCondition())
					{
						Why = TEXT("[소멸 대기 초기화 — 맞은 대상에게]");
					}
					else if (Effect.EffectType == ENUJobEffectType::Damage
						&& Effect.ConditionType == ENUEffectCondition::OnTurnEnd)
					{
						Why = TEXT("[턴 끝 지속 피해 — 감쇠 전에 상대 칸의 자원 수만큼, 방어력 무시]");
					}
					else if (Effect.EffectType == ENUJobEffectType::HealReceivedMod
						&& Effect.DurationScope == ENUEffectDurationScope::Permanent
						&& Effect.TargetScope == ENUJobTargetScope::Self)
					{
						Why = TEXT("[받는 회복 — 전투 중 가지고만 있어도 붙음]");
					}
					else if (Effect.EffectType == ENUJobEffectType::ShieldMod
						&& Effect.DurationScope == ENUEffectDurationScope::Permanent
						&& Effect.TargetScope == ENUJobTargetScope::Ally)
					{
						Why = TEXT("[아군 보호막 — 걸리는 순간 곱함]");
					}
					else
					{
						Why = TEXT("[코드 필요]");
					}
				}
				else if (Effect.ConditionType == ENUEffectCondition::HitCountAtLeast
					&& Effect.EffectType == ENUJobEffectType::ResourceGain)
				{
					Why = TEXT("[이번 턴 공격 횟수가 기준에 닿는 순간 한 번 — 패시브만]");
				}
				else if (Effect.EffectType == ENUJobEffectType::Heal
					&& !Effect.HasGatingCondition()
					&& !Effect.ValuePer.IsNone())
				{
					// 회복 갈래는 비례 기준을 읽지 않고 1회분만 돎 (3006 정령 해방 · 3308 성배 방출)
					Why = TEXT("[못 세는 기준 — 1회분으로 돎]");
				}
				else if (Effect.DurationScope != ENUEffectDurationScope::Permanent)
				{
					Why = TEXT("[상시 아님 — 액티브 경로 대기]");
				}
				else if (Effect.TargetScope == ENUJobTargetScope::Enemy
					&& Effect.EffectType == ENUJobEffectType::StatModifier
					&& Effect.ConditionType == ENUEffectCondition::None
					&& Data->FindJobResource(Effect.ValuePer)
					&& Data->FindJobResource(Effect.ValuePer)->OwnerScope == ENUJobResourceOwner::Target)
				{
					// 3107 마크 1중첩당 속도 -5 — 줄 주인이 아니라 자원을 든 적에게 걸림
					Why = TEXT("[대상에게 붙은 자원 1당 — 자원을 든 적에게 걸림]");
				}
				else if (Effect.TargetScope != ENUJobTargetScope::Self && IsPropagatedAtHit(Effect))
				{
					Why = TEXT("[같은 편에 전파 — 받는 쪽이 피해 계산 때 모음]");
				}
				else if (Effect.TargetScope != ENUJobTargetScope::Self)
				{
					Why = TEXT("[자기 대상 아님 — 전파 대기]");
				}
				else if (Effect.ConditionType == ENUEffectCondition::DuringState
					|| Effect.ConditionType == ENUEffectCondition::AfterState)
				{
					Why = FString::Printf(TEXT("[상태 중에만 — %s]"), *Effect.GetConditionName().ToString());
				}
				else if (!Effect.ValuePer.IsNone() && !CanCountJobValuePer(*Data, Effect.ValuePer))
				{
					Why = TEXT("[못 세는 기준 — 그 줄만 빠짐]");
				}
				else
				{
					AlwaysRows += 1;
				}
				
				UE_LOG(LogNUDamageDebug, Display,
					TEXT("  #%d %s / %s / %s  값 %g%s(계산값 %g)  %s%s  지속 %s %d턴  조건 %s %s  %s"),
					Effect.EffectIndex,
					*UEnum::GetDisplayValueAsText(Effect.EffectType).ToString(),
					*UEnum::GetDisplayValueAsText(Effect.TargetScope).ToString(),
					*UEnum::GetDisplayValueAsText(Effect.StackType).ToString(),
					Effect.Value, Effect.bIsPercent ? TEXT("%") : TEXT(""), Effect.GetScaledValue(),
					Effect.ValuePer.IsNone() ? TEXT("") : *FString::Printf(TEXT("%s %g당  "),
						*Effect.ValuePer.ToString(), Effect.GetPerUnit()),
					FMath::IsNearlyZero(Effect.ValueCap) ? TEXT("") : *FString::Printf(TEXT("상한 %g  "), Effect.ValueCap),
					*UEnum::GetDisplayValueAsText(Effect.DurationScope).ToString(),
					Effect.DurationTurns,
					*UEnum::GetDisplayValueAsText(Effect.ConditionType).ToString(),
					*Effect.ConditionValue,
					*Why);
			}
		}
		
		if (TotalRows == 0)
		{
			UE_LOG(LogNUDamageDebug, Warning,
				TEXT("[nu.Damage.JobRows] 행이 하나도 없다 — DT_JobSkillEffect 를 "
					"FNUJobSkillEffectRow 로 임포트하고 설정의 JobSkillEffectTable 에 걸었는지 확인할 것"));
			return;
		}
		
		UE_LOG(LogNUDamageDebug, Display,
			TEXT("[nu.Damage.JobRows] 모두 %d줄, 그중 상시로 도는 줄 %d"), TotalRows, AlwaysRows);
	}));

// 상태이상을 걸 방법이 계측에 없어서 해제(2104)도 확인할 수 없었다
// 확률·면역을 타지 않게 Chance 1로 고정해서 부른다, 걸렸는지 아닌지로 헷갈릴 일이 없어야 함
static FAutoConsoleCommandWithWorldAndArgs GApplyStatusCmd(
	TEXT("nu.Damage.Status"),
	TEXT("상태이상을 임시로 건다: nu.Damage.Status <상태ID 5001~5012> [턴] [naia|enemy]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogNUDamageDebug, Warning,
				TEXT("[nu.Damage.Status] 사용법: nu.Damage.Status <상태ID> [턴] [naia|enemy]"));
			return;
		}

		FString Label;
		UNUAbilitySystemComponent* ASC =
			PickCombatant(World, Args, TEXT("nu.Damage.Status"), &Label);

		if (!IsValid(ASC))
		{
			return;
		}

		const int32 StatusEffectId = FCString::Atoi(*Args[0]);
		const int32 Turns = Args.Num() >= 2 && Args[1].IsNumeric() ? FCString::Atoi(*Args[1]) : 0;

		// 부여자도 대상 자신으로 둔다, 2832 전이가 되튕기는 것을 확인하려면 적에게 걸고 때리면 됨
		const FActiveGameplayEffectHandle Handle =
			ASC->TryApplyStatusEffect(ASC, StatusEffectId, 1.f, Turns);

		if (!Handle.IsValid())
		{
			UE_LOG(LogNUDamageDebug, Warning,
				TEXT("[nu.Damage.Status] %s 에 상태이상 %d 를 걸지 못했다 (번호가 없거나 면역)"),
				*Label, StatusEffectId);
			return;
		}

		UE_LOG(LogNUDamageDebug, Display, TEXT("[nu.Damage.Status] %s 에 상태이상 %d 를 걸었다 (%s)"),
			*Label, StatusEffectId,
			Turns > 0 ? *FString::Printf(TEXT("%d턴"), Turns) : TEXT("표의 지속 턴"));
	}));

// 2833 동기화는 턴 1에만 발동한다, 그 시점엔 적에게 패시브를 붙여둘 틈이 없어 손으로 다시 굴린다
// 가져올 것이 없었으면 동기화 표시가 안 남으므로 이 명령은 몇 번이고 다시 부를 수 있다
static FAutoConsoleCommandWithWorldAndArgs GBattleStartCmd(
	TEXT("nu.Damage.BattleStart"),
	TEXT("전투 시작 패시브를 다시 굴린다(2833 동기화): nu.Damage.BattleStart [naia|enemy]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		FString Label;
		UNUAbilitySystemComponent* ASC =
			PickCombatant(World, Args, TEXT("nu.Damage.BattleStart"), &Label);

		if (!IsValid(ASC))
		{
			return;
		}

		ASC->ApplyBattleStartPassives();

		UE_LOG(LogNUDamageDebug, Display, TEXT("[nu.Damage.BattleStart] %s 의 전투 시작 패시브를 굴렸다"),
			*Label);
	}));

} // namespace NUDamageDebug

#endif // !UE_BUILD_SHIPPING
