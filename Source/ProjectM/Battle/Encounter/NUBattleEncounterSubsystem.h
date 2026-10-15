#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Battle/Encounter/NUBattleEncounterTypes.h"
#include "Battle/Formation/NUBattleFormationTypes.h"
#include "NUBattleEncounterSubsystem.generated.h"

struct FNUBattleParticipant;
class ADirectionalLight;
class AExponentialHeightFog;
class ANUBattleFormationAxis;
class ANUBattleFormationSlot;
class ASkyLight;
class ULevel;
class ULevelStreamingDynamic;
class UWorld;

// 배경 세트가 화면에 완전히 보이게 된 시점, EnsureStageLoaded로 새로 비동기 스트리밍을 요청했을 때만 옴
// 이미 그 세트가 떠 있어서 EnsureStageLoaded가 조기 반환되면 오지 않으므로 IsStageReady()로 먼저 확인해야 함
DECLARE_MULTICAST_DELEGATE(FNUOnStageShown);

//필드에서 전투 요청을 받아 ANUBattleEncounter 생성
UCLASS()
class PROJECTM_API UNUBattleEncounterSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:
	// 필드에서 호출하는 전투 시작 함수
	UFUNCTION(BlueprintCallable, Category = "NU|Battle|Encounter")
	ENUBattleEncounterStartResult TryStartEncounter(const FNUBattleEncounterRequest& Request, ANUBattleEncounter*& OutEncounter);
	
	// 전투가 벌어지는 아레나 좌표 (ANUGameMode::ArenaCenter)
	// 3D 배경을 세트 하나로 쓰기 위해 모든 전투가 같은 자리를 공유함
	FVector GetArenaLocation() const;

	/* 전투 배경 서브레벨 */

	// 배경 서브레벨을 아레나 위에 올림, 같은 세트를 다시 요청하면 아무것도 하지 않음
	// 다른 세트가 오면 이전 것을 내리고 새로 올림
	void EnsureStageLoaded(const TSoftObjectPtr<UWorld>& StageLevel);

	// 올린 세트가 보이는 상태인지, 올린 세트가 없으면 true
	// 인카운터가 암전을 유지할지 판단하는 데 사용함
	bool IsStageReady() const;

	
	/* 배경이 정한 진형과 카메라 */

	// 세트가 이 열의 자리를 정해두었으면 true, Out 에 월드 트랜스폼을 채움
	// SlotCount 는 그 열의 총 인원, 축 기준 정렬에서 가운데 정렬에 필요함
	// false 면 호출부가 자기 기본 계산으로 위치를 잡아야 함
	bool ResolveFormationTransform(ENUBattleFormationRow Row, int32 SlotIndex, int32 SlotCount,
		FTransform& OutTransform) const;

	// 세트가 들고 있는 전투 카메라, 없으면 nullptr
	AActor* GetStageViewTarget() const;


	/* 배경 세트 전용 조명 */

	// 세트가 자기 전역 조명을 들고 있으면 필드 조명과 맞바꿈
	// 세트가 아무것도 안 들고 있으면 조명은 필드 것을 그대로 사용함
	// 안개는 조명과 달리 세트가 들고 있든 아니든 항상 필드 것을 끔
	void ApplyStageLighting();

	// 필드 조명으로 되돌림, 전투가 어떤 경로로 끝나도 반드시 호출해야 함
	// 놓치면 다음 전투까지 필드가 어두운 채로 남음
	void RestoreFieldLighting();

public:
	FNUOnStageShown OnStageShown;

private:
	UFUNCTION()
	void HandleStageLevelShown();

