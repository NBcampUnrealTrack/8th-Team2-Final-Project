// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "NUBGMTypes.h"
#include "NUBGMSubsystem.generated.h"

class UAudioComponent;
class UNUBGMConfig;
class UNUBattleNetSubsystem;
class UNUBattleNetStageSubsystem;
class ANUBGMZone;

/*
레벨 / 구역 / 전투 레이어 BGM 재생

지금 나올 곡 = 요청이 들어 있는 가장 높은 레이어
각 레이어가 자기 슬롯에 BGMId를 넣거나 비우면 Resolve()가 위에서부터 첫 유효 값을 골라서 재생하는 방식(스택과 유사)
*/
UCLASS()
class PROJECTM_API UNUBGMSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ANUBGMZone의 BeginPlay/EndPlay 전용
	void RegisterZone(ANUBGMZone* Zone);
	void UnregisterZone(ANUBGMZone* Zone);

	// ANUBGMZone의 오버랩 이벤트, 그리고 전투 종료(필드 폰 콜리전 재활성화) 직후 전용.
	// 등록된 박스 존 전원에게 IsLocalPawnInside()를 다시 물어 Area 레이어를 재구성함 -
	// 오버랩 이벤트 누적을 캐시하지 않으므로 콜리전 토글로 이벤트가 흐트러져도 안전함
	void RefreshAreaLayer();

	// 로컬 전투 진입/종료 - ANUPlayerController가 HandleBattleEncounterChanged에서 호출
	void SetLocalBattleActive(bool bActive);

private:
	void SetLayer(ENUBGMLayer Layer, FName BGMId);
	void Resolve();
	void ApplyResolved(FName NewBGMId);
	void StopBGM(float FadeOutSeconds);
	const FNUBGMEntry* FindEntry(FName BGMId) const;

	void RecomputeBattleLayer();

	// 보스전이면 BossBattleBGMId, 아니면 전투 세트 BGM, 그것도 없으면 BattleBGMId
	FName ResolveBattleBGMId(bool bBoss) const;

	// 적 나이아 아바타가 하나라도 있으면 true, 그중 보스 나이아(Category=Boss)가 있으면 bOutBoss
	bool FindEnemyAvatars(bool& bOutBoss) const;

	// 전투 레이어에 곡을 넣는다(보스 여부는 한 번 정하면 전투 끝까지 유지)
	void ChooseBattleLayer(bool bBoss);

	// 적 아바타는 전투가 켜진 뒤 몇 프레임 늦게 생기므로 생길 때까지 기다렸다 곡을 고른다
	bool PollBossBattle(float DeltaSeconds);
	void StopBossPolling();
	void HandleNetBattleStateChanged();
	void HandleNetStageReadyChanged(bool bReady);
	void BindNetStageSubsystem(UWorld* World);

	// 레벨 전환 동안 존이 잠깐 사라져도 Resolve()가 폴백으로 잘못 크로스페이드하지 않도록 막음
	void HandlePreLoadMap(const FString& MapName);
	void HandlePostLoadMap(UWorld* LoadedWorld);

private:
	UPROPERTY(Transient)
	TObjectPtr<UNUBGMConfig> BGMConfig = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> AudioComponent = nullptr;

	FName CurrentBGMId = NAME_None;
	FName LayerRequests[static_cast<uint8>(ENUBGMLayer::Count)] = {};

	// 지금 로드되어 있는 전투 세트가 Scope=BattleStage 존으로 지정한 BGM, 없으면 NAME_None
	FName CurrentBattleStageBGMId = NAME_None;

	TArray<TWeakObjectPtr<ANUBGMZone>> RegisteredZones;

	bool bSuspendResolve = false;
	bool bLocalBattleActive = false;
	bool bBattleActive = false;

	bool bBossBattle = false;
	bool bBattleLayerChosen = false;
	float BossPollElapsed = 0.f;
	uint64 BossPollStartFrame = 0;
	FTSTicker::FDelegateHandle BossPollHandle;
};
