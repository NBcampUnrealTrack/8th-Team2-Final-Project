// 전투 수동 확인용 콘솔 명령 (nu.Battle.*) — 개발 전용, Shipping 제외.
// 등록 방식은 self-registering static (FAutoConsoleCommandWithWorldAndArgs), nu.Inv.* 와 같음.
//
// 필드에 심볼이 아직 없는 적(보스 등)을 레벨 배치 없이 로컬 전투로 바로 불러 보려고 만든 것이다.
// 서버에 로그인돼 있어도 서버 전투로 보내지 않는다 — 서버가 아직 모르는 적 패턴 · 보스 스킬을 보는 용도라서.

#if !UE_BUILD_SHIPPING

#include "Battle/Encounter/NUBattleEncounter.h"
#include "Battle/Encounter/NUBattleEncounterSubsystem.h"
#include "Battle/Encounter/NUBattleEncounterTypes.h"
#include "Battle/Net/NUBattleNetStageSubsystem.h"
#include "Battle/Net/NUBattleNetSubsystem.h"
#include "CharacterState/NUPlayerState.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Field/Encounter/NURandomEncounterComponent.h"
#include "Field/FieldEnemy.h"
#include "Field/FieldPlayerCharacter.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Naia/Data/NUNaiaDataSubsystem.h"
#include "Net/EmberServerManager.h"
#include "NUGAS/NUAbilitySystemComponent.h"
#include "WorldLocation/World/NUEncounterZone.h"

DEFINE_LOG_CATEGORY_STATIC(LogNUBattleDebug, Log, All);

namespace NUBattleEncounterDebug
{

// 인카운터 클래스는 필드 캐릭터의 랜덤 인카운터 설정을 그대로 씀, 비어 있으면 레벨의 필드 심볼 것
TSoftClassPtr<ANUBattleEncounter> FindEncounterClass(UWorld* World, const AFieldPlayerCharacter* FieldPlayer)
{
	if (IsValid(FieldPlayer->RandomEncounterComponent) && !FieldPlayer->RandomEncounterComponent->EncounterClass.IsNull())
	{
		return FieldPlayer->RandomEncounterComponent->EncounterClass;
	}

	for (TActorIterator<AFieldEnemy> It(World); It; ++It)
	{
		if (!It->EncounterClass.IsNull())
		{
			return It->EncounterClass;
		}
	}

	return nullptr;
}

// 배경은 서 있는 인카운터 존 > 레벨의 아무 존 > 필드 심볼 순, 다 비어 있으면 배경을 바꾸지 않음
TSoftObjectPtr<UWorld> FindStageLevel(UWorld* World, const AFieldPlayerCharacter* FieldPlayer)
{
	TArray<AActor*> OverlappingZones;
	FieldPlayer->GetOverlappingActors(OverlappingZones, ANUEncounterZone::StaticClass());
	for (const AActor* Actor : OverlappingZones)
	{
		const ANUEncounterZone* Zone = Cast<ANUEncounterZone>(Actor);
		if (IsValid(Zone) && !Zone->GetBattleStageLevel().IsNull())
		{
			return Zone->GetBattleStageLevel();
		}
	}

	for (TActorIterator<ANUEncounterZone> It(World); It; ++It)
	{
		if (!It->GetBattleStageLevel().IsNull())
		{
			return It->GetBattleStageLevel();
		}
	}

	for (TActorIterator<AFieldEnemy> It(World); It; ++It)
	{
		if (!It->BattleStageLevel.IsNull())
		{
			return It->BattleStageLevel;
		}
	}

	return nullptr;
}

} // namespace NUBattleEncounterDebug

