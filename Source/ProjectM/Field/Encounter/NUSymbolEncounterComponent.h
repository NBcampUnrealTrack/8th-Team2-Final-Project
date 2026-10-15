#pragma once

#include "CoreMinimal.h"
#include "Battle/Encounter/NUBattleEncounterTypes.h"
#include "Battle/NUBattleTypes.h"
#include "Components/ActorComponent.h"
#include "NUSymbolEncounterComponent.generated.h"

class ANUBattleEncounter;
class UNUBattleNetSubsystem;

// 필드에서 인카운트 시작과 종료 연결
// 배틀이 종료되면 SourceAcotr에 결과를 전달

UCLASS(ClassGroup = (NU), meta = (BlueprintSpawnableComponent))
class PROJECTM_API UNUSymbolEncounterComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNUSymbolEncounterComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// 필드 인카운터 시작을 요청
	// BattleEncounterSubsystem을 통해 Encounter를 생성
	UFUNCTION(BlueprintCallable, Category = "NU|Field|Encounter")
	ENUBattleEncounterStartResult TryStartFieldEvent(
		const TArray<FNUEnemySpawnSpec>& Enemies,
		TSoftClassPtr<ANUBattleEncounter> EncounterClass,
		ENUBattleEncounterSource Source,
		AActor* SourceActor,
		TSoftObjectPtr<UWorld> BattleStageLevel);

	// BattleEncounter 반환
	UFUNCTION(BlueprintPure, Category = "NU|Field|Encounter")
	ANUBattleEncounter* GetActiveEncounter() const;

	// 전투 발생한 필드 몬스터 반환
	UFUNCTION(BlueprintPure, Category = "NU|Field|Encounter")
	AActor* GetActiveSourceActor() const;

private:
	// 인카운터 상태 검증
	ENUBattleEncounterStartResult ValidateFieldEvent(
		const TArray<FNUEnemySpawnSpec>& Enemies,
		TSoftClassPtr<ANUBattleEncounter> EncounterClass,
		ENUBattleEncounterSource Source,
		AActor* SourceActor) const;

	// BattleEnconter가 종료될 경우 호출
	// 승리, 패배, 도주 등
	void HandleEncounterCompleted(ANUBattleEncounter* Encounter, ENUBattleResult Result);
	// encounter 완료 델리게이트 해제
	void ClearActiveEncounter();

	// 서버에 로그인돼 있으면 서버 전투로 보냄, 로컬 전투는 검증·디버깅용으로 남겨둠
	// 현재 넷 전투는 야생 나이아 한 마리만 받는 프로토콜이라 Enemies가 1개가 아니면 로컬로 떨어짐
	// 성공하면 true, 실패(서버 미접속·나이아 다수·정의 없음 등)하면 false를 돌려줘서 호출부가 로컬로 이어가게 함
	bool TryStartServerBattle(
		const TArray<FNUEnemySpawnSpec>& Enemies,
		AActor* SourceActor,
		const TSoftObjectPtr<UWorld>& BattleStageLevel);

	// 서버 전투가 끝날 때 결과를 돌려줄 심볼로 잡고 심볼을 전투 중으로 바꿈, 시작한 쪽 · 끌려 들어간 파티원 공용
	void TrackServerBattle(UNUBattleNetSubsystem* BattleNet, AActor* SourceActor);

	// 파티원이 시작한 심볼 전투에 서버가 나를 넣었을 때, 이 월드의 같은 심볼로 배경과 결과를 맞춤
	// 심볼이 없으면(이미 처치했거나 다른 레벨) 서 있는 존의 배경으로 들어감
	void HandlePartyBattleJoined(int64 SymbolId);

	// 서버 전투 종료 푸시, SVR_Battle_End.reason(1 승리·2 패배·3 도주)이 ENUBattleResult 값과 그대로 대응함
	void HandleServerBattleEnded(int32 Reason);
	void HandleServerBattleStateChanged();

	UPROPERTY(Transient)
	TWeakObjectPtr<ANUBattleEncounter> ActiveEncounter;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> ActiveSourceActor;

	// 로컬 ActiveEncounter에 대응하는 서버 전투 진행 플래그, ANUBattleEncounter가 없어 별도로 둠
	bool bServerBattleActive = false;
};