private:
	// request 검증
	bool ValidateEncounterRequest(const FNUBattleEncounterRequest& Request, ENUBattleEncounterStartResult& OutResult) const;

	// Request를 전투 시스템에서 사용하는 Participant로 변환
	bool BuildParticipantsFromRequest(const FNUBattleEncounterRequest& Request, TArray<FNUBattleParticipant>& OutParticipants) const;

	TSubclassOf<ANUBattleEncounter> ResolveEncounterClass(const FNUBattleEncounterRequest& Request) const;

	// 세트 안의 진형, 카메라, 조명 액터를 캐싱해둠
	// 세트가 바뀌면 다시 만들어야 하므로 ULevel 포인터로 키를 잡음
	// 최초 사용과 교체 감지가 한 조건으로 처리되고 별도 신호가 필요 없음
	void RebuildStageCacheIfNeeded() const;

	// 세트 레벨 밖의 DirectionalLight를 끄고, 끈 것만 명단에 담아둠
	// 세트가 DirectionalLight를 들고 있을 때만 호출함, DirectionalLight는 스택이 아니라 가산이라 두 개면 중첩됨
	void TurnoffFieldDirectionalLights();

	// 세트 레벨 밖의 ExponentialHeightFog를 끄고, 끈 것만 명단에 담아둠
	// 조명과 달리 세트가 안개를 들고 있는지와 무관하게 항상 호출함
	// 아레나가 지면보다 10000 아래라 필드 안개의 높이 밀도가 포화돼 세트가 통째로 뿌옇게 덮이기 때문
	void TurnOffFieldHeightFogs();

	// 세트가 들고 있는 전역 조명과 안개를 한꺼번에 켜고 끔
	void SetStageGlobalVisualsVisible(bool bVisible);

private:
	// 지금 올라와 있는 배경 서브레벨의 스트리밍 핸들
	UPROPERTY()
	TObjectPtr<ULevelStreamingDynamic> StageStreaming;

	// 그 핸들이 어느 에셋으로 올라간 것인지, 같은 세트 판정에 사용
	TSoftObjectPtr<UWorld> LoadedStageLevel;

	// 현재 캐싱중인 세트
	mutable TWeakObjectPtr<ULevel> CachedStageLevel;

	// 캐싱중인 진형 축, 열마다 하나
	mutable TMap<ENUBattleFormationRow, TWeakObjectPtr<ANUBattleFormationAxis>> CachedAxisByRow;

	// 캐싱중인 진형 자리, 열마다 SlotIndex 오름차순으로 정렬해 보관함
	mutable TMap<ENUBattleFormationRow, TArray<TWeakObjectPtr<ANUBattleFormationSlot>>> CachedSlotsByRow;

	// 캐싱중인 세트 내 카메라
	mutable TWeakObjectPtr<AActor> CachedStageCamera;

	// 캐싱중인 세트 내 전역 조명, 전투 중에만 켜기 위해 들고 있음
	// 세트는 전투가 끝나도 내려가지 않아서 그냥 두면 필드까지 계속 비춤
	mutable TArray<TWeakObjectPtr<ADirectionalLight>> CachedStageDirectionalLights;
	mutable TArray<TWeakObjectPtr<ASkyLight>> CachedStageSkyLights;

	// 캐싱중인 세트 내 안개, 필드 안개를 끄면 이것이 렌더러가 읽는 첫 안개가 됨
	// 전투가 끝날 때 같이 끄지 않으면 필드 안개보다 먼저 등록된 상태가 되어 필드까지 덮음
	mutable TArray<TWeakObjectPtr<AExponentialHeightFog>> CachedStageHeightFogs;

	// 전투 때문에 끈 필드 DirectionalLight
	// 원래 꺼져 있던 조명을 켜지 않기 위해 명단을 들고 있음
	TArray<TWeakObjectPtr<ADirectionalLight>> FieldDirectionalLights;

	// 전투 때문에 끈 필드 ExponentialHeightFog, 같은 이유로 명단을 들고 있음
	TArray<TWeakObjectPtr<AExponentialHeightFog>> FieldHeightFogs;

	// 조명을 맞바꾼 상태인지, 되돌리기를 두 번 불러도 되게 함
	bool bStageLightingApplied = false;
};