static FAutoConsoleCommandWithWorldAndArgs GFightCmd(
	TEXT("nu.Battle.Fight"),
	TEXT("배치 없이 로컬 전투 시작 (서버 전투로 보내지 않음): nu.Battle.Fight <NaiaId> [NaiaId ...]  예) nu.Battle.Fight 1701"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		using namespace NUBattleEncounterDebug;

		if (Args.IsEmpty() || World == nullptr)
		{
			UE_LOG(LogNUBattleDebug, Warning, TEXT("[nu.Battle.Fight] 사용법: nu.Battle.Fight <NaiaId> [NaiaId ...]"));
			return;
		}

		APlayerController* Controller = World->GetFirstPlayerController();
		const AFieldPlayerCharacter* FieldPlayer = Controller ? Cast<AFieldPlayerCharacter>(Controller->GetPawn()) : nullptr;
		ANUPlayerState* PlayerState = Controller ? Controller->GetPlayerState<ANUPlayerState>() : nullptr;
		if (!IsValid(FieldPlayer) || !IsValid(PlayerState))
		{
			UE_LOG(LogNUBattleDebug, Warning, TEXT("[nu.Battle.Fight] 필드 캐릭터가 없습니다. 필드에서 불러 주세요"));
			return;
		}

		// 넷 전투 중이거나 넷 전투 화면 전환 중이면 겹쳐 띄우지 않음
		UGameInstance* GameInstance = World->GetGameInstance();
		const UNUBattleNetSubsystem* Net = GameInstance ? GameInstance->GetSubsystem<UNUBattleNetSubsystem>() : nullptr;
		const UNUBattleNetStageSubsystem* NetStage = World->GetSubsystem<UNUBattleNetStageSubsystem>();
		if (IsValid(PlayerState->GetBattleEncounter())
			|| (Net && (Net->IsInBattle() || Net->IsBattleStartPending()))
			|| (NetStage && NetStage->IsTransitionActive()))
		{
			UE_LOG(LogNUBattleDebug, Warning, TEXT("[nu.Battle.Fight] 이미 전투 중입니다"));
			return;
		}

		const UNUAbilitySystemComponent* ASC = PlayerState->GetNUASC();
		if (!IsValid(ASC) || !ASC->IsAlive())
		{
			UE_LOG(LogNUBattleDebug, Warning, TEXT("[nu.Battle.Fight] 플레이어가 쓰러진 상태입니다"));
			return;
		}

		// 적 번호는 나이아 표의 RowName, 표에 없으면 인카운터가 적을 못 만들어 중간에 멈추므로 먼저 거름
		const UNUNaiaDataSubsystem* NaiaData = GameInstance ? GameInstance->GetSubsystem<UNUNaiaDataSubsystem>() : nullptr;
		FNUBattleEncounterRequest Request;
		for (const FString& Arg : Args)
		{
			const FName EnemyId(*Arg);
			if (!Arg.IsNumeric() || NaiaData == nullptr || !NaiaData->HasDefinition(EnemyId))
			{
				UE_LOG(LogNUBattleDebug, Warning, TEXT("[nu.Battle.Fight] 나이아 표에 없는 번호입니다: %s"), *Arg);
				return;
			}

			FNUEnemySpawnSpec& Enemy = Request.Enemies.AddDefaulted_GetRef();
			Enemy.EnemyId = EnemyId;
		}

		Request.Initiator = PlayerState;
		Request.Source = ENUBattleEncounterSource::Random;
		Request.EncounterClass = FindEncounterClass(World, FieldPlayer);
		Request.BattleStageLevel = FindStageLevel(World, FieldPlayer);

		if (Request.EncounterClass.IsNull())
		{
			UE_LOG(LogNUBattleDebug, Warning, TEXT("[nu.Battle.Fight] 인카운터 클래스를 찾지 못했습니다 (필드 캐릭터의 랜덤 인카운터 · 필드 심볼 모두 비어 있음)"));
			return;
		}

		if (Request.BattleStageLevel.IsNull())
		{
			UE_LOG(LogNUBattleDebug, Warning, TEXT("[nu.Battle.Fight] 이 레벨에 전투 배경이 지정된 존 · 심볼이 없어 배경을 바꾸지 않습니다"));
		}

		const UEmberServerManager* Server = GameInstance ? GameInstance->GetSubsystem<UEmberServerManager>() : nullptr;
		if (Server && Server->IsLoggedIn())
		{
			UE_LOG(LogNUBattleDebug, Warning, TEXT("[nu.Battle.Fight] 서버에 로그인돼 있지만 로컬 전투로 띄웁니다. 결과 · 보상 · 퀘스트 진행은 서버에 가지 않습니다"));
		}

		UNUBattleEncounterSubsystem* Encounters = World->GetSubsystem<UNUBattleEncounterSubsystem>();
		ANUBattleEncounter* Encounter = nullptr;
		const ENUBattleEncounterStartResult Result = Encounters
			? Encounters->TryStartEncounter(Request, Encounter)
			: ENUBattleEncounterStartResult::SpawnFailed;

		UE_LOG(LogNUBattleDebug, Log, TEXT("[nu.Battle.Fight] %s 결과=%s 배경=%s 인카운터=%s"),
			*FString::Join(Args, TEXT(" ")),
			*UEnum::GetValueAsString(Result),
			Request.BattleStageLevel.IsNull() ? TEXT("없음") : *Request.BattleStageLevel.GetAssetName(),
			*Request.EncounterClass.GetAssetName());
	}));

#endif // !UE_BUILD_SHIPPING
