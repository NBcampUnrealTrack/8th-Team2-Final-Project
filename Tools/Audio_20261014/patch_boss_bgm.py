# 일회성 패치: 전투 BGM 이 보스전(적에 Category=Boss 나이아)이면 BossBattleBGMId 를 틀게 한다.
import sys

ROOT = sys.argv[1] + '/Source/ProjectM/'


def patch(name, pairs):
    p = ROOT + name
    s = open(p, encoding='utf-8').read()
    for a, b in pairs:
        assert s.count(a) == 1, (name, a[:70])
        s = s.replace(a, b)
    open(p, 'w', encoding='utf-8').write(s)


patch('Audio/NUBGMSubsystem.h', [
('''#include "Subsystems/GameInstanceSubsystem.h"
''', '''#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
'''),
('''	void RecomputeBattleLayer();''', '''	void RecomputeBattleLayer();

	// 보스전이면 BossBattleBGMId, 아니면 전투 세트 BGM, 그것도 없으면 BattleBGMId
	FName ResolveBattleBGMId(bool bBoss) const;

	// 지금 전투의 적 아바타 중 보스 나이아(Category=Boss)가 있는지
	bool IsBossBattleNow() const;

	// 적 아바타는 전투가 켜진 뒤 조금 늦게 생기므로 잠시 다시 확인한다
	bool PollBossBattle(float DeltaSeconds);
	void StopBossPolling();'''),
('''	bool bBattleActive = false;
};''', '''	bool bBattleActive = false;

	bool bBossBattle = false;
	float BossPollElapsed = 0.f;
	FTSTicker::FDelegateHandle BossPollHandle;
};'''),
])

patch('Audio/NUBGMSubsystem.cpp', [
('''#include "Battle/Net/NUBattleNetStageSubsystem.h"
''', '''#include "Battle/Avatar/NUBattleAvatar.h"
#include "Battle/Net/NUBattleNetStageSubsystem.h"
'''),
('''#include "Components/AudioComponent.h"
''', '''#include "Components/AudioComponent.h"
#include "EngineUtils.h"
#include "Naia/Data/NUNaiaDataSubsystem.h"
#include "Naia/Data/NUNaiaDefinition.h"
'''),
('''void UNUBGMSubsystem::Deinitialize()
{
''', '''void UNUBGMSubsystem::Deinitialize()
{
	StopBossPolling();
'''),
('''	if (bBattleActive)
	{
		// 지금 로드된 전투 세트가 자기 BGM을 지정했으면 그걸 우선시 함 -
		// 지역/보스마다 전투 세트가 다르면 이걸로 지역별·보스별 전투곡이 코드 변경 없이 달라짐
		if (!CurrentBattleStageBGMId.IsNone())
		{
			SetLayer(ENUBGMLayer::Battle, CurrentBattleStageBGMId);
			return;
		}

		const UNUAudioSettings* Settings = GetDefault<UNUAudioSettings>();
		SetLayer(ENUBGMLayer::Battle, Settings ? Settings->BattleBGMId : NAME_None);
		return;
	}

	SetLayer(ENUBGMLayer::Battle, NAME_None);''', '''	if (bBattleActive)
	{
		bBossBattle = IsBossBattleNow();
		SetLayer(ENUBGMLayer::Battle, ResolveBattleBGMId(bBossBattle));

		// 적 아바타가 아직 안 생겼으면 잠시 다시 확인해 보스전이면 보스곡으로 바꾼다
		const UNUAudioSettings* Settings = GetDefault<UNUAudioSettings>();
		if (!bBossBattle && Settings != nullptr && !Settings->BossBattleBGMId.IsNone())
		{
			StopBossPolling();
			BossPollElapsed = 0.f;
			BossPollHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::PollBossBattle), 0.2f);
		}
		return;
	}

	StopBossPolling();
	bBossBattle = false;
	SetLayer(ENUBGMLayer::Battle, NAME_None);'''),
('''void UNUBGMSubsystem::HandleNetBattleStateChanged()''', '''FName UNUBGMSubsystem::ResolveBattleBGMId(bool bBoss) const
{
	const UNUAudioSettings* Settings = GetDefault<UNUAudioSettings>();

	// 보스전은 어느 전투 세트에서 싸우든 보스곡이 우선
	if (bBoss && Settings != nullptr && !Settings->BossBattleBGMId.IsNone() && FindEntry(Settings->BossBattleBGMId) != nullptr)
	{
		return Settings->BossBattleBGMId;
	}

	// 지금 로드된 전투 세트가 자기 BGM을 지정했으면 그걸 우선시 함 -
	// 지역마다 전투 세트가 다르면 이걸로 지역별 전투곡이 코드 변경 없이 달라짐
	if (!CurrentBattleStageBGMId.IsNone())
	{
		return CurrentBattleStageBGMId;
	}

	return Settings != nullptr ? Settings->BattleBGMId : NAME_None;
}

bool UNUBGMSubsystem::IsBossBattleNow() const
{
	UGameInstance* GI = GetGameInstance();
	UWorld* World = IsValid(GI) ? GI->GetWorld() : nullptr;
	const UNUNaiaDataSubsystem* NaiaData = IsValid(GI) ? GI->GetSubsystem<UNUNaiaDataSubsystem>() : nullptr;
	if (World == nullptr || NaiaData == nullptr)
	{
		return false;
	}

	for (TActorIterator<ANUBattleAvatar> It(World); It; ++It)
	{
		if (It->IsAllyCombatant() || It->GetNaiaId() <= 0)
		{
			continue;
		}
		const FNUNaiaDefinition* Definition = NaiaData->FindDefinitionById(It->GetNaiaId());
		if (Definition != nullptr && Definition->Category == ENUNaiaCategory::Boss)
		{
			return true;
		}
	}
	return false;
}

bool UNUBGMSubsystem::PollBossBattle(float DeltaSeconds)
{
	BossPollElapsed += DeltaSeconds;
	if (!bBattleActive || bBossBattle)
	{
		BossPollHandle.Reset();
		return false;
	}

	if (IsBossBattleNow())
	{
		bBossBattle = true;
		UE_LOG(LogTemp, Log, TEXT("BGMSubsystem: 보스전 감지 -> %s"), *ResolveBattleBGMId(true).ToString());
		SetLayer(ENUBGMLayer::Battle, ResolveBattleBGMId(true));
		BossPollHandle.Reset();
		return false;
	}

	// 전투 시작 연출(암전·적 등장)이 끝날 때까지만 본다
	if (BossPollElapsed > 8.f)
	{
		BossPollHandle.Reset();
		return false;
	}
	return true;
}

void UNUBGMSubsystem::StopBossPolling()
{
	if (BossPollHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(BossPollHandle);
		BossPollHandle.Reset();
	}
}

void UNUBGMSubsystem::HandleNetBattleStateChanged()'''),
])

patch('Audio/NUAudioSettings.h', [(
'''	// 보스 전용 BGM 자리, 아직 연결은 X''',
'''	// 보스전 BGM - 적 중에 보스 나이아(Category=Boss)가 있으면 전투 세트 BGM 보다 우선해서 튼다''')])
print('patched')
